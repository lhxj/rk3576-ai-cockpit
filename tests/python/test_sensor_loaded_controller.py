import importlib.util,pathlib,types,unittest
from unittest.mock import patch
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('sensor_loaded',ROOT/'scripts/board/mpu6050/run_sensor_t5_t6_loaded.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class LoadedController(unittest.TestCase):
 def test_exact_psmisc_no_double_dash(self):
  m.END=m.time.monotonic()+10
  with patch.object(m.pathlib.Path,'is_char_device',return_value=True),patch.object(m.subprocess,'run',return_value=types.SimpleNamespace(returncode=1,stdout=b'',stderr=b'')) as invoke:m.all_uid_occupants()
  self.assertEqual(invoke.call_args.args[0],['fuser','/dev/video11','/dev/snd/pcmC0D0c'])
 def test_occupants_and_usage_fail_closed(self):
  m.END=m.time.monotonic()+10
  for result in [types.SimpleNamespace(returncode=0,stdout=b'123',stderr=b''),types.SimpleNamespace(returncode=1,stdout=b'',stderr=b'usage')]:
   with patch.object(m.pathlib.Path,'is_char_device',return_value=True),patch.object(m.subprocess,'run',return_value=result),self.assertRaises(RuntimeError):m.all_uid_occupants()
 def test_source_age_guard(self):
  with patch.object(m.argparse.ArgumentParser,'parse_args',return_value=types.SimpleNamespace(source_age_seconds=391)),self.assertRaises(RuntimeError):m.main()
 def test_actual_gnu_note_shape_and_identity(self):
  import struct
  for name,digest in [('rk3576_sensor','8b450105c25698f4709fc2f00c501d3b55abb8ec'),('rk3576_amp_health_test','0d5716e9ba0f949baff9d68fa1cdffe84ea6e08c')]:
   note=struct.pack('<III',4,20,3)+b'GNU\0'+bytes.fromhex(digest);m.validate_loaded_note(note,name)
   for bad in [note[:-1],note[:16]+bytes(20),struct.pack('<III',4,20,2)+note[12:]]:
    with self.assertRaises(RuntimeError):m.validate_loaded_note(bad,name)
 def test_actual_paired_elf_gnu_notes(self):
  import struct
  paths=[('rk3576_sensor',ROOT/'artifacts/local/sensor-ko-host-v4/module/rk3576_sensor.ko'),('rk3576_amp_health_test',pathlib.Path('/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-system/artifacts/local/system-board-validation-20261004/health-ko-v2/module/rk3576_amp_health_test.ko'))]
  if not all(p.is_file() for _,p in paths):self.skipTest('external matched KO ELF absent; generic UAPI fixture remains active')
  for name,path in paths:
   raw=path.read_bytes();self.assertEqual(raw[:6],b'\x7fELF\x02\x01')
   shoff=struct.unpack_from('<Q',raw,40)[0];ents,num,strings=struct.unpack_from('<HHH',raw,58)
   sections=[struct.unpack_from('<IIQQQQIIQQ',raw,shoff+i*ents) for i in range(num)]
   table=raw[sections[strings][4]:sections[strings][4]+sections[strings][5]]
   found=[]
   for section in sections:
    title=table[section[0]:].split(b'\0',1)[0]
    if title==b'.note.gnu.build-id':found.append(raw[section[4]:section[4]+section[5]])
   self.assertEqual(len(found),1);m.validate_loaded_note(found[0],name)
 def test_owner_ioctl_error_and_generation_guards(self):
  import struct
  fields=[1,1,0,0,1,0,0,0,0];m.validate_link_state(struct.pack('<IIIIQQQQQ',*fields))
  for index,value in [(0,2),(1,0),(2,1),(3,1),(4,0),(7,1),(8,1)]:
   bad=list(fields);bad[index]=value
   with self.assertRaises(RuntimeError):m.validate_link_state(struct.pack('<IIIIQQQQQ',*bad))
 def test_no_insmod_or_reboot(self):
  code=(ROOT/'scripts/board/mpu6050/run_sensor_t5_t6_loaded.py').read_text()
  self.assertNotIn("['insmod'",code);self.assertNotIn("['reboot'",code);self.assertIn('0x80385301',code);self.assertIn('END=time.monotonic()+480',code)
