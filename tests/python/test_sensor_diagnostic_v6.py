import contextlib
import importlib.util
import io
import json
import pathlib
import types
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[2]
def controller(name):
 spec=importlib.util.spec_from_file_location(name,ROOT/'scripts/board/mpu6050'/name)
 module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

class DiagnosticV6(unittest.TestCase):
 def test_actual_fuser_diagnostic_bounded_and_gate_unchanged(self):
  m=controller('run_sensor_t5_t6_loaded_v6.py');m.END=m.time.monotonic()+10
  for rc,stdout,stderr,free in [(1,b'',b'',True),(0,b'123',b'',False),(1,b'',b'usage\n'*1000,False)]:
   result=types.SimpleNamespace(returncode=rc,stdout=stdout,stderr=stderr)
   with patch.object(m.pathlib.Path,'is_char_device',return_value=True),patch.object(m.subprocess,'run',return_value=result),contextlib.redirect_stdout(io.StringIO()) as evidence:
    if free:m.all_uid_occupants()
    else:
     with self.assertRaises(RuntimeError):m.all_uid_occupants()
   line=evidence.getvalue().splitlines()[0]
   row=json.loads(line.split(' ',1)[1]);self.assertEqual(row['returncode'],rc)
   self.assertEqual(row['stderr_bytes'],len(stderr));self.assertLessEqual(len(row['stderr_head']),512)
   self.assertLess(len(line),8192)
 def test_v6_paths_and_frozen_window_gates(self):
  loaded=controller('run_sensor_t5_t6_loaded_v6.py');entry=controller('run_sensor_t5_t6_entry_v6.py');coex=controller('run_sensor_coexistence_v6.py')
  self.assertEqual(str(loaded.APP),'/home/cat/cockpit/mpu-sensor-app-v5')
  self.assertEqual(str(coex.ROOT),str(loaded.APP))
  entry.digest(ROOT/'scripts/board/mpu6050/run_sensor_t5_t6_loaded_v6.py',None,entry.LOADED_SHA)
  code=(ROOT/'scripts/board/mpu6050/run_sensor_coexistence_v6.py').read_text()
  for gate in ["<300000","<420","'--seconds','300'","max(frames)>=8000","512*1024*1024"]:self.assertIn(gate,code)
  self.assertIn("sensor-coexistence-run-v6",code)
 def test_coexistence_too_late_rejected_before_graphics(self):
  m=controller('run_sensor_coexistence_v6.py')
  with patch.object(m.os,'uname',return_value=types.SimpleNamespace(release='6.1.99-rk3576-m0echo-p026')),patch.object(m.Path,'read_text',return_value='amp_test_stage=C mpu_sensor=MPU_SENSOR_COEXISTENCE_V1'),patch.object(m,'health',return_value={'elapsed_ms':'300000'}),patch.object(m,'graphics_env') as graphics,self.assertRaises(RuntimeError):m.main()
  graphics.assert_not_called()
 def test_script_and_app_assets_checked_before_first_command(self):
  m=controller('run_sensor_t5_t6_entry_v6.py');events=[]
  def digest(path,*args,**kwargs):events.append(('digest',path.name))
  def command(args,**kwargs):events.append(('command',args[0]));raise RuntimeError('fixture stop after preflight')
  with patch.object(m,'identity'),patch.object(m,'digest',side_effect=digest),patch.object(m.subprocess,'run',side_effect=command),self.assertRaisesRegex(RuntimeError,'fixture stop'):m.run(100)
  first_command=next(i for i,event in enumerate(events) if event[0]=='command')
  for name in [m.COEX.name,m.LOADED.name,*m.APP_ASSETS]:self.assertIn(('digest',name),events[:first_command])
 def test_v6_actual_native_manifest_pins(self):
  m=controller('run_sensor_t5_t6_entry_v6.py')
  p=ROOT/'artifacts/local/mpu-root-review/rtp-pacing-native/application-build.json'
  if not p.is_file():self.skipTest('root native actual manifest not available')
  manifest=json.loads(p.read_text())
  for name,(size,sha) in m.APP_ASSETS.items():
   self.assertEqual(manifest['files']['apps/cockpit_ui/'+name],{'bytes':size,'sha256':sha})

def load_tests(loader,tests,pattern):
 # Re-run the existing production-function suites against the independent variants.
 for source,original,replacement in [('test_sensor_loaded_controller.py','run_sensor_t5_t6_loaded.py','run_sensor_t5_t6_loaded_v6.py'),('test_sensor_entry.py','run_sensor_t5_t6_entry.py','run_sensor_t5_t6_entry_v6.py')]:
  module=types.ModuleType('v6_'+source);module.__file__=str(ROOT/'tests/python'/source)
  text=(ROOT/'tests/python'/source).read_text().replace(original,replacement)
  exec(compile(text,module.__file__,'exec'),module.__dict__)
  tests.addTests(loader.loadTestsFromModule(module))
 return tests
