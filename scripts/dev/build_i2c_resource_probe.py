#!/usr/bin/env python3
"""Host-only fresh SI_HEALTH-derived resource probe; never deploy or access board."""
import argparse,hashlib,json,os,pathlib,shutil,subprocess,sys,struct,zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
AMP=pathlib.Path('/home/ywx/rk3576-work/worktrees/rk3576-amp-platform/project')
SYSTEM=pathlib.Path('/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-system')
SI=SYSTEM/'artifacts/local/system-board-validation-20261004'
BASE=SI/'persistent-host-v2/source'
KERNEL=AMP/'artifacts/local/p023-kernel-source/kernel-521833e2d28decbd6473d5717f1f96cc4108e208'
PREPARED=SI/'health-ko-v2/prepared-kernel'
TOOLCHAIN=pathlib.Path('/home/ywx/.local/toolchains/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi/bin')
TOOLS=AMP/'artifacts/local/p028-build-final-v4/tools'
CONTROL=AMP/'artifacts/local/p030-final-fdt-package-v1/fdt.bin'
KEYDIR=AMP/'artifacts/local/p029-signed-v8/private'
DEST='/boot/amp-p029/i2c-resource-probe-v1'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args,cwd,log,env=None):
 with log.open('wb') as f:subprocess.run([str(x) for x in args],cwd=cwd,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180,check=True)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=pathlib.Path,required=True);a=ap.parse_args();out=a.output.resolve()
 if not out.is_relative_to(ROOT/'artifacts/local') or out.exists():raise SystemExit('new local artifact output required')
 inputs=json.loads((ROOT/'patches/mpu6050/resource-probe-inputs.json').read_text())
 kernel_inputs=json.loads((ROOT/'patches/mpu6050/kernel-resource-inputs.json').read_text())
 for name,want in kernel_inputs['files'].items():
  if sha(KERNEL/name)!=want:raise SystemExit('kernel source mismatch: '+name)
 for name,want in inputs['files'].items():
  if sha(BASE/name)!=want:raise SystemExit('SI source identity mismatch: '+name)
 frozen=[SI/'persistent-host-v2/rtthread.bin',SI/'persistent-package-v1/packet/amp-signed.itb',SI/'health-ko-v2/module/rk3576_amp_health_test.ko',PREPARED/'.config',PREPARED/'Module.symvers',CONTROL]
 before={str(p):sha(p) for p in frozen}
 assert before[str(CONTROL)]=='43164981efd8869432e42fab2411d99941d17476700f3fc03ab76948621049ce'
 assert before[str(frozen[1])]=='3be5eea15bdbb44fece3a06bec9fd80e026f27f17fd443edd7466ac67265fe28'
 assert before[str(frozen[2])]=='57b1d830d6687f4f891bbabc3c9694c976c79ab2fcea3d1671a6289d95dea700'
 assert (PREPARED/'include/config/kernel.release').read_text().strip()=='6.1.99-rk3576-m0echo-p026'
 out.mkdir();tree=out/'rtos'
 shutil.copytree(BASE,tree,symlinks=True,ignore=shutil.ignore_patterns('.git','build','*.o','*.elf','*.bin','*.map','.sconsign*','*.pyc','__pycache__'))
 for p in tree.rglob('*'):
  if p.is_symlink() and not p.resolve().is_relative_to(tree):raise SystemExit('escaping SI SDK symlink: '+str(p))
 bsp=tree/'bsp/rockchip/rk3576-mcu'
 for name in ('amp_echo.c','resource_probe.h'):shutil.copyfile(ROOT/'rtos/resource_probe'/name,bsp/'applications'/name)
 config=(bsp/'.config').read_text();assert '# CONFIG_RT_USING_I2C is not set' in config
 (bsp/'.config').write_text(config+'\nCONFIG_I2C_RESOURCE_PROBE_V1=y\n')
 with (bsp/'Kconfig').open('a') as f:f.write('\nconfig I2C_RESOURCE_PROBE_V1\n    bool "Read-only resource probe diagnostic"\n    default n\n    depends on !RT_USING_I2C\n')
 env=dict(os.environ,RTT_ROOT=str(tree),RTT_CC='gcc',RTT_EXEC_PATH=str(TOOLCHAIN));env.pop('BSP_ROOT',None);env.pop('PKGS_ROOT',None)
 run(['scons','--useconfig=.config'],bsp,out/'config.log',env);run(['scons','-j4'],bsp,out/'firmware-build.log',env)
 h=(bsp/'rtconfig.h').read_text();assert '#define I2C_RESOURCE_PROBE_V1' in h and '#define RT_USING_I2C\n' not in h
 for tool,args in [('size',[]),('nm',[]),('readelf',['-l','-S'])]:run([TOOLCHAIN/('arm-none-eabi-'+tool),*args,bsp/'rtthread.elf'],bsp,out/('elf-'+tool+'.txt'))
 prepared=out/'prepared-kernel';prepared.mkdir()
 for name in ('.config','Makefile','Module.symvers'):shutil.copyfile(PREPARED/name,prepared/name)
 for name in ('include','scripts','arch/arm64/include/generated'):shutil.copytree(PREPARED/name,prepared/name)
 module=out/'module';shutil.copytree(ROOT/'scripts/board/mpu6050/resource_probe_linux',module)
 deps=AMP/'artifacts/local/p023-standard-tools/root'
 env=dict(os.environ,PATH=str(deps/'usr/bin')+':'+os.environ['PATH'],BISON_PKGDATADIR=str(deps/'usr/share/bison'),M4=str(deps/'usr/bin/m4'),CPATH=str(deps/'usr/include'),LIBRARY_PATH=str(deps/'usr/lib/x86_64-linux-gnu'),LD_LIBRARY_PATH=str(deps/'usr/lib/x86_64-linux-gnu'))
 run(['make','-C',KERNEL,'O='+str(prepared),'M='+str(module),'ARCH=arm64','CROSS_COMPILE=aarch64-linux-gnu-','modules'],ROOT,out/'ko-build.log',env)
 ko=module/'rk3576_i2c_resource_probe.ko';run(['modinfo',ko],ROOT,out/'modinfo.txt')
 assert '6.1.99-rk3576-m0echo-p026 SMP mod_unload aarch64' in (out/'modinfo.txt').read_text()
 for name,want in {'mkimage':'988a2b46efb8dc21710b82a0b856496dbc13fa2b0fc194f3f947f1ba35a61f83','fit_check_sign':'3175a54383c7fc8e65b4a57a1dbcda3251f032906244fbce48cada5382b60ef8'}.items():assert sha(TOOLS/name)==want
 signed=out/'signed';signed.mkdir();shutil.copyfile(bsp/'rtthread.bin',signed/'rttmcu.bin')
 its=(AMP/'artifacts/local/p030-m0-initdiag-v1/vendor-fit-v2/amp-signed.its').read_text().replace('P029 development signed cold BUS M0','I2C_RESOURCE_PROBE_V1 read-only diagnostic')
 binary=(bsp/'rtthread.bin').read_bytes();total=0x1000+((len(binary)+511)&~511)
 import re
 its=re.sub(r'totalsize = <0x[0-9a-f]+>;',f'totalsize = <0x{total:x}>;',its,count=1)
 (signed/'amp-signed.its').write_text(its)
 assert (KEYDIR/'p029dev.key').stat().st_mode & 0o077==0
 # Signer alone opens existing private key. No key creation/copy/read/printing.
 run([TOOLS/'mkimage','-f','amp-signed.its','-E','-p','0x1000','-k',KEYDIR,'-r','amp-signed.itb'],signed,out/'sign.log')
 run([TOOLS/'fit_check_sign','-f',signed/'amp-signed.itb','-k',CONTROL,'-s'],signed,out/'verify.log')
 assert 'Signature check OK' in (out/'verify.log').read_text()
 sys.path.insert(0,str(ROOT/'scripts/amp'));from validate_host_proposal import fdt
 fit=(signed/'amp-signed.itb').read_bytes();nodes=fdt(fit);props=nodes['/images/mcu'];prior=fdt((frozen[1]).read_bytes())
 assert {k:v for k,v in props.items() if k!='data-size'}=={k:v for k,v in prior['/images/mcu'].items() if k!='data-size'}
 assert int.from_bytes(props['data-position'],'big')==0x1000 and int.from_bytes(props['data-size'],'big')==len(binary)
 assert int.from_bytes(nodes['/']['totalsize'],'big')==len(fit) and fit[0x1000:0x1000+len(binary)]==binary
 assert nodes['/images/mcu/hash']['value'].hex()==hashlib.sha256(binary).hexdigest()
 assert set(nodes['/configurations/conf/signature']['hashed-nodes'].rstrip(b'\0').decode().split('\0'))=={'/','/configurations','/configurations/conf','/images/mcu','/images/mcu/hash'}
 packet=out/'packet';packet.mkdir()
 dt=ROOT/'artifacts/local/mpu-i2c9-owner-dt-final/mpu-i2c9-candidate.dtb'
 assert sha(dt)=='33dc67a8a0199d4d0b313e980657982297155f364509552ecc3a92d650c3064e'
 shutil.copyfile(dt,packet/'resource-probe.dtb');shutil.copyfile(signed/'amp-signed.itb',packet/'amp-signed.itb');shutil.copyfile(ko,packet/ko.name)
 basepacket=SI/'persistent-package-v1/packet'
 cmd=(basepacket/'stage-health.cmd').read_text().replace('SI HEALTH V1 TEST_ONLY persistent echo','I2C_RESOURCE_PROBE_V1 read-only diagnostic').replace('/amp-p029/system-health-v1',DEST[5:]).replace('amp_health_test=SI_HEALTH_V1','i2c_resource_probe=I2C_RESOURCE_PROBE_V1')
 # Replace all DT length/path references, leaving Image/initrd/transport/entry unchanged.
 cmd=cmd.replace('/amp-p029/stage-C.dtb',DEST[5:]+'/resource-probe.dtb').replace('0x4b455',hex(dt.stat().st_size)).replace(' 4b455;',f' {dt.stat().st_size:x};').replace('"0x20000"',f'"0x{len(fit):x}"')
 assert cmd.count('amp_m0load '+DEST[5:]+'/amp-signed.itb 0x48300000')==1
 assert 'saveenv' not in cmd and 'reboot' not in cmd
 raw=cmd.encode();data=struct.pack('>II',len(raw),0)+raw;hdr=bytearray((basepacket/'stage-health.scr').read_bytes()[:64]);struct.pack_into('>I',hdr,12,len(data));struct.pack_into('>I',hdr,24,zlib.crc32(data));hdr[4:8]=bytes(4);struct.pack_into('>I',hdr,4,zlib.crc32(hdr))
 (packet/'stage-resource-probe.cmd').write_bytes(raw);(packet/'stage-resource-probe.scr').write_bytes(bytes(hdr)+data)
 from p029_prepare_signed_stage_b import script_text
 assert script_text((packet/'stage-resource-probe.scr').read_bytes())==raw
 report={'id':'I2C_RESOURCE_PROBE_V1','role':'DIAGNOSTIC_NOT_SENSOR','board_deployed':False,'destination':DEST,'kernel':'6.1.99-rk3576-m0echo-p026','firmware_source':inputs,'rtos_window_ms':120000,'rtos_window_origin':'NS endpoint created after link-up','rtos_request_limit':128,'linux_window_ms':90000,'linux_window_origin':'RPMsg probe after CCF owner acquisition','reply_deadline_ms':3000,'m0_probe_limit':1,'m0_mmio_writes':0,'i2c_transactions':0,'frozen_inputs_unchanged':before,'files':{x.name:{'bytes':x.stat().st_size,'sha256':sha(x)} for x in packet.iterdir()},'native_files':{x:{'bytes':(bsp/x).stat().st_size,'sha256':sha(bsp/x)} for x in ['.config','rtconfig.h','rtthread.elf','rtthread.bin','rtthread.map']},'reuse':{'Image':'8c82342d740c9aefe0a4f0be8d84c436d14ebf7edb5f462e14631ce355cf3e8d','initrd':'c000c9ab2126b0a2e64581514077a7ca79aa80660989ebfbcb3a200366a1346d'},'control_dt_sha256':sha(CONTROL),'prepared_config_sha256':sha(PREPARED/'.config'),'prepared_symvers_sha256':sha(PREPARED/'Module.symvers')}
 assert before=={str(x):sha(x) for x in frozen},'frozen input modified'
 # Finalize six payloads plus manifest/checksum: exact eight-file packet.
 protected=json.loads((ROOT/'patches/mpu6050/resource-probe-protected-files.json').read_text())['files']
 owner=fdt(dt.read_bytes())['/mcu-amp']
 props={k:owner[k].hex() for k in ['clocks','assigned-clocks','assigned-clock-parents','assigned-clock-rates','pinctrl-0']}
 expected={x.name:sha(x) for x in packet.iterdir()}
 pre=(ROOT/'scripts/board/mpu6050/preflight_resource_probe.py.in').read_text().replace('__KO_SHA__',sha(ko)).replace('__KO_BYTES__',str(ko.stat().st_size)).replace('__KO_READ_LIMIT__',str(ko.stat().st_size+1)).replace('__OWNER_PROPERTIES__',repr(props)).replace('__PACKET_PROPERTIES__',repr(expected)).replace('__PROTECTED_FILES__',repr(protected))
 compile(pre,'preflight-resource-probe.py','exec')
 (packet/'preflight-resource-probe.py').write_text(pre)
 report['protected_boot_files']=protected
 report['files']={x.name:{'bytes':x.stat().st_size,'sha256':sha(x)} for x in packet.iterdir()}
 (packet/'RESOURCE_PROBE.json').write_text(json.dumps(report,sort_keys=True,indent=2)+'\n')
 (packet/'SHA256SUMS').write_text(''.join(sha(x)+'  '+x.name+'\n' for x in sorted(packet.iterdir()) if x.name!='SHA256SUMS'))
 installer=(ROOT/'scripts/board/mpu6050/install_resource_probe.py').read_text().replace('__PIN_MANIFEST__',sha(packet/'RESOURCE_PROBE.json'))
 (out/'install-resource-probe.py').write_text(installer)
 import importlib.util
 spec=importlib.util.spec_from_file_location('probe_installer',out/'install-resource-probe.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);mod.validate_packet(packet)
 report['package_files']={x.name:{'bytes':x.stat().st_size,'sha256':sha(x)} for x in packet.iterdir()}
 report['installer']={'bytes':(out/'install-resource-probe.py').stat().st_size,'sha256':sha(out/'install-resource-probe.py')}
 report['source_files']={str(x.relative_to(ROOT)):sha(x) for x in [ROOT/'scripts/dev/build_i2c_resource_probe.py',*sorted((ROOT/'rtos/resource_probe').iterdir()),*sorted((ROOT/'scripts/board/mpu6050/resource_probe_linux').iterdir()),ROOT/'scripts/board/mpu6050/preflight_resource_probe.py.in',ROOT/'scripts/board/mpu6050/install_resource_probe.py'] if x.is_file()}
 (out/'build-result.json').write_text(json.dumps(report,indent=2)+'\n');print(out/'build-result.json')
if __name__=='__main__':main()
