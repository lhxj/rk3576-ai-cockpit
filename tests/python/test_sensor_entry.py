import hashlib
import importlib.util
import pathlib
import subprocess
import tempfile
import types
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('sensor_entry', ROOT/'scripts/board/mpu6050/run_sensor_t5_t6_entry.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class Entry(unittest.TestCase):
 def test_digest_actual_file_hash_size_and_symlink(self):
  with tempfile.TemporaryDirectory() as tmp:
   p = pathlib.Path(tmp)/'asset'; p.write_bytes(b'actual')
   sha = hashlib.sha256(b'actual').hexdigest()
   m.digest(p, 6, sha)
   for size, digest in [(5,sha),(6,'0'*64)]:
    with self.assertRaises(RuntimeError): m.digest(p,size,digest)
   link = pathlib.Path(tmp)/'link'; link.symlink_to(p)
   with self.assertRaises(OSError): m.digest(link,6,sha)
 def test_identity_wrong_kernel_module_and_cmdline(self):
  with patch.object(m.os,'geteuid',return_value=0),patch.object(m.os,'uname',return_value=types.SimpleNamespace(release='wrong')):
   with self.assertRaisesRegex(RuntimeError,'paired kernel'): m.identity()
  with patch.object(m.os,'geteuid',return_value=0),patch.object(m.os,'uname',return_value=types.SimpleNamespace(release='6.1.99-rk3576-m0echo-p026')),patch.object(m.pathlib.Path,'read_text',return_value='wrong'):
   with self.assertRaisesRegex(RuntimeError,'boot identity'): m.identity()
  with patch.object(m.os,'geteuid',return_value=0),patch.object(m.os,'uname',return_value=types.SimpleNamespace(release='6.1.99-rk3576-m0echo-p026')),patch.object(m.pathlib.Path,'read_text',return_value='amp_test_stage=C mpu_sensor=MPU_SENSOR_COEXISTENCE_V1'),patch.object(m.pathlib.Path,'exists',return_value=True):
   with self.assertRaisesRegex(RuntimeError,'already loaded'): m.identity()
 def test_health_actual_parser_rejects_error_done_budget_stale(self):
  base = 'bound=1 phase=READY hello_ack=1 pong=3 timeout=0 error=0 elapsed_ms=1000 last_pong_age_ms=0 window_ms=720000'
  with patch.object(m.pathlib.Path,'read_text',return_value=base): self.assertTrue(m.health_ready())
  for bad in [base.replace('error=0','error=1'),base.replace('READY','DONE'),base.replace('720000','900000'),base.replace('age_ms=0','age_ms=5000')]:
   with patch.object(m.pathlib.Path,'read_text',return_value=bad),self.assertRaises(RuntimeError): m.health_ready()
 def test_production_order_and_updated_age(self):
  clock=[10.0]
  calls=[]
  def command(args,**kwargs):
   calls.append(args); clock[0]+=1
   return types.SimpleNamespace(returncode=0)
  with patch.object(m,'identity'),patch.object(m,'digest'),patch.object(m,'health_ready',side_effect=[False,True,True]),patch.object(m.time,'monotonic',side_effect=lambda:clock[0]),patch.object(m.time,'sleep',side_effect=lambda seconds:clock.__setitem__(0,clock[0]+seconds)),patch.object(m.subprocess,'run',side_effect=command),patch.object(m.os,'execv') as execute:
   m.run(100)
  self.assertEqual([call[0] for call in calls],['python3','insmod','insmod'])
  self.assertTrue(calls[1][1].endswith('rk3576_amp_health_test.ko'))
  self.assertTrue(calls[2][1].endswith('rk3576_sensor.ko'))
  self.assertEqual(execute.call_args.args[1][:3],['python3',str(m.LOADED),'--source-age-seconds'])
  self.assertEqual(float(execute.call_args.args[1][3]),103.25)
 def test_hash_failure_blocks_commands(self):
  with patch.object(m,'identity'),patch.object(m,'digest',side_effect=RuntimeError('bad KO hash')),patch.object(m.subprocess,'run') as invoke,self.assertRaises(RuntimeError): m.run(100)
  invoke.assert_not_called()
 def test_preload_deadline_and_source_age_no_handoff(self):
  for age in [-1,391,float('nan')]:
   with self.assertRaises(RuntimeError): m.run(age)
  with patch.object(m.time,'monotonic',side_effect=[0,61]),patch.object(m.subprocess,'run') as invoke,self.assertRaisesRegex(RuntimeError,'preload60s'): m.run(100)
  invoke.assert_not_called()
  with patch.object(m.time,'monotonic',side_effect=[0,2]),self.assertRaisesRegex(RuntimeError,'fresh SOURCE'): m.run(389)
 def test_handshake_deadline_preserves_loaded_refs(self):
  clock=[0.0]; calls=[]
  with patch.object(m,'identity'),patch.object(m,'digest'),patch.object(m,'health_ready',return_value=False),patch.object(m.time,'monotonic',side_effect=lambda:clock[0]),patch.object(m.time,'sleep',side_effect=lambda seconds:clock.__setitem__(0,clock[0]+seconds)),patch.object(m.subprocess,'run',side_effect=lambda args,**kwargs:calls.append(args)),patch.object(m.os,'execv') as execute,self.assertRaisesRegex(RuntimeError,'handshake20s'):
   m.run(100)
  self.assertEqual([call[0] for call in calls],['python3','insmod']);execute.assert_not_called()
 def test_preflight_failure_no_insmod(self):
  with patch.object(m,'identity'),patch.object(m,'digest'),patch.object(m.subprocess,'run',side_effect=subprocess.CalledProcessError(1,['python3'])) as invoke,patch.object(m.os,'execv') as execute,self.assertRaises(subprocess.CalledProcessError): m.run(100)
  self.assertEqual(invoke.call_count,1);execute.assert_not_called()
