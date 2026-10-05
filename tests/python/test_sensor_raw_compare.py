import importlib.util,json,pathlib,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('raw_compare',ROOT/'scripts/board/mpu6050/compare_sensor_raw.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
class RawCompareTest(unittest.TestCase):
 def test_actual_parser_intersection_and_rejections(self):
  with tempfile.TemporaryDirectory() as tmp:
   uart=pathlib.Path(tmp)/'uart';linux=pathlib.Path(tmp)/'linux'
   uart.write_text('MPU_CONFIG epoch=00000000:0000007b who=68 power=01 accel_fs=00 gyro_fs=00 dlpf=03 divider=31 config=00010331\n'+''.join(f'MPU_RAW n={i} seq={i} m0_ms={i*50} ax=-32768 ay=32767 az=16384 temp=-340 gx=-1 gy=0 gz=1 config=00010331\n' for i in range(1,101)))
   rows=[{'remote_epoch':123,'subscription':1 if i<=102 else 2,'sample_seq':i,'publish_seq':i,'m0_ms':i*50,'accel':[-32768,32767,16384],'temp':-340,'gyro':[-1,0,1],'config_id':0x10331,'protocol_errors':0} for i in range(3,203)]
   def write():linux.write_text(''.join('SENSOR_SAMPLE '+json.dumps(x)+'\n' for x in rows)+'SENSOR_EXIT samples=100 unsubscribe_confirmed=1 protocol_errors=0\n'*2)
   write();self.assertEqual(mod.compare(uart,linux)['matched_raw'],98)
   rows[0]['temp']=0;write()
   with self.assertRaisesRegex(AssertionError,'raw mismatch'):mod.compare(uart,linux)
   rows[0]['temp']=-340;rows[0]['remote_epoch']=124;write()
   with self.assertRaisesRegex(AssertionError,'epoch'):mod.compare(uart,linux)
   rows[0]['remote_epoch']=123
   for row in rows:row['sample_seq']+=1000
   write()
   with self.assertRaisesRegex(AssertionError,'insufficient'):mod.compare(uart,linux)
