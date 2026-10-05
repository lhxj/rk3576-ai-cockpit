import contextlib,io,pathlib,types,unittest
from unittest.mock import patch
ROOT=pathlib.Path(__file__).resolve().parents[2]
def load():
 m=types.ModuleType('sensor_controller');m.__name__='fixture'
 exec(compile((ROOT/'scripts/board/mpu6050/run_sensor_t5_t6.py.in').read_text(),'<actual controller>','exec'),m.__dict__);return m
class SensorCoexistenceController(unittest.TestCase):
 def test_static_exact_window_and_bias_preserved(self):
  m=load();w=m.StaticWindow()
  with contextlib.redirect_stdout(io.StringIO()) as out:
   for t in range(1,27):w.feed(f'SENSOR_UI elapsed_s={t} valid=1 ax=0 ay=0 az=1 gx=-6.6 gy=1.2 gz=0.1')
  self.assertEqual(len(w.rows),15);self.assertTrue(w.done);self.assertAlmostEqual(sum(x[1][0] for x in w.rows)/len(w.rows),-6.6);self.assertIn('T5_DIRECTION_CHANGE_ALLOWED',out.getvalue())
 def test_static_outside_window_ignored(self):
  m=load();w=m.StaticWindow()
  with contextlib.redirect_stdout(io.StringIO()):
   for t in range(27):w.feed(f'SENSOR_UI elapsed_s={t} valid=1 ax=0 ay=0 az={1 if 10<=t<25 else 5} gx=0 gy=0 gz=0')
  self.assertTrue(w.done)
 def test_bad_static_fails_before_direction(self):
  m=load();w=m.StaticWindow()
  with contextlib.redirect_stdout(io.StringIO()) as out:
   with self.assertRaises(RuntimeError):
    for t in range(26):w.feed(f'SENSOR_UI elapsed_s={t} valid=1 ax=0 ay=0 az=1 gx=11 gy=0 gz=0')
  self.assertNotIn('T5_DIRECTION_CHANGE_ALLOWED',out.getvalue())
 def test_codec_old_allowed_new_rejected(self):
  m=load();text='[ 5.00] ES8323 x -6\n[ 20.00] unrelated\n';cursor=m.codec_cursor(text);m.codec_check(text,cursor)
  with contextlib.redirect_stdout(io.StringIO()),self.assertRaises(RuntimeError):m.codec_check(text+'[ 20.01] ES8323 x -6\n',cursor)
 def test_codec_baseline_before_spawn(self):
  m=load();events=[]
  def snapshot():events.append('snapshot');return '[ 20.00] baseline\n'
  def spawn(*a,**kw):events.append('spawn');raise OSError('fixture no process')
  with patch.object(m,'codec_snapshot',snapshot),patch.object(m.subprocess,'Popen',spawn),contextlib.redirect_stdout(io.StringIO()),self.assertRaises(OSError):m.run_process([],1,{},True)
  self.assertEqual(events,['snapshot','spawn'])
 def test_initial_health_error_never_waited(self):
  m=load();bad='bound=0 phase=INIT hello_ack=0 timeout=1 error=0 pong=0 last_pong_age_ms=0'
  with patch.object(m.pathlib.Path,'read_text',return_value=bad),self.assertRaises(RuntimeError):m.health(initial=True)
 def test_initial_health_unready_can_wait(self):
  m=load();pending='bound=0 phase=INIT hello_ack=0 timeout=0 error=0 pong=0 last_pong_age_ms=0'
  with patch.object(m.pathlib.Path,'read_text',return_value=pending):self.assertEqual(m.health(initial=True)['pong'],'0')
 def test_all_uid_fuser_fail_closed(self):
  m=load();m.END=m.time.monotonic()+10
  with patch.object(m.pathlib.Path,'is_char_device',return_value=True),patch.object(m.subprocess,'run',return_value=types.SimpleNamespace(returncode=0,stdout=b'123',stderr=b'')),self.assertRaises(RuntimeError):m.all_uid_occupants()
if __name__=='__main__':unittest.main()
