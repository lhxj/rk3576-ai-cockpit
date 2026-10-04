#!/usr/bin/env python3
"""Host compares bounded baseline/recovery logs; does not perform shutdown."""
import argparse,json
from pathlib import Path
def read(path):
 lines=path.read_text().splitlines()
 rows=[json.loads(x[5:]) for x in lines if x.startswith('FILE ')]
 return {x['path']:(x['bytes'],x['sha256']) for x in rows},lines
def validate(before_path,after_path):
 before,_=read(before_path);after,lines=read(after_path)
 assert len(before)>=18 and before==after,'default/frozen/SI hashes differ or unreadable'
 assert any(x.startswith('UNAME') and '6.1.99-rk3576 ' in x for x in lines),'wrong default kernel'
 assert 'RPMSG_DEVICES []' in lines,'RPMsg endpoint remains'
 assert 'RPMSG_MODULES []' in lines,'RPMsg/echo/health/probe module remains'
 assert 'PROJECT_OCCUPANTS []' in lines,'project process remains'
 identity=json.loads(next(x[len('BOOT_IDENTITY '):] for x in lines if x.startswith('BOOT_IDENTITY ')))
 assert 'root=/dev/mmcblk0p3' in identity and 'boot_part=2' in identity,'wrong root/boot'
 assert not any(x.startswith(('amp_test_stage=','amp_health_test=','i2c_resource_probe=')) for x in identity),'diagnostic marker remains'
 return len(before)
def main():
 p=argparse.ArgumentParser();p.add_argument('before',type=Path);p.add_argument('after',type=Path);a=p.parse_args()
 print('RESOURCE_PROBE_COLD_DEFAULT_RECOVERY_AND_BASELINE_HASHES_PASS',validate(a.before,a.after))
if __name__=='__main__':main()
