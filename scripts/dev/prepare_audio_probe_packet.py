#!/usr/bin/env python3
"""Prepare fresh Host diagnostic packet and target/rollback plan; never installs."""
import argparse,hashlib,json,os,pathlib,re,shutil,struct,subprocess,tarfile,time,zlib
from package_audio_probe_initrd import ROOT,NATIVE,NEW,need,file_sha
FROZEN=ROOT/'artifacts/local/mpu-sensor-coexistence-package-v1/packet'
USER='/home/cat/cockpit/audio-probe-diagnostic-d1';DEST=USER+'/boot'
CMD_SHA='a5fa6ca38c67b52e6a3f18fe3bc67c1d6c16fe94d477c24fbb97dbcf6cfc2619'
FIT_SHA='21ee94796ab72ae5d6c675f0be2c540f7782d900819ef6da422aaf78ab159bcb'
DT_SHA='33dc67a8a0199d4d0b313e980657982297155f364509552ecc3a92d650c3064e'
def derive_stage(text,image_bytes,initrd_bytes):
 need(hashlib.sha256(text.encode()).hexdigest()==CMD_SHA,'frozen command input')
 text=text.replace('echo MPU_SENSOR_COEXISTENCE_V1 bounded formal sensor','echo AUDIO_PROBE_DIAGNOSTIC_D1 bounded startup errno only')
 for old,new in [('/amp-p029/Image',DEST+'/Image'),('/amp-p029/initrd',DEST+'/initrd'),('/amp-p029/mpu-sensor-coexistence-v1/sensor.dtb',DEST+'/sensor.dtb')]:text=text.replace(old,new)
 guard='if part number mmc 0 rootfs audio_root_part; then true; else echo STOP missing rootfs partition; exit 1; fi\nif test "${audio_root_part}" = "3"; then true; else echo STOP unexpected rootfs partition; exit 1; fi\n'
 text=text.replace('if setenv filesize; then true; else echo STOP clear size Image; exit 1; fi',guard+'if setenv filesize; then true; else echo STOP clear size Image; exit 1; fi',1)
 for line in text.splitlines():
  if ('size mmc ' in line or 'load mmc ' in line) and DEST+'/' in line:text=text.replace(line,line.replace('0:${p029_part}','0:${audio_root_part}'))
 text=text.replace('2930200',f'{image_bytes:x}').replace('64c22e',f'{initrd_bytes:x}')
 text=text.replace('dyndbg="file virtio_rpmsg_bus.c +p"'+chr(39), 'dyndbg="file virtio_rpmsg_bus.c +p" audio_probe=AUDIO_PROBE_DIAGNOSTIC_D1'+chr(39))
 for marker in ['0x40400000','0x4a200000','0x48300000','amp_m0load','amp_test_stage=C','mpu_sensor=MPU_SENSOR_COEXISTENCE_V1','no same-session boot or retry permitted','audio_probe=AUDIO_PROBE_DIAGNOSTIC_D1']:need(marker in text,'lost original/new guard '+marker)
 validate_storage(text)
 return text

def validate_storage(text):
 """Check actual U-Boot argv after named-partition guard substitution."""
 variables={}
 for name,var in re.findall(r'part number mmc 0 (boot|rootfs) ([A-Za-z_][A-Za-z_0-9]*)',text):
  value='2' if name=='boot' else '3'
  need('if test "${'+var+'}" = "'+value+'"; then true; else' in text,'partition exact guard '+name)
  variables[var]=value
 need(variables=={'p029_part':'2','audio_root_part':'3'},'named boot/rootfs guards')
 commands=[]
 for line in text.splitlines():
  match=re.search(r'if (size|load) mmc (\S+) (.*?); then',line)
  if not match:continue
  kind,part,tail=match.groups()
  for var,value in variables.items():part=part.replace('${'+var+'}',value)
  args=tail.split();path=args[0] if kind=='size' else args[1]
  if path.startswith(DEST+'/'):
   need(part=='0:3' and path.rsplit('/',1)[1] in {'Image','initrd','sensor.dtb'},'Linux argv requires rootfs p3')
  else:need(kind=='size' and part=='0:2' and path=='/amp-p029/mpu-sensor-coexistence-v1/amp-signed.itb','FIT size requires frozen boot p2')
  commands.append((kind,part,path))
 need(len(commands)==7 and sum(c[1]=='0:3' for c in commands)==6,'all Linux/FIT size/load argv')
 need(text.count('amp_m0load /amp-p029/mpu-sensor-coexistence-v1/amp-signed.itb 0x48300000')==1,'fixed boot-partition loader ABI/path')
 return commands

def prepare(out):
 deadline=time.monotonic()+60;out=out.resolve();need(out.is_relative_to(ROOT/'artifacts/local') and not out.exists(),'fresh packet output')
 native=json.loads((ROOT/'docs/bringup/mpu6050/AUDIO_PROBE_DIAGNOSTIC_BUILD.json').read_text());init=json.loads((ROOT/'artifacts/local/audio-probe-diagnostic-initrd-v1/manifest.json').read_text())
 need(init['status']=='HOST_PAIRED_INITRD_PASS_NOT_DEPLOYED' and init['release']==NEW,'actual paired initrd')
 sources={'Image':NATIVE/'build/arch/arm64/boot/Image','initrd':ROOT/init['output']['file'],'sensor.dtb':FROZEN/'sensor.dtb','amp-signed.itb':FROZEN/'amp-signed.itb','rk3576_amp_health_test.ko':NATIVE/'health/rk3576_amp_health_test.ko','rk3576_sensor.ko':NATIVE/'sensor/rk3576_sensor.ko','modules.tar':NATIVE/'modules.tar'}
 pins={'Image':native['artifacts']['build/arch/arm64/boot/Image']['sha256'],'initrd':init['output']['sha256'],'sensor.dtb':DT_SHA,'amp-signed.itb':FIT_SHA,'rk3576_amp_health_test.ko':native['artifacts']['health/rk3576_amp_health_test.ko']['sha256'],'rk3576_sensor.ko':native['artifacts']['sensor/rk3576_sensor.ko']['sha256'],'modules.tar':native['artifacts']['modules.tar']['sha256']}
 for name,p in sources.items():need(file_sha(p,deadline)==pins[name],'packet source '+name)
 command=derive_stage((FROZEN/'stage-sensor.cmd').read_text(),sources['Image'].stat().st_size,sources['initrd'].stat().st_size)
 out.mkdir();packet=out/'packet';packet.mkdir()
 for name,p in sources.items():shutil.copy2(p,packet/name)
 (packet/'stage-audio.cmd').write_text(command)
 env=dict(os.environ,SOURCE_DATE_EPOCH='1791244800');argv=['/usr/bin/mkimage','-A','arm','-O','linux','-T','script','-C','none','-n','AUDIO_PROBE_DIAGNOSTIC_D1','-d',str(packet/'stage-audio.cmd'),str(packet/'stage-audio.scr')]
 result=subprocess.run(argv,capture_output=True,timeout=10,env=env);need(result.returncode==0 and len(result.stdout)+len(result.stderr)<=16384,'mkimage result');(out/'mkimage.log').write_bytes(result.stdout+result.stderr)
 scr=(packet/'stage-audio.scr').read_bytes();header=list(struct.unpack('>7I4B32s',scr[:64]));need(header[0]==0x27051956 and header[3]==len(scr)-64,'script header length')
 saved=header[1];header[1]=0;need(zlib.crc32(struct.pack('>7I4B32s',*header))&0xffffffff==saved,'header CRC');need(zlib.crc32(scr[64:])&0xffffffff==header[6],'data CRC');need(header[7:11]==[5,2,6,0],'script type/arch/compression');need(scr[64:72]==struct.pack('>II',len(command.encode()),0) and scr[72:]==command.encode(),'script exact body')
 files={}
 for p in packet.iterdir():
  target='/boot/amp-p029/mpu-sensor-coexistence-v1/amp-signed.itb' if p.name=='amp-signed.itb' else DEST+'/'+p.name if p.name in ['Image','initrd','sensor.dtb','amp-signed.itb','stage-audio.cmd','stage-audio.scr'] else USER+'/'+p.name
  files[p.name]={'source':str(p.relative_to(ROOT)),'bytes':p.stat().st_size,'target_sha256':file_sha(p,deadline),'target_path':target,'install':p.name!='amp-signed.itb','dependency_evidence':'artifacts/local/mpu-root-review/audio-probe-frozen-fit-dependency.log' if p.name=='amp-signed.itb' else None,'current_target_sha256':FIT_SHA if p.name=='amp-signed.itb' else 'ABSENT_PARENT_SNAPSHOT: installation must lstat recheck/reject overwrite'}
 members=[]
 with tarfile.open(packet/'modules.tar') as archive:
  for p in archive:
   need(time.monotonic()<deadline,'module inventory deadline')
   need(p.name=='lib/modules/'+NEW or p.name.startswith('lib/modules/'+NEW+'/'),'module release path');need(p.isdir() or p.isfile(),'module archive link/special')
   digest=None
   if p.isfile():
    h=hashlib.sha256()
    with archive.extractfile(p) as stream:
     while True:
      need(time.monotonic()<deadline,'module hash deadline');block=stream.read(1048576)
      if not block:break
      h.update(block)
    digest=h.hexdigest()
   members.append({'archive_path':p.name,'target_path':'/'+p.name.replace('lib/modules/','usr/lib/modules/',1),'type':'directory' if p.isdir() else 'file','bytes':p.size,'mode':p.mode,'uid':p.uid,'gid':p.gid,'sha256':digest,'current_target_sha256':'ABSENT_PARENT_SNAPSHOT_RECHECK_REQUIRED'})
 (out/'root-modules-targets.json').write_text(json.dumps(members,indent=2)+'\n')
 manifest={'status':'HOST_PACKAGED_REVIEWED_NOT_DEPLOYED','deployable':False,'release':NEW,'board_runtime':'NOT_RUN','audio_cause':'UNKNOWN','files':files,'root_modules':{'target_directory':'/usr/lib/modules/'+NEW,'logical_module_directory':'/lib/modules/'+NEW,'canonical_parent_evidence':'artifacts/local/mpu-root-review/audio-probe-install-parent-inventory.log','archive':'modules.tar','members':len(members),'target_inventory_file':str((out/'root-modules-targets.json').relative_to(ROOT)),'target_inventory_sha256':file_sha(out/'root-modules-targets.json',deadline),'installation':'sudo-root: tar --strip-components=2 -C /usr/lib/modules -xpf pinned modules.tar; compare same mapping; exact newrelease absent'},'storage':{'boot_partition_write':False,'filesystem':'ext4','mmc_device_partition':'0:3','feature_evidence':'artifacts/local/mpu-root-review/audio-probe-rootfs-features-readonly.log','runtime_read':'NOT_RUN','old_packet_v1':'STOPPED_NOT_INSTALLABLE_BOOT_SPACE_INSUFFICIENT','old_packet_v2':'STOPPED_WRONG_INTERNAL_LINUX_PARTITION','partition_identity_evidence':'artifacts/local/mpu-root-review/audio-probe-partition-identities.log','fit_dependency_partition':'0:2' },'fresh_target_inventory_evidence':'artifacts/local/mpu-root-review/audio-probe-fresh-path-inventory.log','initrd':init,'tools':{'mkimage_sha256':file_sha(pathlib.Path('/usr/bin/mkimage'),deadline),'mkimage_version':subprocess.check_output(['/usr/bin/mkimage','-V'],text=True,timeout=5).strip(),'mkimage_command':argv,'mkimage_exit':result.returncode,'SOURCE_DATE_EPOCH':env['SOURCE_DATE_EPOCH']},'uboot':{'load_command':'load mmc 0:3 0x4c000000 /home/cat/cockpit/audio-probe-diagnostic-d1/boot/stage-audio.scr','expected_scr_bytes':len(scr),'expected_scr_filesize_hex':f'{len(scr):x}','inspect_command':'printenv fileaddr filesize','source_command':'source 0x4c000000','manual_once_only':True},'window':{'total_seconds':600,'source_seconds':180,'p3_read_runtime':'NOT_RUN','normal_shutdown_before_rootfs_read_required':True,'uart_bytes_each':262144,'no_ko_no_hello_no_mpu_no_qt':True,'same_combination_retry':False},'rollback':'normal root shutdown/UART Power down/default user cold restore; verify default/no task modules/node/cards; only exact fresh boot/rootrelease/userdir targets remove after identity/manifest/nooccupants review'}
 (out/'deployment-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');return manifest
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=pathlib.Path,required=True);args=ap.parse_args();print(json.dumps(prepare(args.output),indent=2))
