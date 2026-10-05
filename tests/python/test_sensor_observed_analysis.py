import importlib.util,pathlib,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('sensor_analysis',ROOT/'scripts/board/mpu6050/analyze_sensor_t5_t6.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class ObservedAnalysis(unittest.TestCase):
 def test_missing_actual_evidence_never_passes(self):
  result=m.analyze('','','');self.assertEqual(result['status'],'INSUFFICIENT_OR_FAILED_EVIDENCE');self.assertEqual(result['human_ui_confirmation'],'USER_CONFIRMATION_PENDING')
 def test_media_stall_rejected_despite_marker(self):
  line='APPLICATION_METRICS capture_frames=1 encoded_frames=1 record_packets=1 vision_frames=1 audio_frames=1 encoder_instances=1\n'
  result=m.analyze('',line*2+'COEXISTENCE_DURATION_MS=300000\nSYSTEM_COEXISTENCE_APPLICATION_PROBE_PASS\n','')
  self.assertIn('audio_frames did not strictly progress',result['problems'])
 def test_numeric_age_and_negative_gyro(self):
  self.assertEqual(m.rows('SENSOR_UI elapsed_s=25 gx=-6.3 age_ms=20','SENSOR_UI ')[0],{'elapsed_s':25,'gx':-6.3,'age_ms':20})
 def test_missing_health_observations_rejected(self):
  result=m.analyze('','','{"rss_kib":1}\n{"rss_kib":2}\n')
  self.assertIn('continuous actual health observations missing',result['problems'])
 def test_missing_error_fields_rejected(self):
  result=m.analyze('','SENSOR_COEX_METRICS seq=1 pubseq=1\nSENSOR_COEX_METRICS seq=2 pubseq=2\n','')
  self.assertIn('required sensor/media/audio error fields missing',result['problems'])
 def test_incomplete_stale_or_stalled_health_rejected(self):
  import json
  for row,problem in [({'timeout':0,'error':0},'required health fields missing'),({'timeout':0,'error':0,'pong':1,'last_pong_age_ms':5000},'actual health stale'),({'timeout':0,'error':0,'pong':1,'last_pong_age_ms':0},'actual health PONG did not strictly progress')]:
   result=m.analyze('','','\n'.join(json.dumps({'rpmsg':row}) for _ in range(2)))
   self.assertIn(problem,result['problems'])
 def test_nul_log_not_silently_recovered(self):
  with tempfile.TemporaryDirectory() as tmp:
   p=pathlib.Path(tmp)/'log';p.write_bytes(b'actual\0log')
   with self.assertRaises(ValueError):m.read(p)
