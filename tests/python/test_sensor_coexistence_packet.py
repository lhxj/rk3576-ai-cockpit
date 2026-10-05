import importlib.util,pathlib,shutil,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
OUT=ROOT/'artifacts/local/mpu-sensor-coexistence-package-v2'
class SensorCoexistencePacketTest(unittest.TestCase):
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
 def test_previous_protected_metadata_changes_rejected(self):
  import json
  spec=importlib.util.spec_from_file_location('sensor_protected',ROOT/'scripts/board/mpu6050/verify_sensor_protected_v3.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
  rows=[{'path':f'/boot/amp-p029/mpu-sensor-v1/file{x}','bytes':10,'sha256':'a'*64,'resolved':f'/boot/amp-p029/mpu-sensor-v1/file{x}','symlink':False,'uid':0,'gid':0,'mode':'0o644'} for x in range(10)]
  def write(path,values):path.write_text(''.join('SENSOR_PROTECTED_FILE '+json.dumps(x)+'\n' for x in values))
  with tempfile.TemporaryDirectory() as temp:
   before=pathlib.Path(temp)/'before';after=pathlib.Path(temp)/'after';write(before,rows);write(after,rows);self.assertEqual(mod.validate(before,after),10)
   for field,value in [('resolved','/boot/other'),('symlink',True),('uid',1000),('mode','0o666'),('bytes',11),('sha256','b'*64)]:
    changed=[dict(x) for x in rows];changed[0][field]=value;write(after,changed)
    with self.assertRaises(AssertionError):mod.validate(before,after)
