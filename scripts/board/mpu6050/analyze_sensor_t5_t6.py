#!/usr/bin/env python3
"""Host-only bounded analysis of actual observation logs; never promotes human/recovery PASS."""
import argparse,hashlib,json,pathlib,re
MAX=2*1024*1024

def read(path):
 info=path.stat()
 if not path.is_file() or info.st_size>MAX:raise ValueError('bounded regular evidence log required')
 raw=path.read_bytes()
 if b'\0' in raw:raise ValueError('NUL in log; preserve raw evidence, no blanket recovery')
 return raw.decode('utf-8',errors='strict'),hashlib.sha256(raw).hexdigest()
def fields(line,prefix):
 return {k:float(v) if '.' in v else int(v) for k,v in re.findall(r'(\w+)=(-?\d+(?:\.\d+)?)',line[len(prefix):])}
def rows(text,prefix):return [fields(line,prefix) for line in text.splitlines() if line.startswith(prefix)]
def extent(data,key):
 values=[r[key] for r in data if key in r]
 return {'min':min(values),'max':max(values),'last':values[-1]} if values else 'UNKNOWN_NOT_EXPORTED'
def analyze(ui_text,app_text,resources_text):
 ui=rows(ui_text,'SENSOR_UI ');sensor=rows(app_text,'SENSOR_COEX_METRICS ');media=rows(app_text,'APPLICATION_METRICS ')
 static=[json.loads(line[len('SENSOR_STATIC_WINDOW '):]) for line in ui_text.splitlines() if line.startswith('SENSOR_STATIC_WINDOW ')]
 duration=re.findall(r'^COEXISTENCE_DURATION_MS=(\d+)$',app_text,re.M)
 observations=[json.loads(line) for line in resources_text.splitlines() if line.strip()]
 problems=[]
 if not ui or 'SENSOR_UI_EXIT' not in ui_text or 'unsubscribe_confirmed=1' not in ui_text:problems.append('T5 observation/confirmed UNSUB missing')
 if len(static)!=1:problems.append('actual fixed static window result missing')
 if len(static)==1:
  report=static[0]
  if report.get('observations',0)<10 or report.get('accel_norm_g_min',0)<.85 or report.get('accel_norm_g_max',99)>1.15 or any(abs(x)>10 for x in report.get('gyro_mean_dps',[99])):problems.append('static functional tolerance failed')
 if 'T5_DIRECTION_CHANGE_ALLOWED' not in ui_text:problems.append('static window completion missing')
 if len(duration)!=1 or int(duration[0])<300000:problems.append('full simultaneous >=300s duration missing')
 if 'SYSTEM_COEXISTENCE_APPLICATION_PROBE_PASS' not in app_text:problems.append('actual application success marker missing')
 for group,keys in ((sensor,('seq','pubseq')),(media,('capture_frames','encoded_frames','record_packets','vision_frames','audio_frames'))):
  if len(group)<2:problems.append('continuous progress rows missing');continue
  for key in keys:
   if any(a[key]>=b[key] for a,b in zip(group,group[1:])):problems.append(key+' did not strictly progress')
 for group,keys in ((sensor,('sample_errors','send_failures','protocol_errors','linux_malformed','linux_send_failures')),(media,('dqbuf_error','qbuf_error','record_overflow','encoder_error','encoder_overflow','audio_xrun','audio_overflow'))):
  if any(key not in row for row in group for key in keys):problems.append('required sensor/media/audio error fields missing')
  if any(row.get(key,0)!=0 for row in group for key in keys):problems.append('reported sensor/media/audio error')
 if any(row.get('encoder_instances')!=1 for row in media):problems.append('single MPP instance violated')
 if any(row.get('lease_active')!=1 for row in sensor):problems.append('subscription not active')
 if len(observations)<2:problems.append('resource interval observations missing')
 health=[row['rpmsg'] for row in observations if 'rpmsg' in row]
 if len(health)<2:problems.append('continuous actual health observations missing')
 if any(not all(key in row for key in ('timeout','error','pong','last_pong_age_ms')) for row in health):problems.append('required health fields missing')
 if any(int(row.get('last_pong_age_ms',5000))>=5000 for row in health):problems.append('actual health stale')
 if any(int(a.get('pong',-1))>=int(b.get('pong',-1)) for a,b in zip(health,health[1:])):problems.append('actual health PONG did not strictly progress')
 if any(int(row.get('timeout',-1))!=0 or int(row.get('error',-1))!=0 for row in health):problems.append('actual health error')
 preview=rows(app_text,'QT_PREVIEW ')
 if len(preview)!=1 or not preview[0].get('converted') or not preview[0].get('delivered'):problems.append('Qt preview delivery missing')
 return {'status':'OBSERVED_LOG_GATES_PASS' if not problems else 'INSUFFICIENT_OR_FAILED_EVIDENCE','problems':problems,'human_ui_confirmation':'USER_CONFIRMATION_PENDING','default_and_previous_packet_recovery':'PENDING_SEPARATE_35_FILE_VALIDATION','coexistence_duration_ms':int(duration[0]) if len(duration)==1 else None,'ui_rows':len(ui),'static_window':static,'sensor_metric_rows':len(sensor),'sensor_counters':{key:extent(sensor,key) for key in sorted(set().union(*(x.keys() for x in sensor)))},'media_metric_rows':len(media),'media_counters':{key:extent(media,key) for key in sorted(set().union(*(x.keys() for x in media)))},'qt_preview':preview,'ui_merge_counts':rows(app_text,'SENSOR_GUI '),'resource_rows':len(observations),'resource_counters':{key:extent(observations,key) for key in ('cpu_percent_per_core_scale','rss_kib','pss_kib','MemAvailable','CmaFree','threads','fds')},'temperature_mC':{key:extent([row.get('temperature_mC',{}) for row in observations],key) for key in sorted(set().union(*(row.get('temperature_mC',{}).keys() for row in observations)))},'health_last':health[-1] if health else None,'not_exported':{'receive_total':'UNKNOWN; metric observations do not count every packet','I2C_NACK_timeout_classification':'UNKNOWN; sample errors are exported in aggregate','full_window_interval_distribution':'UNKNOWN; use actual M0 bounded periodic/first100 trace separately','stack_high_water':'NOT_MEASURED','one_way_latency':'NOT_COMPUTED; M0/Linux clocks unsynchronized'}}
def main():
 p=argparse.ArgumentParser();p.add_argument('--ui-log',type=pathlib.Path,required=True);p.add_argument('--application-log',type=pathlib.Path,required=True);p.add_argument('--resources',type=pathlib.Path,required=True);a=p.parse_args()
 logs=[read(x) for x in (a.ui_log,a.application_log,a.resources)];result=analyze(*(x[0] for x in logs));result['evidence_hashes']={str(path):item[1] for path,item in zip((a.ui_log,a.application_log,a.resources),logs)};print(json.dumps(result,indent=2,sort_keys=True))
if __name__=='__main__':main()
