#!/usr/bin/env python3
"""Host-only fresh errno diagnostic kernel/module build; never installs to a host."""
import argparse,hashlib,json,os,pathlib,selectors,shutil,signal,subprocess,time
ROOT=pathlib.Path(__file__).resolve().parents[2]
AMP=pathlib.Path('/home/ywx/rk3576-work/worktrees/rk3576-amp-platform/project')
BASE=AMP/'artifacts/local/p023-kernel-source/kernel-521833e2d28decbd6473d5717f1f96cc4108e208'
PATCHROOT=ROOT/'patches/mpu6050/audio-probe-diagnostic-v1'
RELEASE='6.1.99-rk3576-audioprobe-d1'
LOG_CAP=2*1024*1024;TOTAL_LOG_CAP=8*1024*1024
BUILD_SECONDS=1800;WHOLE_SECONDS=2400;MIN_FREE=16*1024**3

def need(ok,reason):
 if not ok:raise ValueError(reason)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def fresh_output(out):
 out=out.resolve();need(out.is_relative_to(ROOT/'artifacts/local') and not out.exists(),'fresh task-local output required')
 need(shutil.disk_usage(out.parent).free>=MIN_FREE,'need 16GiB free disk')
 return out

def config_changes(before,after):
 def parse(raw):
  rows={}
  for line in raw.splitlines():
   if line.startswith('CONFIG_'):key,_,value=line.partition('=');rows[key]=value
   elif line.startswith('# CONFIG_') and line.endswith(' is not set'):rows[line[2:-11]]='n'
  return rows
 a,b=parse(before),parse(after);diff={k:(a.get(k),b.get(k)) for k in set(a)|set(b) if a.get(k)!=b.get(k)}
 need(set(diff)<={'CONFIG_LOCALVERSION','CONFIG_LOCALVERSION_AUTO'},'unexpected Kconfig changes '+str(diff))
 need(b.get('CONFIG_LOCALVERSION')=='"-rk3576-audioprobe-d1"' and b.get('CONFIG_LOCALVERSION_AUTO')=='n','independent release config')
 return diff

def verify_inputs():
 pins=json.loads((PATCHROOT/'SOURCE.json').read_text());codec=BASE/pins['source_path']
 need(sha(codec)==pins['source_sha256'],'codec base source hash')
 need(sha(PATCHROOT/'0001-es8323-initial-read-errno.patch')==pins['patch_sha256'],'patch hash')
 for item in pins['inputs'].values():need(sha(pathlib.Path(item['path']))==item['sha256'],'input hash '+item['path'])
 for name,digest in pins['sensor_inputs'].items():need(sha(ROOT/'apps/rpmsg_srv/kernel'/name)==digest,'sensor source hash '+name)
 resources=json.loads((ROOT/'patches/mpu6050/kernel-resource-inputs.json').read_text())
 for name,digest in resources['files'].items():need(sha(BASE/name)==digest,'kernel resource source hash '+name)
 return pins

class Runner:
 def __init__(self,out,env):self.out=out;self.env=env;self.start=time.monotonic();self.log_bytes=0;self.commands=[]
 def run(self,args,name,seconds):
  deadline=min(self.start+WHOLE_SECONDS,time.monotonic()+seconds);need(deadline>time.monotonic(),'whole build deadline')
  self.commands.append({'argv':[str(x) for x in args],'log':name,'cap_seconds':seconds})
  p=subprocess.Popen([str(x) for x in args],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=self.env,start_new_session=True)
  sel=selectors.DefaultSelector();sel.register(p.stdout,selectors.EVENT_READ);count=0
  try:
   with (self.out/name).open('wb') as log:
    while sel.get_map():
     need(time.monotonic()<deadline,'command deadline '+name)
     for key,_ in sel.select(.1):
      raw=os.read(key.fileobj.fileno(),65536)
      if not raw:sel.unregister(key.fileobj);continue
      count+=len(raw);self.log_bytes+=len(raw)
      need(count<=LOG_CAP and self.log_bytes<=TOTAL_LOG_CAP,'bounded build log cap '+name);log.write(raw)
    rc=p.wait(timeout=max(.01,deadline-time.monotonic()));need(rc==0,'command rc '+str(rc)+' '+name)
  finally:
   sel.close()
   if p.poll() is None:
    os.killpg(p.pid,signal.SIGTERM)
    try:p.wait(timeout=2)
    except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait(timeout=2)
   p.stdout.close()

 def make(self,src,out,*targets,name,seconds=120):
  self.run(['make','-C',src,'O='+str(out),'ARCH=arm64','CROSS_COMPILE=aarch64-linux-gnu-',*targets],name,seconds)

def copy_source(src,dst,deadline):
 # Traverse bounded source before copy; reject escaping symlinks/special files.
 files=[];total=0
 for p in src.rglob('*'):
  need(time.monotonic()<deadline,'source inventory deadline')
  if p.is_symlink():
   need(p.resolve().is_relative_to(src),'source symlink escape');need(p.is_file(),'source directory symlink unsupported')
  if p.is_file():
   total+=p.stat().st_size;files.append(p);need(total<=3*1024**3 and len(files)<=150000,'source copy cap')
  else:need(p.is_dir(),'source special file')
 dst.mkdir()
 for p in files:
  need(time.monotonic()<deadline,'source copy deadline')
  target=dst/p.relative_to(src);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target)
 return {'files':len(files),'bytes':total}

def build(out,check_only=False):
 pins=verify_inputs();out=fresh_output(out);out.mkdir();deps=AMP/'artifacts/local/p023-standard-tools/root'
 env=dict(os.environ,PATH=str(deps/'usr/bin')+':'+os.environ['PATH'],BISON_PKGDATADIR=str(deps/'usr/share/bison'),M4=str(deps/'usr/bin/m4'),CPATH=str(deps/'usr/include'),LIBRARY_PATH=str(deps/'usr/lib/x86_64-linux-gnu'),LD_LIBRARY_PATH=str(deps/'usr/lib/x86_64-linux-gnu'))
 # Prevent external kernel/Kbuild variables escaping this fresh output.
 for key in ['KBUILD_OUTPUT','KBUILD_SRC','KCONFIG_CONFIG','LOCALVERSION','INSTALL_MOD_PATH','INSTALL_HDR_PATH','M','O']:env.pop(key,None)
 run=Runner(out,env);src=out/'src';obj=out/'build';stage=out/'stage'
 result={'status':'FAILED_OR_INCOMPLETE','deployable':False,'board_operations':False,'release':RELEASE,'pins':pins,'limits':{'whole_seconds':WHOLE_SECONDS,'native_build_seconds':BUILD_SECONDS,'source_copy_seconds':300,'log_each_bytes':LOG_CAP,'log_total_bytes':TOTAL_LOG_CAP,'min_free_bytes':MIN_FREE}}
 try:
  # Check-only never copies full SDK nor starts make. Exact codec patch is verified in a tiny scratch tree.
  if check_only:
   target=out/'patch-check'/pins['source_path'];target.parent.mkdir(parents=True);shutil.copy2(BASE/pins['source_path'],target)
   run.run(['patch','--dry-run','--fuzz=0','-p1','-d',out/'patch-check','-i',PATCHROOT/'0001-es8323-initial-read-errno.patch'],'patch-dryrun.log',10)
   run.run(['patch','--fuzz=0','-p1','-d',out/'patch-check','-i',PATCHROOT/'0001-es8323-initial-read-errno.patch'],'patch-apply.log',10)
   need(sha(target)==pins['patched_source_sha256'],'patched source hash');result['status']='HOST_INPUT_PATCH_CHECK_ONLY_PASS';return result
  result['source_copy']=copy_source(BASE,src,run.start+300);obj.mkdir()
  run.run(['patch','--dry-run','--fuzz=0','-p1','-d',src,'-i',PATCHROOT/'0001-es8323-initial-read-errno.patch'],'patch-dryrun.log',10)
  run.run(['patch','--fuzz=0','-p1','-d',src,'-i',PATCHROOT/'0001-es8323-initial-read-errno.patch'],'patch-apply.log',10)
  need(sha(src/pins['source_path'])==pins['patched_source_sha256'],'patched source hash')
  config=pathlib.Path(pins['inputs']['config']['path']);shutil.copy2(config,obj/'.config');shutil.copy2(config,out/'input.config')
  run.run([src/'scripts/config','--file',obj/'.config','--set-str','LOCALVERSION','-rk3576-audioprobe-d1','--disable','LOCALVERSION_AUTO'],'config-edit.log',10)
  run.make(src,obj,'olddefconfig',name='config.log');result['config_changes']=config_changes(config.read_text(),(obj/'.config').read_text())
  run.run(['aarch64-linux-gnu-gcc','--version'],'compiler.log',10)
  run.make(src,obj,'-j4','Image','modules','rockchip/rk3576-lubancat-3-v2.dtb',name='kernel-build.log',seconds=BUILD_SECONDS)
  need((obj/'include/config/kernel.release').read_text().strip()==RELEASE,'built independent kernel release')
  need('# CONFIG_MODVERSIONS is not set' in (obj/'.config').read_text(),'expected pinned no-MODVERSIONS config')
  for kind,items in [('health',['health_makefile','health_source','health_header']),('sensor',None)]:
   module=out/kind;module.mkdir()
   if items:
    for key in items:
     p=pathlib.Path(pins['inputs'][key]['path']);shutil.copy2(p,module/p.name)
   else:
    for name in pins['sensor_inputs']:shutil.copy2(ROOT/'apps/rpmsg_srv/kernel'/name,module/name)
   run.make(src,obj,'M='+str(module),'modules',name=kind+'-build.log',seconds=180)
   ko=module/('rk3576_amp_health_test.ko' if kind=='health' else 'rk3576_sensor.ko')
   run.run(['modinfo',ko],kind+'-modinfo.log',10);need(RELEASE+' SMP mod_unload aarch64' in (out/(kind+'-modinfo.log')).read_text(),'KO release '+kind)
   run.run(['aarch64-linux-gnu-nm','-u',ko],kind+'-imports.log',10)
   symbols={row.split()[1] for row in (obj/'Module.symvers').read_text().splitlines()}
   for line in (out/(kind+'-imports.log')).read_text().splitlines():need(line.split()[-1] in symbols,'undefined KO import '+line)
  run.make(src,obj,'INSTALL_MOD_PATH='+str(stage),'DEPMOD=true','modules_install',name='modules-install.log',seconds=180)
  run.run(['depmod','-b',stage,'-F',obj/'System.map',RELEASE],'depmod.log',60)
  # Strip only known generated host build/source links from our fresh staging tree.
  for name in ['build','source']:
   link=stage/'lib/modules'/RELEASE/name
   if link.is_symlink():link.unlink()
   else:need(not link.exists(),'unexpected staged '+name)
  run.run(['tar','--sort=name','--mtime=2026-10-05 00:00:00Z','--owner=0','--group=0','--numeric-owner','-C',stage,'-cf',out/'modules.tar','lib/modules/'+RELEASE],'modules-pack.log',120)
  headers=out/'matching-headers';headers.mkdir()
  for name in ['Makefile','.config','Module.symvers','System.map']:shutil.copy2(obj/name,headers/name)
  for name in ['include','scripts','arch/arm64/include/generated']:shutil.copytree(obj/name,headers/name,symlinks=True)
  # Source include/arch headers remain separately in src; this is a matching generated Kbuild bundle, not a distro package.
  header_files={str(p.relative_to(headers)):{'bytes':p.stat().st_size,'sha256':sha(p)} for p in headers.rglob('*') if p.is_file()}
  (out/'matching-headers-files.json').write_text(json.dumps(header_files,sort_keys=True)+'\n')
  result['headers_kind']='matching generated Kbuild bundle + retained exact patched src';result['initrd_status']='NOT_BUILT_NOT_DEPLOYABLE: root must review independent initrd packaging before any board use'
  artifacts=[obj/'arch/arm64/boot/Image',obj/'arch/arm64/boot/dts/rockchip/rk3576-lubancat-3-v2.dtb',obj/'.config',obj/'Module.symvers',obj/'System.map',out/'health/rk3576_amp_health_test.ko',out/'sensor/rk3576_sensor.ko',out/'modules.tar',out/'matching-headers-files.json']
  result['artifacts']={str(p.relative_to(out)):{'bytes':p.stat().st_size,'sha256':sha(p)} for p in artifacts};result['status']='HOST_NATIVE_BUILT_NOT_DEPLOYED'
  return result
 finally:
  result['commands']=run.commands;result['elapsed_seconds']=time.monotonic()-run.start;result['log_bytes']=run.log_bytes
  (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=pathlib.Path,required=True);ap.add_argument('--check-only',action='store_true');args=ap.parse_args();print(json.dumps(build(args.output,args.check_only),indent=2))
