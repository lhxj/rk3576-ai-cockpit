#!/usr/bin/env python3
"""Root-reviewed one-shot preloader; failure retains references for root cleanup."""
import argparse
import hashlib
import math
import os
import pathlib
import stat
import subprocess
import time

BASE = pathlib.Path('/boot/amp-p029/mpu-sensor-coexistence-v1')
LOADED = pathlib.Path('/home/cat/cockpit/mpu-sensor-runtime-loaded-v4/run_sensor_t5_t6_loaded_v4.py')
EXPECTED = {
 'SENSOR.json': (13638, '88738e0c011bafbb8d52a73ff73f3a333a37647d0613d615e7b17f808677dd88'),
 'preflight-sensor.py': (15803, '3459b7ce448e032635b1ac11c559b0b479d1ccb72bb2ef80e0705c9f78598680'),
 'rk3576_amp_health_test.ko': (90096, '57b1d830d6687f4f891bbabc3c9694c976c79ab2fcea3d1671a6289d95dea700'),
 'rk3576_sensor.ko': (144128, '063175ef8c5a12855192f790b64664f2d14fe439415be44ab383479c8ccea69a'),
}
LOADED_SHA = 'ccf85c2ae2d5d29e1e87fde28dca799a55bccdd6cb502192bb228ce6ce15b4a8'

COEX = pathlib.Path('/home/cat/cockpit/mpu-sensor-runtime-loaded-v4/run_sensor_coexistence_v4.py')
COEX_EXPECTED = (10869, '84f0f5195496948ff6ef7f10df5b9758d74467be4fd12b362be831544a73ee72')
APP_ASSETS = {'cockpit_ui': (13142952, '0c15efa4a9087bfdeea089035a5e0db12f46ce5fb918b1941be8ea9d5d86a541'), 'sensor_stream_probe': (9090880, '780de853a50ee8c184cf61a6aa10433d3ade672a99a87cc7bea4aa430b6914b3'), 'sensor_ui_probe': (12810112, '33bf7de9aacdc24c75bcb1d6fdc2268434963a67acdd53039cf780b8171e0775'), 'sensor_coexistence_probe': (15983848, '3496c570cbceea6264c1f878162c5a795b59183ad36dee68172eea5823fbf9f5')}

def need(ok, reason):
 if not ok:
  raise RuntimeError('SENSOR_ENTRY_STOP ' + reason)

def digest(path, expected_size, expected_sha, max_bytes=512*1024):
 fd = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
 try:
  before = os.fstat(fd)
  need(stat.S_ISREG(before.st_mode) and 0 < before.st_size <= max_bytes, 'bounded regular asset')
  need(expected_size is None or before.st_size == expected_size, 'asset size ' + path.name)
  raw = b''
  while len(raw) <= max_bytes:
   chunk = os.read(fd, 65536)
   if not chunk:
    break
   raw += chunk
  after = os.fstat(fd)
  need(before.st_size == after.st_size == len(raw) and before.st_mtime_ns == after.st_mtime_ns,
       'asset changed during read')
  need(hashlib.sha256(raw).hexdigest() == expected_sha, 'asset hash ' + path.name)
 finally:
  os.close(fd)

def identity():
 need(os.geteuid() == 0, 'root only')
 need(os.uname().release == '6.1.99-rk3576-m0echo-p026', 'paired kernel')
 raw = pathlib.Path('/proc/cmdline').read_text()
 need(len(raw) < 8192, 'bounded cmdline')
 args = raw.split()
 need('amp_test_stage=C' in args and 'mpu_sensor=MPU_SENSOR_COEXISTENCE_V1' in args,
      'paired boot identity')
 for name in ('rk3576_amp_health_test', 'rk3576_sensor'):
  need(not (pathlib.Path('/sys/module') / name).exists(), 'module already loaded ' + name)
 need(not pathlib.Path('/dev/rk3576-sensor-v1').exists(), 'old sensor node')

def health_ready():
 path = pathlib.Path('/sys/module/rk3576_amp_health_test/parameters/health_status')
 raw = path.read_text()
 need(len(raw) < 2048, 'bounded health')
 row = dict(part.split('=', 1) for part in raw.split())
 need(row['timeout'] == '0' and row['error'] == '0', 'health timeout/error')
 need(row['phase'] not in ('FAILED', 'STOPPED', 'DONE'), 'health phase ended')
 need(int(row['window_ms']) == 720000 and int(row['elapsed_ms']) <= 200000, 'health budget')
 ready = (row['bound'] == '1' and row['hello_ack'] == '1' and int(row['pong']) >= 3
          and row['phase'] in ('READY', 'WAIT_PONG'))
 if ready:
  need(int(row['last_pong_age_ms']) < 5000, 'stale health')
  print('SENSOR_ENTRY_HEALTH_READY', raw.strip(), flush=True)
 return ready

def run(source_age):
 start = time.monotonic()
 need(math.isfinite(source_age) and 0 <= source_age <= 390, 'SOURCE age <=390s')
 def guard():
  elapsed = time.monotonic() - start
  need(0 <= elapsed < 60, 'preload60s deadline')
  need(source_age + elapsed <= 390, 'fresh SOURCE age <=390s')
  return 60 - elapsed
 def verify(name):
  guard()
  digest(BASE / name, *EXPECTED[name])
  guard()
 def command(args, limit):
  subprocess.run(args, check=True, timeout=min(limit, guard()))
  guard()
 guard()
 identity()
 for name in EXPECTED:
  verify(name)
 digest(LOADED, None, LOADED_SHA)
 digest(COEX, *COEX_EXPECTED)
 for name, expected in APP_ASSETS.items():
  guard()
  digest(pathlib.Path('/home/cat/cockpit/mpu-sensor-app-v4/build/apps/cockpit_ui') / name, *expected, max_bytes=32*1024*1024)
  guard()
 verify('preflight-sensor.py')
 command(['python3', str(BASE / 'preflight-sensor.py')], 15)
 verify('rk3576_amp_health_test.ko')
 command(['insmod', str(BASE / 'rk3576_amp_health_test.ko')], 10)
 health_end = time.monotonic() + min(20, guard())
 while True:
  guard()
  need(time.monotonic() < health_end, 'health handshake20s deadline')
  if health_ready():
   break
  time.sleep(min(.25, guard()))
 verify('rk3576_sensor.ko')
 command(['insmod', str(BASE / 'rk3576_sensor.ko')], 10)
 verify('SENSOR.json')
 digest(COEX, *COEX_EXPECTED)
 digest(LOADED, None, LOADED_SHA)
 guard()
 need(health_ready(), 'health changed before loaded handoff')
 age = source_age + time.monotonic() - start
 guard()
 print('SENSOR_ENTRY_HANDOFF source_age_seconds=' + str(age), flush=True)
 # Replace this process: unchanged loaded controller owns its normal exit paths/deadline.
 os.execv('/usr/bin/python3', ['python3', str(LOADED), '--source-age-seconds', str(age)])

def main():
 parser = argparse.ArgumentParser()
 parser.add_argument('--source-age-seconds', type=float, required=True,
                     help='root measured actual SOURCE age at dispatch, never a planned age')
 args = parser.parse_args()
 run(args.source_age_seconds)

if __name__ == '__main__':
 main()
