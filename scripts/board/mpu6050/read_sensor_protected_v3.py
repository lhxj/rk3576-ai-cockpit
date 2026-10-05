#!/usr/bin/env python3
"""Bounded read-only previous sensor packet protection. No device/boot mutation."""
import hashlib,json,pathlib,stat,sys
BASE=pathlib.Path('/boot/amp-p029/mpu-sensor-v1')
EXPECTED={'amp-signed.itb': {'bytes': 143872, 'sha256': 'd2d618e79c8083002f22f314ae2e2792a7b4f3803575bedc60e476e54201c9fb'}, 'stage-sensor.cmd': {'bytes': 3081, 'sha256': '3f9cca9313c850f0a1f728a9b55d07a5d7cfc501e2265c32d16c3e015e09c0ed'}, 'SHA256SUMS': {'bytes': 749, 'sha256': '091be4e305b0768ffc7ae83b291e680d32b5806dd85f9747d56605236d1433f4'}, 'preflight-sensor.py': {'bytes': 13261, 'sha256': '26e6a7cd9a197f7453b853519063a1b34e3fba856f9bbd322adff749da5c0154'}, 'sensor.dtb': {'bytes': 308361, 'sha256': '33dc67a8a0199d4d0b313e980657982297155f364509552ecc3a92d650c3064e'}, 'stage-sensor.scr': {'bytes': 3153, 'sha256': '5c19bcb293bd64203ab9999c0bac4dd0d4e8e69115c6e6029c756e6f3aa678ea'}, 'rk3576_amp_health_test.ko': {'bytes': 90096, 'sha256': '57b1d830d6687f4f891bbabc3c9694c976c79ab2fcea3d1671a6289d95dea700'}, 'rk3576_sensor.ko': {'bytes': 144128, 'sha256': '063175ef8c5a12855192f790b64664f2d14fe439415be44ab383479c8ccea69a'}, 'run-sensor-t1-t4.py': {'bytes': 4463, 'sha256': 'b6b1ef7e8db6fc4e8297fc1b3e10f42a8c5fc3b3088de3435dddd7669b75e5a8'}, 'SENSOR.json': {'bytes': 12763, 'sha256': '4bf4748465e184939bacd821e3c6c6491bce738eb35a90abafa901ad2a7991ec'}}
def main():
 if sys.argv[1:]!=['--read-only-protected-v3']:raise SystemExit('explicit read-only identifier required')
 assert BASE.is_dir() and not BASE.is_symlink() and {p.name for p in BASE.iterdir()}==set(EXPECTED),'previous packet members'
 for name,want in sorted(EXPECTED.items()):
  p=BASE/name;before=p.lstat();assert stat.S_ISREG(before.st_mode) and before.st_nlink==1 and not p.is_symlink(),'bounded regular protected file'
  assert before.st_size==want['bytes'] and before.st_size<512*1024 and before.st_uid==0 and before.st_gid==0 and stat.S_IMODE(before.st_mode)==0o644,'protected metadata'
  raw=p.read_bytes();after=p.lstat();assert (before.st_ino,before.st_size,before.st_mtime_ns)==(after.st_ino,after.st_size,after.st_mtime_ns),'changed protected file'
  digest=hashlib.sha256(raw).hexdigest();assert digest==want['sha256'],'previous packet hash'
  print('SENSOR_PROTECTED_FILE',json.dumps({'path':str(p),'bytes':len(raw),'sha256':digest,'resolved':str(p.resolve()),'symlink':False,'uid':after.st_uid,'gid':after.st_gid,'mode':oct(stat.S_IMODE(after.st_mode))}),flush=True)
 print('SENSOR_PREVIOUS_V3_TEN_FILES_READONLY_PASS',flush=True)
if __name__=='__main__':main()
