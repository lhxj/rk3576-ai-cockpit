import importlib.util,pathlib,shutil,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
OUT=ROOT/'artifacts/local/mpu-sensor-package-v3'
class SensorPacketTest(unittest.TestCase):
 def test_actual_packet_and_rejection(self):
  if not OUT.is_dir():self.skipTest('external signed ARM/native package absent; source CI is not package verification')
  spec=importlib.util.spec_from_file_location('sensor_installer',OUT/'install-sensor.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
  mod.validate_packet(OUT/'packet')
  for mode in ('extra','missing','manifest','checksum','fit','symlink'):
   with tempfile.TemporaryDirectory() as temp:
    copy=pathlib.Path(temp)/'packet';shutil.copytree(OUT/'packet',copy)
    if mode=='extra':(copy/'extra').write_text('x')
    elif mode=='missing':(copy/'sensor.dtb').unlink()
    elif mode=='symlink':(copy/'sensor.dtb').unlink();(copy/'sensor.dtb').symlink_to(OUT/'packet/sensor.dtb')
    else:
     name={'manifest':'SENSOR.json','checksum':'SHA256SUMS','fit':'amp-signed.itb'}[mode];p=copy/name;p.write_bytes(p.read_bytes()+b'x')
    with self.assertRaises((RuntimeError,OSError)):mod.validate_packet(copy)
 def test_actual_runner_health_fails_closed(self):
  code=(ROOT/'scripts/board/mpu6050/run_sensor_t1_t4.py.in').read_text().replace('__APP_BYTES__','1').replace('__APP_SHA__','a'*64)
  env={'__name__':'fixture'};exec(compile(code,'runner','exec'),env)
  class FakePath:
   text=''
   def __init__(self,*args):pass
   def read_text(self):return self.text
  env['pathlib'].Path=FakePath
  try:
   for bad in ('bound=0 phase=READY hello_ack=1 pong=3 timeout=0 error=0 last_pong_age_ms=1','bound=1 phase=FAILED timeout=0 error=0','bound=1 phase=READY timeout=1 error=0','bound=1 phase=READY timeout=0 error=1','bound=1 phase=READY timeout=0 error=0 pong=3 last_pong_age_ms=3000'):
    FakePath.text=bad
    with self.assertRaises(RuntimeError):env['health']()
   FakePath.text='bound=1 phase=READY hello_ack=1 pong=3 timeout=0 error=0 last_pong_age_ms=2999';self.assertEqual(env['health']()['pong'],'3')
  finally:env['pathlib'].Path=pathlib.PosixPath

 def test_actual_runner_chunked_lines_and_partial_eof(self):
  import contextlib,io,types,unittest.mock
  code=(ROOT/'scripts/board/mpu6050/run_sensor_t1_t4.py.in').read_text().replace('__APP_BYTES__','1').replace('__APP_SHA__','a'*64)
  env={'__name__':'fixture'};exec(compile(code,'runner','exec'),env)
  class Process:
   pid=123;stdout=io.BytesIO()
   def wait(self,timeout):return 0
   def poll(self):return 0
  class Selector:
   active=True
   def register(self,*args):pass
   def get_map(self):return self.active
   def select(self,*args):return [(types.SimpleNamespace(fd=42,fileobj=Process.stdout),1)]
   def unregister(self,*args):self.active=False
   def close(self):pass
  for parts,ok in (([b'SENSOR_SAMPLE {"sample_',b'seq":1}\n',b''],True),([b'SENSOR_SAMPLE {',b''],False)):
   Process.stdout=io.BytesIO();output=io.StringIO();env['health']=lambda:print('SENSOR_HEALTH phase=READY')
   with unittest.mock.patch.object(env['subprocess'],'Popen',return_value=Process()),unittest.mock.patch.object(env['selectors'],'DefaultSelector',Selector),unittest.mock.patch.object(env['os'],'read',side_effect=parts),contextlib.redirect_stdout(output):
    if ok:env['samples'](1)
    else:
     with self.assertRaisesRegex(RuntimeError,'incomplete'):env['samples'](1)
   if ok:self.assertIn('SENSOR_SAMPLE {"sample_seq":1}\n',output.getvalue())
