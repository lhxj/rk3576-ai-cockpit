import importlib.util,pathlib,sys,unittest
R=pathlib.Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'scripts/dev'))
spec=importlib.util.spec_from_file_location('packet',R/'scripts/dev/prepare_audio_probe_packet.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Packet(unittest.TestCase):
 def test_exact_derivation_retains_original_guards(self):
  old=(R/'tests/fixtures/audio_probe_original_stage.cmd').read_text();new=m.derive_stage(old,43188736,6603260)
  for needle in ['0x40400000','0x4a200000','0x48300000','amp_m0load','amp_test_stage=C','mpu_sensor=MPU_SENSOR_COEXISTENCE_V1','0xffffffffffffffff','0x23200','0x4b489']:
   self.assertEqual(old.count(needle),new.count(needle))
  self.assertIn('audio_probe=AUDIO_PROBE_DIAGNOSTIC_D1',new);self.assertIn('/amp-p029/mpu-sensor-coexistence-v1/amp-signed.itb',new)
  self.assertEqual(old.count('exit 1')+2,new.count('exit 1'))
  commands=m.validate_storage(new);self.assertEqual(sum(c[1]=='0:3' for c in commands),6)
  self.assertIn('/home/cat/cockpit/audio-probe-diagnostic-d1/boot/Image',new)
  self.assertIn('booti 0x40400000 0x4a200000:'+format(6603260,'x')+' 0x48300000',new)
 def test_command_tamper_rejected(self):
  with self.assertRaises(ValueError):m.derive_stage('different',43188736,6603260)

 def test_old_v2_wrong_partition_rejected(self):
  correct=m.derive_stage((R/'tests/fixtures/audio_probe_original_stage.cmd').read_text(),43188736,6603260)
  # Regression: v2 moved filenames but left Linux size/load on boot p2.
  flawed=correct.replace('0:${audio_root_part}','0:${p029_part}')
  with self.assertRaises(ValueError):m.validate_storage(flawed)
 def test_linux_and_fit_partition_negative(self):
  correct=m.derive_stage((R/'tests/fixtures/audio_probe_original_stage.cmd').read_text(),43188736,6603260)
  for wrong in [correct.replace('size mmc 0:${audio_root_part}', 'size mmc 0:${p029_part}',1),correct.replace('size mmc 0:${p029_part} /amp-p029','size mmc 0:${audio_root_part} /amp-p029'),correct.replace('rootfs audio_root_part','boot audio_root_part'),correct.replace('amp_m0load /amp-p029/mpu-sensor-coexistence-v1/amp-signed.itb','amp_m0load '+m.DEST+'/amp-signed.itb')]:
   with self.assertRaises(ValueError):m.validate_storage(wrong)
