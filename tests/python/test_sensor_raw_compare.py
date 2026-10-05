import importlib.util,json,pathlib,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('raw_compare',ROOT/'scripts/board/mpu6050/compare_sensor_raw.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
class RawCompareTest(unittest.TestCase):
 def test_actual_parser_intersection_and_rejections(self):
  with tempfile.TemporaryDirectory() as tmp:
   uart=pathlib.Path(tmp)/'uart';linux=pathlib.Path(tmp)/'linux'
   uart.write_text('MPU_CONFIG epoch=00000000:0000007b who=68 power=01 accel_fs=00 gyro_fs=00 dlpf=03 divider=31 config=00010331\n'+''.join(f'MPU_RAW n={i} seq={i} m0_ms={i*50} ax=-32768 ay=32767 az=16384 temp=-340 gx=-1 gy=0 gz=1 config=00010331\n' for i in range(1,101)))
   rows=[{'remote_epoch':123,'subscription':1 if i<=102 else 2,'sample_seq':i,'publish_seq':i,'m0_ms':i*50,'accel':[-32768,32767,16384],'temp':-340,'gyro':[-1,0,1],'config_id':0x10331,'protocol_errors':0} for i in range(3,203)]
   def write():linux.write_text(''.join(''.join('SENSOR_SAMPLE '+json.dumps(x)+'\n' for x in rows[start:start+100])+'SENSOR_EXIT samples=100 unsubscribe_confirmed=1 protocol_errors=0\n' for start in (0,100)))
   write();self.assertEqual(mod.compare(uart,linux)['matched_raw'],98)
   # Exact observed framing: config tail at127-byte print limit ends NUL,
   # followed immediately by complete raw1 marker. IDs are session scoped.
   original=uart.read_text();uart.write_text(original.replace('config=00010331\n','config=00010331 odr_target_hz=20 \x00',1))
   for row in rows:row['subscription']=3
   write();result=mod.compare(uart,linux)
   self.assertTrue(result['config_tail_truncated']);self.assertEqual(result['matched_raw'],98)
   broken=uart.read_text().replace('ax=-32768','ax=\x0032768',1);uart.write_text(broken)
   with self.assertRaisesRegex(AssertionError,'NUL'):mod.compare(uart,linux)
   uart.write_text(original.replace('config=00010331\n','config=00010331 odr_target_hz=20 \x00',1))
   rows[0]['temp']=0;write()
   with self.assertRaisesRegex(AssertionError,'raw mismatch'):mod.compare(uart,linux)
   rows[0]['temp']=-340;rows[0]['remote_epoch']=124;write()
   with self.assertRaisesRegex(AssertionError,'epoch'):mod.compare(uart,linux)
   rows[0]['remote_epoch']=123
   for row in rows:row['sample_seq']+=1000
   write()
   with self.assertRaisesRegex(AssertionError,'insufficient'):mod.compare(uart,linux)

 def test_actual_saved_board_bytes(self):
  m0=ROOT/'artifacts/local/mpu-root-review/sensor-live-m0-snapshot.log';linux=ROOT/'artifacts/local/mpu-root-review/sensor-t1-t4-live.log'
  if not m0.exists() or not linux.exists():self.skipTest('external actual raw logs not shipped with source CI')
  result=mod.compare(m0,linux);self.assertEqual((result['raw_lines'],result['linux_samples'],result['matched_raw']),(100,200,100));self.assertTrue(result['config_tail_truncated'])
