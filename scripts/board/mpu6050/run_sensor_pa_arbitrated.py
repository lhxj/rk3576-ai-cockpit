#!/usr/bin/env python3
"""Root one-shot source-only USER suspension; unchanged v5 entry owns hardware guards."""
import argparse,hashlib,json,math,os,pathlib,re,selectors,signal,subprocess,time
SOURCE='alsa_input.platform-es8388-sound.HiFi__hw_rockchipes8388__source'
SERVER='unix:/run/user/1000/pulse/native'
ENTRY=pathlib.Path('/home/cat/cockpit/mpu-sensor-entry-v5/run_sensor_t5_t6_entry_v5.py')
ENTRY_SHA='bbad224f60604b91a0219ae659829a19ee028c4ee0a88597b72de04cbda4172d'
KNOWN={'USER','IDLE','SESSION','PASSTHROUGH','INTERNAL','APPLICATION','UNAVAILABLE'}
END=0

def need(ok,why):
 if not ok:raise RuntimeError('PA_ARBITRATION_STOP '+why)

def bounded(argv,seconds=5,cap=65536):
 seconds=min(seconds,END-time.monotonic()) if END else seconds
 need(seconds>0,'whole550 deadline')
 process=subprocess.Popen(argv,stdout=subprocess.PIPE,stderr=subprocess.PIPE,start_new_session=True)
 sel=selectors.DefaultSelector();out=[bytearray(),bytearray()];end=time.monotonic()+seconds
 for i,pipe in enumerate([process.stdout,process.stderr]):sel.register(pipe,selectors.EVENT_READ,i)
 try:
  while sel.get_map():
   need(time.monotonic()<end,'command deadline')
   for key,_ in sel.select(min(.1,max(0,end-time.monotonic()))):
    raw=os.read(key.fileobj.fileno(),4096)
    if not raw:sel.unregister(key.fileobj);continue
    out[key.data].extend(raw);need(sum(map(len,out))<=cap,'command output cap')
  rc=process.wait(timeout=max(.01,end-time.monotonic()))
  need(rc==0,'command rc '+str(rc)+' '+bytes(out[1])[:256].decode(errors='replace'))
  return bytes(out[0]).decode()
 finally:
  sel.close()
  if process.poll() is None:
   os.killpg(process.pid,signal.SIGTERM)
   try:process.wait(timeout=1)
   except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait(timeout=1)
  process.stdout.close();process.stderr.close()

def pa(program,*args):
 return bounded(['/usr/sbin/runuser','-u','cat','--','/usr/bin/env','LC_ALL=C.UTF-8','XDG_RUNTIME_DIR=/run/user/1000','PULSE_SERVER='+SERVER,'/usr/bin/'+program,*args])

def parse_source(text):
 need(len(text.encode())<=65536,'pacmd size')
 blocks=re.split(r'(?m)^\s*\*?\s*index:\s*',text)[1:]
 targets=[]
 for block in blocks:
  if re.search(r'(?m)^\s*name: <'+re.escape(SOURCE)+r'>\s*$',block):targets.append(block)
 need(len(targets)==1,'unique exact capture source')
 b=targets[0];index=int(b.splitlines()[0].strip());need(index==1,'capture source index changed')
 for key,value in [('device.class','sound'),('alsa.card','0'),('alsa.device','0'),('alsa.name','dailink-multicodecs ES8323 HiFi-0')]:
  need(re.search(r'(?m)^\s*'+re.escape(key)+r' = "'+re.escape(value)+r'"\s*$',b),'capture identity '+key)
 need(re.search(r'(?m)^\s*driver: <module-alsa-card.c>\s*$',b),'capture driver')
 causes=re.findall(r'(?m)^\s*suspend cause:\s*(.*?)\s*$',b);states=re.findall(r'(?m)^\s*state:\s*(\S+)\s*$',b)
 need(len(causes)==1 and len(states)==1,'cause/state fields')
 tokens=set() if causes[0]=='(none)' else set(causes[0].split('|'));need(tokens<=KNOWN and (tokens or causes[0]=='(none)'),'unknown suspend cause')
 need(states[0] in {'SUSPENDED','IDLE','RUNNING'},'source state')
 need(bool(tokens)==(states[0]=='SUSPENDED'),'inconsistent cause/state')
 return {'index':index,'name':SOURCE,'state':states[0],'causes':sorted(tokens),'user':'USER' in tokens}

def sink_signature(text):
 rows=[]
 for block in re.split(r'(?m)^\s*\*?\s*index:\s*',text)[1:]:
  def field(key):
   values=re.findall(r'(?m)^\s*'+key+r':\s*(.*?)\s*$',block);need(len(values)==1,'sink field '+key);return values[0]
  rows.append((int(block.splitlines()[0]),field('name'),field('state'),field('suspend cause')))
 need(bool(rows),'sink signature missing');return sorted(rows)

def snapshot():return parse_source(pa('pacmd','list-sources'))

def server_identity():
 info=json.loads(pa('pactl','--format=json','info'))
 need(info.get('server_string')==SERVER and info.get('user_name')=='cat' and info.get('server_name')=='pulseaudio' and info.get('server_version')=='16.1','exact local PA16.1 server')
 socket=pathlib.Path('/run/user/1000/pulse/native').stat()
 return (info['cookie'],socket.st_dev,socket.st_ino)

def runtime_precheck(age,check_only):
 need(os.geteuid()==0,'root wrapper')
 kernel=os.uname().release
 if check_only:need(kernel=='6.1.99-rk3576','check-only default kernel')
 else:
  need(kernel=='6.1.99-rk3576-m0echo-p026','paired kernel')
  args=pathlib.Path('/proc/cmdline').read_text().split()
  need('amp_test_stage=C' in args and 'mpu_sensor=MPU_SENSOR_COEXISTENCE_V1' in args,'runtime identity')
  need(math.isfinite(age) and 0<=age<=390,'finite SOURCE age')
  fd=os.open(ENTRY,os.O_RDONLY|os.O_NOFOLLOW)
  try:
   need(os.fstat(fd).st_size==5713,'entry exact size');raw=os.read(fd,5714)
   need(hashlib.sha256(raw).hexdigest()==ENTRY_SHA,'entry exact hash')
  finally:os.close(fd)

def actors_released():
 for p in pathlib.Path('/proc').iterdir():
  if not p.name.isdigit():continue
  try:args=(p/'cmdline').read_bytes().split(b'\0')
  except OSError:continue
  if args and any(arg.startswith(b'/home/cat/cockpit/mpu-sensor-app-v4/build/apps/cockpit_ui/') for arg in args):return False
  if args and b'ffmpeg' in args[0] and b'rtsp://127.0.0.1:8554/cam0' in args:return False
 process=subprocess.run(['/usr/bin/fuser','/dev/video11','/dev/snd/pcmC0D0c'],capture_output=True,timeout=2)
 need(len(process.stdout)+len(process.stderr)<65536,'fuser bound')
 if process.returncode!=1 or process.stdout or process.stderr:return False
 return not bounded(['/usr/bin/ss','-H','-lnt','sport = :8554'],seconds=2).strip()

def stop_owned(child):
 if child is None:return
 if child.poll() is None:
  child.send_signal(signal.SIGINT) # Original Python finally owns nested runuser/app/decoder cleanup.
  try:child.wait(timeout=min(10,max(.01,END-time.monotonic())))
  except subprocess.TimeoutExpired:raise RuntimeError('owned entry still running; restore pending')
 need(actors_released(),'nested actors/resources not proved released; restore pending')

def arbitrate(age,check_only=False):
 global END
 start=time.monotonic();END=start+(60 if check_only else 550);runtime_precheck(age,check_only)
 identity=server_identity();before=snapshot();sinks=sink_signature(pa('pacmd','list-sinks')) if check_only else None
 print('PA_BASELINE',json.dumps(before),flush=True)
 restore=False;child=None;child_rc=0
 try:
  if not before['user']:
   restore=True # Setter may have succeeded even if the client times out.
   pa('pactl','suspend-source',SOURCE,'1')
  need(server_identity()==identity,'PA server changed')
  suspended=snapshot();need(suspended['user'] and suspended['state']=='SUSPENDED','capture USER suspend unproven')
  print('PA_TARGET_USER_SUSPENDED',json.dumps(suspended),flush=True)
  if not check_only:
   updated=age+time.monotonic()-start;need(updated<=390,'SOURCE age after arbitration')
   child=subprocess.Popen(['/usr/bin/python3',str(ENTRY),'--source-age-seconds',str(updated)],start_new_session=True)
   deadline=start+500 # Reserve 50s for original finally/resource proof/USER restore.
   while child.poll() is None:
    need(time.monotonic()<deadline,'entry lifecycle deadline');time.sleep(.1)
   child_rc=child.wait();print('PA_ENTRY_EXIT',child_rc,flush=True)
 finally:
  try:
   stop_owned(child)
   need(server_identity()==identity,'PA server changed; restore pending')
   snapshot() # Revalidate exact identity/cause before any rollback mutation.
   if restore:pa('pactl','suspend-source',SOURCE,'0')
   after=snapshot();need(after['user']==before['user'],'USER bit restoration failed')
   if check_only:need(sink_signature(pa('pacmd','list-sinks'))==sinks,'check-only sink state changed')
   print('PA_USER_RESTORED',json.dumps(after),flush=True)
  except Exception as error:
   print('PA_RESTORE_PENDING',type(error).__name__,str(error),flush=True);raise
 need(child_rc==0,'original entry failed '+str(child_rc))
 return child_rc

def interrupted(sig,frame):raise RuntimeError('PA wrapper signal '+str(sig))
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--source-age-seconds',type=float,default=0);parser.add_argument('--arbitration-check-only',action='store_true');args=parser.parse_args()
 for sig in (signal.SIGTERM,signal.SIGINT,signal.SIGHUP):signal.signal(sig,interrupted)
 try:arbitrate(args.source_age_seconds,args.arbitration_check_only)
 except Exception as error:print('PA_WRAPPER_FAIL',type(error).__name__,str(error),flush=True);raise SystemExit(1)
