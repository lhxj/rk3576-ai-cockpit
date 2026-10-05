#!/usr/bin/env python3
"""Host-only bounded UART/RPMsg evidence correlation. Never subtract remote/local clocks."""
import argparse,json,pathlib,re
FIELDS=('ax','ay','az','temp','gx','gy','gz')
def read(path):
 assert path.is_file() and not path.is_symlink() and path.stat().st_size<=1024*1024,'bounded regular log'
 return path.read_text(errors='replace').splitlines()
def compare(uart,linux):
 raw={};config=[];samples=[];exits=[];groups=[];current=[]
 # Fixed RT_CONSOLEBUF_SIZE=128 uses vsnprintf size127 and emits clamped127
 # bytes including NUL. Only recover the observed complete raw marker after
 # this exact boundary; never remove NUL within a value or invent a newline.
 text='\n'.join(read(uart));assert text.count('MPU_CONFIG ')==1,'one configuration marker'
 text=text[text.index('MPU_CONFIG '):];boundary='\x00MPU_RAW '
 nul_count=text.count('\x00');recovered=text.count(boundary)
 assert nul_count==recovered and recovered<=1,'unrecognized UART NUL/truncation'
 if recovered:text=text.replace(boundary,'\nMPU_RAW ')
 for line in text.splitlines():
  if line.startswith('MPU_CONFIG '):config.append(dict(x.split('=',1) for x in line.split()[1:]))
  if line.startswith('MPU_RAW '):
   row=dict(x.split('=',1) for x in line.split()[1:]);key=(int(row['seq']),int(row['m0_ms']),int(row['config'],16))
   assert set(row)=={'n','seq','m0_ms','config',*FIELDS},'incomplete raw fields'
   values=tuple(int(row[x]) for x in FIELDS);assert all(-32768<=x<=32767 for x in values),'raw int16 range'
   assert key not in raw,'duplicate M0 trace';raw[key]=values
 assert len(config)==1 and len(raw)==100,'one readback and100 real trace lines required'
 c=config[0];assert tuple(int(c[x],16) for x in ('who','power','accel_fs','gyro_fs','dlpf','divider','config'))==(0x68,1,0,0,3,49,0x10331),'readback config'
 epoch=int(c['epoch'].replace(':',''),16);assert epoch,'remote epoch'
 for line in read(linux):
  if line.startswith('SENSOR_SAMPLE '):
   sample=json.loads(line[len('SENSOR_SAMPLE '):]);samples.append(sample);current.append(sample)
  if line.startswith('SENSOR_EXIT '):
   exits.append(dict(x.split('=',1) for x in line.split()[1:]));groups.append(current);current=[]
 assert len(samples)==200 and len(exits)==2,'two100-sample bounded clients required'
 assert all(x['samples']=='100' and x['unsubscribe_confirmed']=='1' and x['protocol_errors']=='0' for x in exits),'normal unsubscribe'
 assert not current and len(groups)==2 and all(len(g)==100 for g in groups),'two complete client lifecycle groups'
 subscriptions={x['subscription'] for x in samples}
 assert all(len({x['subscription'] for x in g})==1 and g[0]['subscription']!=0 for g in groups),'subscription changed inside client'
 # Subscription IDs are scoped to session; independent clients may both use1.
 # This probe did not log session IDs, so do not claim distinct-session proof.
 matched=[]
 for s in samples:
  assert s['remote_epoch']==epoch and s['config_id']==0x10331 and s['protocol_errors']==0,'epoch/config/protocol'
  key=(s['sample_seq'],s['m0_ms'],s['config_id'])
  if key in raw:
   assert tuple(s['accel'])+(s['temp'],)+tuple(s['gyro'])==raw[key],'raw mismatch'
   matched.append(s['sample_seq'])
 assert len(matched)>=10,'insufficient shared raw evidence; do not infer correspondence'
 assert all(samples[i]['sample_seq']>samples[i-1]['sample_seq'] for i in range(1,len(samples))),'old/duplicate sample across clients'
 return {'raw_lines':len(raw),'linux_samples':len(samples),'matched_raw':len(matched),'matched_seq_min':min(matched),'matched_seq_max':max(matched),'subscriptions':sorted(subscriptions),'client_groups':len(groups),'session_identity':'NOT_LOGGED; subscription ids are session scoped','remote_epoch':epoch,'clock_latency':'NOT_COMPUTED','config_tail_truncated':bool(recovered),'raw_marker_boundary_recovered':recovered,'observed_config':c,'result':'RAW_INTERSECTION_PASS'}
def main():
 p=argparse.ArgumentParser();p.add_argument('m0',type=pathlib.Path);p.add_argument('linux',type=pathlib.Path);a=p.parse_args();print(json.dumps(compare(a.m0,a.linux),sort_keys=True))
if __name__=='__main__':main()
