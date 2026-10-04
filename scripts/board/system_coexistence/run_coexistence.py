#!/usr/bin/env python3
"""Bounded paired-kernel regression + unchanged Application coexistence harness."""
from pathlib import Path
import datetime
import json
import os
import shutil
import signal
import subprocess
import time

ROOT=Path('/home/cat/cockpit/system-coexistence-20261004-2200')
HEALTH=Path('/sys/module/rk3576_amp_health_test/parameters/health_status')
MAX_LOG=2*1024*1024
app=None
client=None


def need(ok,reason):
    if not ok: raise RuntimeError('STOP coexistence: '+reason)


def health():
    raw=HEALTH.read_text()
    need(len(raw)<2048,'health size')
    fields=dict(part.split('=',1) for part in raw.split())
    need(fields['bound']=='1' and fields['hello_ack']=='1','bound/ACK')
    need(fields['phase'] in ('READY','WAIT_PONG'),'health phase '+fields['phase'])
    need(fields['timeout']=='0' and fields['error']=='0','RPMsg timeout/error')
    need(int(fields['pong'])>0 and int(fields['last_pong_age_ms'])<5000,'real PONG stale')
    return fields


def graphics_env():
    choices=[]
    for p in Path('/proc').iterdir():
        if not p.name.isdigit(): continue
        try:
            if (p/'comm').read_text().strip()!='gnome-shell' or p.stat().st_uid!=os.getuid(): continue
            fields=(p/'environ').read_bytes().split(b'\0')
            env={field.split(b'=',1)[0].decode():field.split(b'=',1)[1].decode()
                 for field in fields if b'=' in field and field.split(b'=',1)[0] in (
                     b'DISPLAY',b'XDG_SESSION_TYPE',b'XDG_RUNTIME_DIR',b'XAUTHORITY')}
            if env.get('XDG_SESSION_TYPE')=='x11' and env.get('DISPLAY') and Path(env.get('XAUTHORITY','')).is_file():
                choices.append(env)
        except (OSError,UnicodeError): pass
    need(len(choices)==1,'exact active cat GNOME X11 environment required')
    env=dict(os.environ,**choices[0],QT_QPA_PLATFORM='xcb',QT_XCB_GL_INTEGRATION='none',
             LD_LIBRARY_PATH='/home/cat/cockpit/asr-target/lib')
    print('ACTUAL_GRAPHICAL_SESSION',json.dumps(choices[0]),flush=True)
    return env


def owners(nodes):
    holders=[]
    for p in Path('/proc').iterdir():
        if not p.name.isdigit(): continue
        try:
            for fd in (p/'fd').iterdir():
                try: target=os.readlink(fd)
                except OSError: continue
                if target in nodes: holders.append((int(p.name),target))
        except OSError: pass
    return holders


def resource(pid,previous):
    now=time.monotonic()
    status=Path(f'/proc/{pid}/stat').read_text()
    fields=status[status.rindex(')')+2:].split()
    ticks=int(fields[11])+int(fields[12])
    cpu=0.0
    if previous:
        cpu=(ticks-previous[1])/os.sysconf('SC_CLK_TCK')/(now-previous[0])*100.0
    rollup=Path(f'/proc/{pid}/smaps_rollup').read_text()
    memory={line.split(':')[0]:int(line.split()[1]) for line in rollup.splitlines()
            if line.startswith(('Rss:','Pss:'))}
    available={line.split(':')[0]:int(line.split()[1]) for line in Path('/proc/meminfo').read_text().splitlines()
               if line.startswith(('MemAvailable:','CmaFree:'))}
    temperature={}
    for zone in Path('/sys/class/thermal').glob('thermal_zone*'):
        try: temperature[(zone/'type').read_text().strip()]=int((zone/'temp').read_text())
        except (OSError,ValueError): pass
    row={'monotonic':round(now,3),'pid':pid,'cpu_percent_per_core_scale':round(cpu,2),
         'rss_kib':memory.get('Rss'),'pss_kib':memory.get('Pss'),**available,
         'threads':len(list(Path(f'/proc/{pid}/task').iterdir())),
         'fds':len(list(Path(f'/proc/{pid}/fd').iterdir())),'temperature_mC':temperature,
         'rpmsg':health()}
    return row,(now,ticks)


def stop_owned(process,label):
    if process is None or process.poll() is not None: return
    print('OWNED_PROCESS_STOP',label,process.pid,flush=True)
    process.send_signal(signal.SIGTERM)
    try: process.wait(timeout=15)
    except subprocess.TimeoutExpired:
        print('OWNED_PROCESS_SIGKILL',label,process.pid,'NOT_CLEAN',flush=True)
        process.kill()
        process.wait(timeout=5)


def main():
    global app,client
    need(os.uname().release=='6.1.99-rk3576-m0echo-p026','paired kernel')
    args=Path('/proc/cmdline').read_text().split()
    need('amp_test_stage=C' in args and 'amp_health_test=SI_HEALTH_V1' in args,'approved runtime identity')
    first=health()
    need(int(first['elapsed_ms'])<180000,'too late for bounded full test; do not bypass')
    env=graphics_env()
    need(shutil.disk_usage(ROOT).free>1024*1024*1024,'recording free space')
    need(not owners({'/dev/video11','/dev/snd/pcmC0D0c'}),'CAM0/ALSA occupant')
    # Discover and assert sensor graph again, not an alias-only mapping.
    graphs=[]
    for media in Path('/dev').glob('media*'):
        result=subprocess.run(['media-ctl','-d',str(media),'-p'],capture_output=True,text=True,timeout=5)
        need(result.returncode==0,'media graph read failed')
        graphs.append(result.stdout)
    # Sensor/CIF and ISP are distinct media controllers on this BSP.
    cif=[g for g in graphs if 'model           rkcif-mipi-lvds' in g and
         'm00_b_ov8858 3-0036' in g and
         '<- "m00_b_ov8858 3-0036":0 [ENABLED]' in g]
    isp=[g for g in graphs if 'model           rkisp0' in g and
         '<- "rkcif-mipi-lvds":0 [ENABLED]' in g and
         'entity 6: rkisp_mainpath' in g and 'device node name /dev/video11' in g]
    need(len(cif)==1 and len(isp)==1,'CAM0 CIF -> ISP graph mapping missing')
    need(not subprocess.check_output(['ss','-H','-lnt','sport = :8554'],text=True).strip(),'8554 already held')
    need(shutil.which('ffmpeg'),'existing RTSP decoder tool required, no installation')
    print('START_UTC',datetime.datetime.now(datetime.timezone.utc).isoformat(),flush=True)
    print('T0_T1_T2_CURRENT_HEALTH',first,flush=True)
    run=ROOT/'coexistence-run-v1'
    need(not run.exists(),'new evidence output only')
    run.mkdir(mode=0o700)
    with (run/'native-ctest.log').open('wb') as log:
        result=subprocess.run(['ctest','--test-dir',str(ROOT/'build'),'--output-on-failure'],
                              env=env,stdout=log,stderr=subprocess.STDOUT,timeout=150)
    text=(run/'native-ctest.log').read_text(errors='replace')
    print('NATIVE_CTEST_EXIT',result.returncode,flush=True)
    print(text[-1500:],flush=True)
    need(result.returncode==0 and 'out of 35' in text,'all existing native 35 tests')
    need(int(health()['pong'])>int(first['pong']),'RPMsg progression during regression')
    need(int(health()['elapsed_ms'])<270000,'insufficient bounded health window after native regression')
    output=run/'recordings'
    output.mkdir(mode=0o700)
    argv=[str(ROOT/'build/system_coexistence_probe'),
          '--camera-device','/dev/video11','--vision-model','/usr/share/model/RK3576/mobilenet_v1.rknn',
          '--asr-config',str(ROOT/'src/config/models/asr/sherpa_file_asr.conf'),
          '--asr-dir','/home/cat/cockpit/asr-target/models',
          '--vad-model','/home/cat/cockpit/asr-vad/models/silero_vad_v5.onnx',
          '--audio-device','hw:CARD=rockchipes8388,DEV=0','--output-dir',str(output),'--seconds','300']
    print('APPLICATION_COMMAND',json.dumps(argv),flush=True)
    previous=None
    seen=set()
    start=time.monotonic()
    with (run/'application.log').open('wb') as app_log, \
         (run/'rtsp-progress.log').open('wb') as progress, \
         (run/'rtsp-decoder.log').open('wb') as decode_log, \
         (run/'resources.jsonl').open('w') as samples:
        app=subprocess.Popen(argv,env=env,stdout=app_log,stderr=subprocess.STDOUT)
        print('OWNED_APP_PID',app.pid,flush=True)
        next_sample=time.monotonic()+5
        while app.poll() is None:
            need(time.monotonic()-start<420,'application outside test deadline')
            for name in ['application.log','rtsp-progress.log','rtsp-decoder.log','resources.jsonl']:
                need((run/name).stat().st_size<MAX_LOG,'log bound '+name)
            log=(run/'application.log').read_text(errors='replace')
            for line in log.splitlines():
                if line.startswith(('STAGE ','SYSTEM_COEXISTENCE_','COEXISTENCE_DURATION','APPLICATION_SHUTDOWN','QT_PREVIEW')) and line not in seen:
                    seen.add(line)
                    print(line,flush=True)
            if 'STAGE T5_RTSP PASS' in log and client is None:
                client=subprocess.Popen(['ffmpeg','-nostdin','-hide_banner','-loglevel','error',
                                         '-nostats','-progress','pipe:1','-rtsp_transport','udp',
                                         '-i','rtsp://127.0.0.1:8554/cam0','-an','-f','null','-'],
                                        stdout=progress,stderr=decode_log,start_new_session=True)
                print('OWNED_RTSP_CLIENT_PID',client.pid,'TRANSPORT UDP LOOPBACK_REAL_DECODE',flush=True)
            if time.monotonic()>=next_sample and app.poll() is None:
                row,previous=resource(app.pid,previous)
                samples.write(json.dumps(row,sort_keys=True)+'\n')
                samples.flush()
                print('RESOURCE_SAMPLE',json.dumps(row,sort_keys=True),flush=True)
                next_sample=time.monotonic()+5
            time.sleep(0.25)
    app_rc=app.wait(timeout=5)
    print('APPLICATION_EXIT',app_rc,flush=True)
    stop_owned(client,'RTSP decode client')
    need(app_rc==0,'application test failure')
    log=(run/'application.log').read_text(errors='replace')
    print(log[-7000:],flush=True)
    need('SYSTEM_COEXISTENCE_APPLICATION_PROBE_PASS' in log,'probe success marker')
    frames=[int(line.split('=',1)[1]) for line in (run/'rtsp-progress.log').read_text().splitlines()
            if line.startswith('frame=')]
    need(frames and max(frames)>=8000,'RTSP actual decoded-frame count')
    errors=(run/'rtsp-decoder.log').read_text(errors='replace')
    print('RTSP_DECODED_FRAMES',max(frames),'DECODER_LOG_BYTES',len(errors),flush=True)
    need(not errors.strip(),'RTSP decoder error; preserve and stop')
    last=health()
    time.sleep(3)
    after=health()
    need(int(after['pong'])>int(last['pong']),'post-process real PONG stopped')
    need(not owners({'/dev/video11','/dev/snd/pcmC0D0c'}),'CAM0/ALSA not released')
    need(not subprocess.check_output(['ss','-H','-lnt','sport = :8554'],text=True).strip(),'8554 not released')
    print('POST_APPLICATION_RPMSG',last,'THEN',after,flush=True)
    print('SHUTDOWN_RESOURCE_RELEASE CAM0=FREE ALSA=FREE 8554=FREE OWNED_APP=EXITED CLIENT=STOPPED',flush=True)
    for recording in output.glob('*.h264'):
        result=subprocess.run(['ffprobe','-v','error','-select_streams','v:0','-show_entries',
                               'stream=codec_name,width,height,r_frame_rate','-of','json',str(recording)],
                              capture_output=True,text=True,timeout=15)
        print('RECORDING_FFPROBE',result.returncode,result.stdout,flush=True)
        need(result.returncode==0,'recording probe')
    print('SYSTEM_COEXISTENCE_CONTROLLER_PASS',flush=True)


if __name__=='__main__':
    try: main()
    except Exception as failure:
        print('SYSTEM_COEXISTENCE_CONTROLLER_FAIL',type(failure).__name__,str(failure),flush=True)
        stop_owned(app,'Application')
        stop_owned(client,'RTSP decode client')
        raise SystemExit(1)
