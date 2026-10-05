#!/usr/bin/env python3
"""Explicit user-directory ARM64 build. No boot/KO/MPU operations or binary execution."""
import argparse,hashlib,json,os,pathlib,subprocess,tarfile,selectors,signal,time
DEST=pathlib.Path('/home/cat/cockpit/mpu-sensor-app-v2')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def bounded_build(command, log_path, seconds, budget):
 process=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
 selector=selectors.DefaultSelector();selector.register(process.stdout,selectors.EVENT_READ);end=time.monotonic()+seconds
 try:
  with log_path.open('wb') as log:
   while selector.get_map():
    if time.monotonic()>=end:raise TimeoutError('native build deadline')
    for key,_ in selector.select(min(1,max(0,end-time.monotonic()))):
     chunk=os.read(key.fd,65536)
     if not chunk:selector.unregister(key.fileobj);continue
     if len(chunk)>budget[0]:raise RuntimeError('native build combined log limit2MiB')
     log.write(chunk);budget[0]-=len(chunk)
  rc=process.wait(timeout=3)
  if rc:raise subprocess.CalledProcessError(rc,command)
 except BaseException:
  if process.poll() is None:
   os.killpg(process.pid,signal.SIGTERM)
   try:process.wait(timeout=3)
   except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait(timeout=3)
  raise
 finally:selector.close();process.stdout.close()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--source',type=pathlib.Path,required=True);ap.add_argument('--manifest',type=pathlib.Path,required=True);a=ap.parse_args()
 if os.uname().machine!='aarch64':raise SystemExit('native aarch64 only')
 if a.source.resolve().parent!=DEST or a.manifest.resolve().parent!=DEST:raise SystemExit('fixed user directory required')
 if (DEST/'src').exists() or (DEST/'build').exists():raise SystemExit('fresh src/build required')
 assert a.source.is_file() and not a.source.is_symlink() and a.source.stat().st_size<16*1024*1024
 m=json.loads(a.manifest.read_text());assert sha(a.source)==m['archive_sha256'] and a.source.stat().st_size==m['archive_bytes']
 with tarfile.open(a.source) as archive:
  items=archive.getmembers();assert len(items)==len(m['members']) and len(items)<2000
  assert {i.name for i in items}==set(m['members'])
  for item in items:
   path=pathlib.PurePosixPath(item.name);assert item.isfile() and not path.is_absolute() and '..' not in path.parts and path.parts[0]=='src' and item.size<2*1024*1024
   data=archive.extractfile(item).read();assert len(data)==m['members'][item.name]['bytes'] and hashlib.sha256(data).hexdigest()==m['members'][item.name]['sha256']
  archive.extractall(DEST)
 args=['cmake','-S',str(DEST/'src'),'-B',str(DEST/'build'),'-DCMAKE_BUILD_TYPE=Debug','-DBUILD_TESTING=ON','-DCOCKPIT_ENABLE_V4L2_CAMERA=ON','-DCOCKPIT_ENABLE_MPP_RECORDING=ON','-DCOCKPIT_ENABLE_ALSA_CAPTURE=ON','-DCOCKPIT_ENABLE_SHERPA_ASR=ON','-DCOCKPIT_ENABLE_RKNN=ON','-DSHERPA_ONNX_INCLUDE_DIR=/home/cat/cockpit/asr-target/include','-DSHERPA_ONNX_LIBRARY=/home/cat/cockpit/asr-target/lib/libsherpa-onnx-c-api.so','-DCOCKPIT_SHERPA_MODEL_DIR=/home/cat/cockpit/asr-target/models','-DCOCKPIT_SHERPA_TEST_WAV=/home/cat/cockpit/asr-target/testdata/0.wav','-DCOCKPIT_VAD_MODEL=/home/cat/cockpit/asr-vad/models/silero_vad_v5.onnx','-DCOCKPIT_VAD_TEST_WAV=/home/cat/cockpit/asr-vad/testdata/1.wav','-DRKNN_INCLUDE_DIR=/home/cat/cockpit/vision-rknn-20261002/rknn/include','-DRKNN_LIBRARY=/usr/lib/librknnrt.so']
 budget=[2*1024*1024]
 for command,name,limit in [(args,'configure.log',90),(['cmake','--build',str(DEST/'build'),'--parallel','2'],'build.log',360)]:
  print('NATIVE_COMMAND',command,flush=True)
  bounded_build(command,DEST/name,limit,budget)
 files={}
 for name in ('apps/cockpit_ui/cockpit_ui','apps/cockpit_ui/sensor_stream_probe'):
  path=DEST/'build'/name;elf=path.read_bytes();assert elf[:4]==b'\x7fELF' and int.from_bytes(elf[18:20],'little')==183
  needed=subprocess.check_output(['readelf','-d',str(path)],text=True,timeout=10);(DEST/(path.name+'-dynamic.txt')).write_text(needed)
  files[name]={'bytes':len(elf),'sha256':sha(path)}
 result={'kind':'MPU_SENSOR_AARCH64_APPLICATION_BUILT_NOT_RUN','source_archive_sha256':m['archive_sha256'],'source_commit':m['source_commit'],'files':files,'board_kernel_at_build':os.uname().release,'hardware_access':False,'boot_ko_changes':False}
 (DEST/'application-build.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
if __name__=='__main__':main()
