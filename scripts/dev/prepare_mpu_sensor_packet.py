#!/usr/bin/env python3
"""Host-only exact formal T1-T4 package; never installs/starts board resources."""
import argparse,hashlib,importlib.util,json,pathlib,shutil,struct,sys,zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
SI=pathlib.Path('/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-system/artifacts/local/system-board-validation-20261004')
DEST='/boot/amp-p029/mpu-sensor-v1'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def entry(p):return {'bytes':p.stat().st_size,'sha256':sha(p)}
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--application-manifest',type=pathlib.Path,required=True);ap.add_argument('--output',type=pathlib.Path,required=True);a=ap.parse_args();out=a.output.resolve()
 assert out.is_relative_to(ROOT/'artifacts/local') and not out.exists()
 app=json.loads(a.application_manifest.read_text());assert app['hardware_access'] is False and app['boot_ko_changes'] is False
 print('Application manifest:',app)
 appentry=app['files']['apps/cockpit_ui/sensor_stream_probe']
 native=ROOT/'artifacts/local/mpu-sensor-service-native-v7';fm=json.loads((native/'manifest.json').read_text())
 fit=native/'signed/amp-signed.itb';assert entry(fit)==fm['files']['signed/amp-signed.itb'] and fm['bounded_uart_trace']['enabled']
 ko=ROOT/'artifacts/local/sensor-ko-host-v4/module/rk3576_sensor.ko'
 if not ko.exists():ko=ROOT/'artifacts/local/sensor-ko-host-v4/rk3576_sensor.ko'
 assert sha(ko)=='063175ef8c5a12855192f790b64664f2d14fe439415be44ab383479c8ccea69a'
 dt=ROOT/'artifacts/local/mpu-i2c9-owner-dt-final/mpu-i2c9-candidate.dtb';assert sha(dt)=='33dc67a8a0199d4d0b313e980657982297155f364509552ecc3a92d650c3064e'
 health=SI/'health-ko-v2/module/rk3576_amp_health_test.ko'
 assert sha(health)=='57b1d830d6687f4f891bbabc3c9694c976c79ab2fcea3d1671a6289d95dea700'
 packet=out/'packet';packet.mkdir(parents=True)
 for src,name in [(fit,'amp-signed.itb'),(dt,'sensor.dtb'),(ko,ko.name),(health,health.name)]:shutil.copyfile(src,packet/name)
 old=SI/'persistent-package-v1/packet';cmd=(old/'stage-health.cmd').read_text().replace('SI HEALTH V1 TEST_ONLY persistent echo','MPU_SENSOR_V1 bounded formal sensor').replace('/amp-p029/system-health-v1',DEST[5:]).replace('amp_health_test=SI_HEALTH_V1','mpu_sensor=MPU_SENSOR_V1')
 cmd=cmd.replace('/amp-p029/stage-C.dtb',DEST[5:]+'/sensor.dtb').replace('0x4b455',hex(dt.stat().st_size)).replace(' 4b455;',f' {dt.stat().st_size:x};').replace('"0x20000"',f'"0x{fit.stat().st_size:x}"')
 assert cmd.count('amp_m0load '+DEST[5:]+'/amp-signed.itb 0x48300000')==1 and 'saveenv' not in cmd and 'reboot' not in cmd
 raw=cmd.encode();data=struct.pack('>II',len(raw),0)+raw;hdr=bytearray((old/'stage-health.scr').read_bytes()[:64]);struct.pack_into('>I',hdr,12,len(data));struct.pack_into('>I',hdr,24,zlib.crc32(data));hdr[4:8]=bytes(4);struct.pack_into('>I',hdr,4,zlib.crc32(hdr))
 (packet/'stage-sensor.cmd').write_bytes(raw);(packet/'stage-sensor.scr').write_bytes(bytes(hdr)+data)
 sys.path.insert(0,str(ROOT/'scripts/amp'));from validate_host_proposal import fdt
 from p029_prepare_signed_stage_b import script_text
 assert script_text((packet/'stage-sensor.scr').read_bytes())==raw
 owner=fdt(dt.read_bytes())['/mcu-amp'];props={k:owner[k].hex() for k in ['clocks','assigned-clocks','assigned-clock-parents','assigned-clock-rates','pinctrl-0']}
 protected=json.loads((ROOT/'patches/mpu6050/resource-probe-protected-files.json').read_text())['files']
 pre=(ROOT/'scripts/board/mpu6050/preflight_sensor.py.in').read_text().replace('__KO_SHA__',sha(ko)).replace('__KO_BYTES__',str(ko.stat().st_size)).replace('__KO_READ_LIMIT__',str(ko.stat().st_size+1)).replace('__OWNER_PROPERTIES__',repr(props)).replace('__PACKET_PROPERTIES__',repr({p.name:sha(p) for p in packet.iterdir()})).replace('__PROTECTED_FILES__',repr(protected))
 compile(pre,'preflight-sensor.py','exec');(packet/'preflight-sensor.py').write_text(pre)
 runner=(ROOT/'scripts/board/mpu6050/run_sensor_t1_t4.py.in').read_text().replace('__APP_BYTES__',str(appentry['bytes'])).replace('__APP_SHA__',appentry['sha256']);compile(runner,'run-sensor-t1-t4.py','exec');(packet/'run-sensor-t1-t4.py').write_text(runner)
 manifest={'id':'MPU_SENSOR_V1','destination':DEST,'board_deployed':False,'kernel':'6.1.99-rk3576-m0echo-p026','role':'BOUNDED_T1_T4_SENSOR','reuse':{'Image':'8c82342d740c9aefe0a4f0be8d84c436d14ebf7edb5f462e14631ce355cf3e8d','initrd':'c000c9ab2126b0a2e64581514077a7ca79aa80660989ebfbcb3a200366a1346d'},'application':app,'firmware':fm,'protected_boot_files':protected,'window_seconds':{'m0_owner':900,'linux_client_each':120,'clients':2,'runner_max':300},'uart_raw_limit':100,'files':{p.name:entry(p) for p in packet.iterdir()}}
 (packet/'SENSOR.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n');(packet/'SHA256SUMS').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in sorted(packet.iterdir()) if p.name!='SHA256SUMS'))
 installer=(ROOT/'scripts/board/mpu6050/install_resource_probe.py').read_text().replace('I2C_RESOURCE_PROBE_V1','MPU_SENSOR_V1').replace('I2C_RESOURCE_PROBE','MPU_SENSOR').replace('i2c-resource-probe-v1','mpu-sensor-v1').replace('RESOURCE_PROBE.json','SENSOR.json').replace('resource-probe.dtb','sensor.dtb').replace('stage-resource-probe','stage-sensor').replace('preflight-resource-probe.py','preflight-sensor.py').replace('rk3576_i2c_resource_probe.ko','rk3576_sensor.ko').replace("'rk3576_sensor.ko'}", "'rk3576_sensor.ko','rk3576_amp_health_test.ko','run-sensor-t1-t4.py'}").replace('exact eight-file packet','exact ten-file packet').replace('__PIN_MANIFEST__',sha(packet/'SENSOR.json'))
 (out/'install-sensor.py').write_text(installer)
 collector=(ROOT/'scripts/board/mpu6050/capture_resource_dual_uart.ps1').read_text().replace('/amp-p029/i2c-resource-probe-v1/stage-resource-probe.scr',DEST[5:]+'/stage-sensor.scr').replace('c98',f'{len(hdr)+len(data):x}').replace('[System.IO.FileAccess]::Write)', '[System.IO.FileAccess]::Write,[System.IO.FileShare]::Read)').replace('diag_limit120s total_limit540s','diag_limit300s total_limit720s').replace('-lt 540','-lt 720').replace('diagnostic capture deadline120s','sensor capture deadline300s').replace("$phase='DIAGNOSTIC';$phaseLimit=120","$phase='DIAGNOSTIC';$phaseLimit=300")
 (out/'capture-sensor-dual-uart.ps1').write_text(collector)

 spec=importlib.util.spec_from_file_location('install_sensor',out/'install-sensor.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);mod.validate_packet(packet)
 (out/'PACKAGE_BUILD.json').write_text(json.dumps({'packet':{p.name:entry(p) for p in packet.iterdir()},'installer':entry(out/'install-sensor.py'),'stage_cmd_bytes':len(raw),'stage_scr_bytes':len(hdr)+len(data)},indent=2)+'\n');print(out/'PACKAGE_BUILD.json')
if __name__=='__main__':main()
