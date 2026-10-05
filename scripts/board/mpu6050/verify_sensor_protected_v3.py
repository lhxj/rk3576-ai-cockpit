#!/usr/bin/env python3
"""Host-only exact previous packet metadata comparison."""
import argparse,json,pathlib
FIELDS=('bytes','sha256','resolved','symlink','uid','gid','mode')
def rows(path):
 values=[json.loads(x[len('SENSOR_PROTECTED_FILE '):]) for x in path.read_text().splitlines() if x.startswith('SENSOR_PROTECTED_FILE ')]
 assert len(values)==10 and len({x['path'] for x in values})==10,'exact ten protected packet records'
 return {x['path']:tuple(x[k] for k in FIELDS) for x in values}
def validate(before,after):assert rows(before)==rows(after),'previous v3 packet hash/metadata changed';return 10
def main():
 p=argparse.ArgumentParser();p.add_argument('before',type=pathlib.Path);p.add_argument('after',type=pathlib.Path);a=p.parse_args();print('SENSOR_PREVIOUS_V3_HASH_METADATA_PASS',validate(a.before,a.after))
if __name__=='__main__':main()
