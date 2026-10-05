#!/usr/bin/env python3
"""Host-only bounded UART/RPMsg evidence correlation. Never subtract remote/local clocks."""
import argparse,json,pathlib,re
FIELDS=('ax','ay','az','temp','gx','gy','gz')
def read(path):
 assert path.is_file() and not path.is_symlink() and path.stat().st_size<=1024*1024,'bounded regular log'
 return path.read_text(errors='replace').splitlines()
def compare(uart,linux):
 raw={};config=[];samples=[];exits=[]
 for line in read(uart):
  if line.startswith('MPU_CONFIG '):config.append(dict(x.split('=',1) for x in line.split()[1:]))
  if line.startswith('MPU_RAW '):
   row=dict(x.split('=',1) for x in line.split()[1:]);key=(int(row['seq']),int(row['m0_ms']),int(row['config'],16))
   assert key not in raw,'duplicate M0 trace';raw[key]=tuple(int(row[x]) for x in FIELDS)
 assert len(config)==1 and len(raw)==100,'one readback and100 real trace lines required'
 c=config[0];assert tuple(int(c[x],16) for x in ('who','power','accel_fs','gyro_fs','dlpf','divider','config'))==(0x68,1,0,0,3,49,0x10331),'readback config'
 epoch=int(c['epoch'].replace(':',''),16);assert epoch,'remote epoch'
 for line in read(linux):
  if line.startswith('SENSOR_SAMPLE '):samples.append(json.loads(line[len('SENSOR_SAMPLE '):]))
  if line.startswith('SENSOR_EXIT '):exits.append(dict(x.split('=',1) for x in line.split()[1:]))
 assert len(samples)==200 and len(exits)==2,'two100-sample bounded clients required'
 assert all(x['samples']=='100' and x['unsubscribe_confirmed']=='1' and x['protocol_errors']=='0' for x in exits),'normal unsubscribe'
 subscriptions={x['subscription'] for x in samples};assert len(subscriptions)==2,'new subscription required'
 matched=[]
 for s in samples:
  assert s['remote_epoch']==epoch and s['config_id']==0x10331 and s['protocol_errors']==0,'epoch/config/protocol'
  key=(s['sample_seq'],s['m0_ms'],s['config_id'])
  if key in raw:
   assert tuple(s['accel'])+(s['temp'],)+tuple(s['gyro'])==raw[key],'raw mismatch'
   matched.append(s['sample_seq'])
 assert len(matched)>=10,'insufficient shared raw evidence; do not infer correspondence'
 assert all(samples[i]['sample_seq']>samples[i-1]['sample_seq'] for i in range(1,len(samples))),'old/duplicate sample across clients'
 return {'raw_lines':len(raw),'linux_samples':len(samples),'matched_raw':len(matched),'matched_seq_min':min(matched),'matched_seq_max':max(matched),'subscriptions':sorted(subscriptions),'remote_epoch':epoch,'clock_latency':'NOT_COMPUTED','result':'RAW_INTERSECTION_PASS'}
def main():
 p=argparse.ArgumentParser();p.add_argument('m0',type=pathlib.Path);p.add_argument('linux',type=pathlib.Path);a=p.parse_args();print(json.dumps(compare(a.m0,a.linux),sort_keys=True))
if __name__=='__main__':main()
