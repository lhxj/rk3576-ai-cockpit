import json
import contextlib,importlib.util,io,pathlib,signal,sys,types,unittest
from unittest.mock import patch,Mock
R=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('pa',R/'scripts/board/mpu6050/run_sensor_pa_arbitrated_v3.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
# Identity/cause lines from actual PA16.1 pacmd baseline, no ignored-log dependency.
BASE='  * index: 1\n\tname: <alsa_input.platform-es8388-sound.HiFi__hw_rockchipes8388__source>\n\tdriver: <module-alsa-card.c>\n\tstate: SUSPENDED\n\tsuspend cause: IDLE\n\tproperties:\n\t\tdevice.class = "sound"\n\t\talsa.card = "0"\n\t\talsa.device = "0"\n\t\talsa.name = "dailink-multicodecs ES8323 HiFi-0"\n'
class Arbitration(unittest.TestCase):
 def test_actual_idle_and_zero_and_multiple_causes(self):
  self.assertFalse(m.parse_source(BASE)['user'])
  for state in ['RUNNING','IDLE']:
   self.assertFalse(m.parse_source(BASE.replace('state: SUSPENDED','state: '+state).replace('suspend cause: IDLE','suspend cause: (none)'))['user'])
  self.assertTrue(m.parse_source(BASE.replace('suspend cause: IDLE','suspend cause: USER|IDLE'))['user'])
 def test_unknown_duplicate_identity_rejected(self):
  for text in [BASE.replace('suspend cause: IDLE','suspend cause: UNKNOWN'),BASE+BASE,BASE.replace('alsa.device = "0"','alsa.device = "1"'),BASE.replace('index: 1','index: 2')]:
   with self.assertRaises(RuntimeError):m.parse_source(text)
 def fixture(self,user=False,setfail=False,restorefail=False,restart=False,actor=False,childfail=False,signalerror=False):
  state={'user':user};events=[]
  def pa(program,*args):
   events.append((program,*args))
   if args and args[0]=='suspend-source':
    state['user']=args[-1]=='1'
    if args[-1]=='1' and setfail:raise RuntimeError('setter timeout after mutation')
    if args[-1]=='0' and restorefail:raise RuntimeError('restore failed')
   return 'ignored'
  def snap():return {'user':state['user'],'state':'SUSPENDED','causes':['USER'] if state['user'] else ['IDLE']}
  child=Mock();child.poll.return_value=1 if childfail else 0;child.wait.return_value=1 if childfail else 0
  ident=iter([('cookie',1,2),('new',1,3),('new',1,3)]) if restart else None
  def identity():return next(ident) if ident else ('cookie',1,2)
  def stop(c):
   events.append(('stop',))
   if actor:raise RuntimeError('nested actor still present')
   if signalerror:raise RuntimeError('owned child still present')
  patches=[patch.object(m,'runtime_precheck'),patch.object(m,'server_identity',side_effect=identity),patch.object(m,'snapshot',side_effect=snap),patch.object(m,'pa',side_effect=pa),patch.object(m.subprocess,'Popen',return_value=child),patch.object(m,'stop_owned',side_effect=stop)]
  with contextlib.ExitStack() as stack:
   for p in patches:stack.enter_context(p)
   stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
   if setfail or restorefail or restart or actor or childfail or signalerror:
    with self.assertRaises(RuntimeError):m.arbitrate(10)
   else:m.arbitrate(10)
  return events,state
 def test_idle_set_and_restore_order(self):
  events,state=self.fixture();self.assertFalse(state['user']);self.assertLess(events.index(('stop',)),events.index(('pactl','suspend-source',m.SOURCE,'0')))
 def test_existing_user_never_changed(self):
  events,state=self.fixture(user=True);self.assertTrue(state['user']);self.assertFalse(any('suspend-source' in x for x in events))
 def test_setter_timeout_still_rollback(self):
  events,state=self.fixture(setfail=True);self.assertFalse(state['user']);self.assertIn(('pactl','suspend-source',m.SOURCE,'0'),events)
 def test_child_failure_rollback(self):self.assertFalse(self.fixture(childfail=True)[1]['user'])
 def test_restart_and_live_actor_do_not_blind_restore(self):
  for kwargs in [{'restart':True},{'actor':True},{'signalerror':True}]:
   events,state=self.fixture(**kwargs);self.assertTrue(state['user']);self.assertNotIn(('pactl','suspend-source',m.SOURCE,'0'),events)
 def test_restore_failure_is_failure(self):self.fixture(restorefail=True)
 def test_bounded_actual_harmless_child_cap_and_timeout(self):
  m.END=0
  self.assertEqual(m.bounded([sys.executable,'-c','print("HOST_ONLY")']).strip(),'HOST_ONLY')
  with self.assertRaisesRegex(RuntimeError,'output cap'):m.bounded([sys.executable,'-c','print("x"*70000)'])
  with self.assertRaisesRegex(RuntimeError,'deadline'):m.bounded([sys.executable,'-c','import time;time.sleep(2)'],seconds=.05)
 def test_structured_sink_ignores_dynamic_fields(self):
  text='    index: 0\n name: <sink>\n state: SUSPENDED\n suspend cause: IDLE\n latency: 1 ms\n'
  self.assertEqual(m.sink_signature(text),m.sink_signature(text.replace('1 ms','9 ms')))
 def test_actual_actor_scan_new_v5_blocks_restore(self):
  process=Mock();process.name='3280';cmdline=Mock();cmdline.read_bytes.return_value=b'/home/cat/cockpit/mpu-sensor-app-v5/build/apps/cockpit_ui/sensor_coexistence_probe\0'
  process.__truediv__=Mock(return_value=cmdline)
  child=Mock();child.poll.return_value=0
  with patch.object(m.pathlib.Path,'iterdir',return_value=[process]),patch.object(m.subprocess,'run') as fuser:
   self.assertFalse(m.actors_released())
   with self.assertRaisesRegex(RuntimeError,'restore pending'):m.stop_owned(child)
   fuser.assert_not_called()
 def test_signal_raises(self):
  with self.assertRaisesRegex(RuntimeError,'signal'):m.interrupted(signal.SIGTERM,None)
 def test_stop_owned_sigint_before_resource_proof(self):
  child=Mock();child.poll.return_value=None;m.END=m.time.monotonic()+40
  with patch.object(m,'actors_released',return_value=False),self.assertRaises(RuntimeError):m.stop_owned(child)
  child.send_signal.assert_called_once_with(signal.SIGINT)
if __name__=='__main__':unittest.main()

class StartupDiagnostic(unittest.TestCase):
 def test_missing_source_keeps_gate_and_no_mutation(self):
  out=io.StringIO()
  with patch.object(m,'pa',return_value='  * index: 0\n name: <monitor>\n'),patch.object(m,'bounded',return_value='ES8323 failed'),contextlib.redirect_stdout(out):
   with self.assertRaisesRegex(RuntimeError,'unique exact capture source'):m.snapshot()
  rows=[json.loads(line.split(' ',1)[1]) for line in out.getvalue().splitlines()]
  self.assertEqual([x['section'] for x in rows],['sources','cards','kernel'])
 def test_diagnostic_failure_preserves_original(self):
  with patch.object(m,'pa',return_value=''),patch.object(m,'startup_diagnostic',side_effect=OSError('failed')),contextlib.redirect_stdout(io.StringIO()):
   with self.assertRaisesRegex(RuntimeError,'unique exact capture source'):m.snapshot()
 def test_json_cap_and_kernel_selection(self):
  out=io.StringIO()
  with patch.object(m,'bounded',side_effect=['x'*100000,'irrelevant\nES8323 '+('界'*20000)]),contextlib.redirect_stdout(out):m.startup_diagnostic('')
  lines=out.getvalue().splitlines();self.assertLess(sum(len(x) for x in lines),65536)
  for line in lines:self.assertLessEqual(len(line.split(' ',1)[1]),16384);json.loads(line.split(' ',1)[1])
  self.assertNotIn('irrelevant',out.getvalue())
