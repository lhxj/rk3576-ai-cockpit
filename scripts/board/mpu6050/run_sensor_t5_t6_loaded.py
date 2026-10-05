#!/usr/bin/env python3
"""Explicit root controller holds board lock. No reboot/reset/retry; fail preserves KO refs."""
import argparse,fcntl,hashlib,json,math,os,struct,pathlib,pwd,selectors,signal,subprocess,time
BASE=pathlib.Path('/boot/amp-p029/mpu-sensor-coexistence-v1')
APP=pathlib.Path('/home/cat/cockpit/mpu-sensor-app-v3')
END=0

def need(ok,reason):
 if not ok:raise RuntimeError('SENSOR_T5T6_STOP '+reason)
def remaining():need(END>time.monotonic(),'whole480s deadline');return END-time.monotonic()
def command(args,seconds):return subprocess.run(args,check=True,timeout=min(seconds,remaining()),capture_output=True)
def health(initial=False):
 text=pathlib.Path('/sys/module/rk3576_amp_health_test/parameters/health_status').read_text();need(len(text)<2048,'bounded health');fields=dict(x.split('=',1) for x in text.split())
 need(fields['timeout']=='0' and fields['error']=='0' and fields['phase'] not in ('FAILED','STOPPED'),'health timeout/error/stop')
 if initial and not (fields['bound']=='1' and fields['hello_ack']=='1' and int(fields['pong'])>=3):return fields
 need(fields['bound']=='1' and fields['phase'] in ('READY','WAIT_PONG') and fields['hello_ack']=='1','health bound/phase/ACK')
 need(int(fields['last_pong_age_ms'])<5000,'health stale');print('SENSOR_T5T6_HEALTH',text.strip(),flush=True);return fields

def graphical_env():
 uid=pwd.getpwnam('cat').pw_uid;choices=[]
 for p in pathlib.Path('/proc').iterdir():
  if not p.name.isdigit():continue
  try:
   if p.stat().st_uid!=uid or (p/'comm').read_text().strip()!='gnome-shell':continue
   entries=(p/'environ').read_bytes().split(b'\0');env={x.split(b'=',1)[0].decode():x.split(b'=',1)[1].decode() for x in entries if b'=' in x and x.split(b'=',1)[0] in (b'DISPLAY',b'XAUTHORITY',b'XDG_SESSION_TYPE',b'XDG_RUNTIME_DIR')}
   if env.get('XDG_SESSION_TYPE')=='x11' and env.get('DISPLAY') and pathlib.Path(env.get('XAUTHORITY','')).is_file():choices.append(env)
  except (OSError,UnicodeError):pass
 need(len(choices)==1,'exact graphical cat session');return dict(os.environ,**choices[0],HOME='/home/cat',USER='cat',LOGNAME='cat',QT_QPA_PLATFORM='xcb',QT_XCB_GL_INTEGRATION='none',LD_LIBRARY_PATH='/home/cat/cockpit/asr-target/lib')

def codec_snapshot():
 raw=command(['dmesg','--color=never'],5).stdout;need(len(raw)<=2*1024*1024,'kernel log cap');return raw.decode(errors='replace')
def codec_cursor(text):
 import re
 values=[float(x) for x in re.findall(r'^\[\s*([0-9.]+)\]',text,re.M)];need(bool(values),'kernel timestamp log');return max(values)
def codec_check(text,cursor):
 import re
 added=[line for line in text.splitlines() if (match:=re.match(r'^\[\s*([0-9.]+)\]',line)) and float(match[1])>cursor and 'ES8323' in line and '-6' in line]
 for line in added:print('NEW_CODEC_ERROR',line,flush=True)
 need(not added,'new codec -6 in T6 window; preserve evidence/no expanded test')

class StaticWindow:
 # Functional plausibility only, no calibration/bias subtraction. Module fixed throughout this window.
 def __init__(self):self.first=None;self.rows=[];self.done=False
 def feed(self,line):
  if not line.startswith('SENSOR_UI elapsed_s='):return
  fields=dict(part.split('=',1) for part in line.split()[1:]);elapsed=int(fields['elapsed_s'])
  if fields['valid']!='1':return
  if self.first is None:self.first=elapsed
  relative=elapsed-self.first
  if 10<=relative<25:
   xyz=[float(fields[k]) for k in ('ax','ay','az')];gyro=[float(fields[k]) for k in ('gx','gy','gz')]
   need(all(math.isfinite(x) for x in xyz+gyro),'finite static data')
   self.rows.append((math.sqrt(sum(x*x for x in xyz)),gyro))
  if relative>=25 and not self.done:
   need(len(self.rows)>=10,'static10..25s sufficient observations')
   norms=[x[0] for x in self.rows];means=[sum(x[1][axis] for x in self.rows)/len(self.rows) for axis in range(3)]
   result={'window':'first VALID +10 inclusive ..25 exclusive seconds','observations':len(self.rows),'accel_norm_g_min':min(norms),'accel_norm_g_max':max(norms),'accel_norm_g_mean':sum(norms)/len(norms),'gyro_mean_dps':means,'tolerance':{'accel_norm_g':[0.85,1.15],'abs_gyro_axis_mean_dps_max':10.0},'calibrated':False}
   print('SENSOR_STATIC_WINDOW',json.dumps(result,sort_keys=True),flush=True)
   need(all(.85<=x<=1.15 for x in norms) and all(abs(x)<=10 for x in means),'functional static tolerance; preserve raw bias')
   self.done=True;print('T5_DIRECTION_CHANGE_ALLOWED fixed static window completed; human observation required',flush=True)

def all_uid_occupants():
 nodes=['/dev/video11','/dev/snd/pcmC0D0c']
 need(all(pathlib.Path(n).is_char_device() for n in nodes),'known CAM0/ALSA capture nodes')
 result=subprocess.run(['fuser',*nodes],timeout=min(5,remaining()),capture_output=True)
 need(result.returncode==1 and not result.stdout.strip() and not result.stderr.strip(),'root all-UID CAM0/ALSA occupant or fuser error')
 print('ROOT_ALL_UID_CAPTURE_FUSER_FREE',nodes,flush=True)

def run_process(args,seconds,env,check_codec=False,static_window=None):
 baseline=codec_snapshot() if check_codec else ''
 cursor=codec_cursor(baseline) if check_codec else 0
 if check_codec:
  old=[line for line in baseline.splitlines() if 'ES8323' in line and '-6' in line]
  need(len(old)<=32,'bounded baseline codec entries')
  print('T6_CODEC_BASELINE',json.dumps({'cursor':cursor,'startup_errors':old}),flush=True)
 process=subprocess.Popen(['runuser','-u','cat','--',*args],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env,start_new_session=True)
 select=selectors.DefaultSelector();select.register(process.stdout,selectors.EVENT_READ);deadline=min(END,time.monotonic()+seconds);pending=b'';budget=2*1024*1024;last=0
 try:
  while select.get_map():
   need(time.monotonic()<deadline,'process deadline')
   if time.monotonic()-last>=5:
    health()
    if check_codec:codec_check(codec_snapshot(),cursor)
    last=time.monotonic()
   for key,_ in select.select(min(1,remaining())):
    raw=os.read(key.fd,4096)
    if not raw:need(not pending,'incomplete log line');select.unregister(key.fileobj);continue
    budget-=len(raw);need(budget>=0,'process2MiB log budget');pending+=raw
    while b'\n' in pending:
     line,pending=pending.split(b'\n',1);need(len(line)<=16384,'bounded log line');text=line.decode(errors='replace');print(text,flush=True)
     if static_window:static_window.feed(text)
    need(len(pending)<=16384,'partial log line cap')
  need(process.wait(timeout=3)==0,'application exit failure')
  if check_codec:codec_check(codec_snapshot(),cursor)
  if static_window:need(static_window.done,'static observation window not complete')
 finally:
  if process.poll() is None:
   os.killpg(process.pid,signal.SIGTERM)
   try:process.wait(timeout=15)
   except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait(timeout=5)
  select.close();process.stdout.close()

def validate_loaded_note(note,name):
 need(len(note)==36,'loaded KO GNU note length')
 namesz,descsz,kind=struct.unpack('<III',note[:12]);need((namesz,descsz,kind)==(4,20,3) and note[12:16]==b'GNU\0','loaded KO buildid ABI')
 need(note[16:].hex()=={'rk3576_sensor':'8b450105c25698f4709fc2f00c501d3b55abb8ec','rk3576_amp_health_test':'0d5716e9ba0f949baff9d68fa1cdffe84ea6e08c'}[name],'loaded KO buildid mismatch')
def validate_link_state(state):
 need(len(state)==56,'loaded ioctl state length')
 abi,ready,removed,reserved,generation,overwrite,drops,malformed,sendfail=struct.unpack('<IIIIQQQQQ',state)
 need(abi==1 and ready==1 and removed==0 and reserved==0 and generation>0 and malformed==0 and sendfail==0,'loaded sensor ABI/owner/generation/state')

def loaded_preflight():
 need(os.uname().release=='6.1.99-rk3576-m0echo-p026','paired loaded kernel')
 args=pathlib.Path('/proc/cmdline').read_text().split();need('mpu_sensor=MPU_SENSOR_COEXISTENCE_V1' in args and 'amp_test_stage=C' in args,'current stage identity')
 manifest=BASE/'SENSOR.json';need(manifest.stat().st_size==13638 and hashlib.sha256(manifest.read_bytes()).hexdigest()=='88738e0c011bafbb8d52a73ff73f3a333a37647d0613d615e7b17f808677dd88','exact frozen v2 manifest')
 data=json.loads(manifest.read_text())
 for name in ['rk3576_sensor','rk3576_amp_health_test']:
  p=BASE/(name+'.ko');want=data['files'][p.name];need(p.is_file() and not p.is_symlink() and p.stat().st_size==want['bytes'] and p.stat().st_size<512*1024 and hashlib.sha256(p.read_bytes()).hexdigest()==want['sha256'],'exact loaded source KO')
  note=(pathlib.Path('/sys/module')/name/'notes/.note.gnu.build-id').read_bytes();need(len(note)==36,'loaded KO GNU note length')
  validate_loaded_note(note,name)
 dt=pathlib.Path('/sys/firmware/devicetree/base');need((dt/'i2c@2ae80000/status').read_bytes()==b'disabled\0','I2C9 Linux disabled')
 need(not pathlib.Path('/sys/bus/platform/devices/2ae80000.i2c').exists(),'no I2C9 platform owner')
 need(pathlib.Path('/sys/bus/platform/devices/mcu-amp/driver').resolve().name=='rockchip-amp','retained clock owner')
 for adapter in pathlib.Path('/sys/class/i2c-adapter').glob('*'):need('2ae80000' not in str((adapter/'device/of_node').resolve()),'no Linux I2C9 adapter')
 fd=os.open('/dev/rk3576-sensor-v1',os.O_RDONLY|os.O_NONBLOCK|os.O_CLOEXEC)
 try:
  state=bytearray(56);fcntl.ioctl(fd,0x80385301,state,True);abi,ready,removed,reserved,generation,overwrite,drops,malformed,sendfail=struct.unpack('<IIIIQQQQQ',state)
  validate_link_state(state)
  print('SENSOR_LOADED_PREFLIGHT_PASS',json.dumps({'abi':abi,'owner_ready':ready,'generation':generation,'sample_overwrites':overwrite,'control_drops':drops,'malformed':malformed,'send_failures':sendfail,'modules_reloaded':False}),flush=True)
 finally:os.close(fd)

def main():
 global END
 parser=argparse.ArgumentParser();parser.add_argument('--source-age-seconds',type=float,required=True);args=parser.parse_args()
 need(math.isfinite(args.source_age_seconds) and 0<=args.source_age_seconds<=390,'root measured SOURCE age <=390s')
 END=time.monotonic()+480
 need(os.geteuid()==0,'root controller only')
 for relative,want in {'apps/cockpit_ui/cockpit_ui': {'bytes': 13142952, 'sha256': '67dd4849053a29a1e1bcc04b8df2bb2efd22516cd5845ba13806ae3c2c406540'}, 'apps/cockpit_ui/sensor_stream_probe': {'bytes': 9090880, 'sha256': '7cdb873cfefe0c3351e1fcf0383588c32807c7436f5fdd2d3f4f3aa344f30fd7'}, 'apps/cockpit_ui/sensor_ui_probe': {'bytes': 12810112, 'sha256': '41278f4296c424f15e51e93c6b524d445594fd7b26e38074fad79dbe6de87302'}, 'apps/cockpit_ui/sensor_coexistence_probe': {'bytes': 15981192, 'sha256': '4268d010fa09d7b9a2ac7a1cf5ca97defbddc35f630156bce39b6bbd3b600518'}}.items():
  p=APP/'build'/relative;need(p.is_file() and not p.is_symlink() and p.stat().st_size==want['bytes'] and p.stat().st_size<32*1024*1024,'exact application bounded file')
  with p.open('rb') as stream:
   digest=hashlib.sha256()
   for raw in iter(lambda:stream.read(1024*1024),b''):digest.update(raw)
  need(digest.hexdigest()==want['sha256'],'application hash')
 loaded_preflight()
 initial_health=health();need(int(initial_health['pong'])>=3 and int(initial_health['elapsed_ms'])<=200000,'loaded health PONG>=3 elapsed<=200s; preserve frozen child300s gate')
 node=pathlib.Path('/dev/rk3576-sensor-v1');need(node.is_char_device(),'sensor char node');os.chown(node,pwd.getpwnam('cat').pw_uid,-1);os.chmod(node,0o600)
 env=graphical_env();print('T5_FIXED_MODULE_SETTLE minimum10s after sensor valid; manual direction changes only after stable observation; raw bias unchanged',flush=True)
 all_uid_occupants()
 run_process([str(APP/'build/apps/cockpit_ui/sensor_ui_probe'),'--observe-real-sensor'],110,env,static_window=StaticWindow())
 print('T5_USER_CONFIRMATION_PENDING unless human observation separately confirmed',flush=True)
 all_uid_occupants()
 run_process(['python3',str(BASE/'run-sensor-coexistence.py')],450,env,True)
 previous=int(health()['pong']);time.sleep(3);need(int(health()['pong'])>previous,'post app health stalled')
 command(['rmmod','rk3576_sensor'],10);command(['rmmod','rk3576_amp_health_test'],10)
 need(not node.exists() and not pathlib.Path('/sys/module/rk3576_sensor').exists() and not pathlib.Path('/sys/module/rk3576_amp_health_test').exists(),'normal module/node release')
 print('SENSOR_T5T6_RUNTIME_EXIT; USER_CONFIRMATION and default cold recovery still required',flush=True)
if __name__=='__main__':main()
