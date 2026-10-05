"""Actual recovery validator rejects incorrect environment/remaining work."""
import ast,importlib.util,json,pathlib,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('probe_recovery',ROOT/'scripts/board/mpu6050/verify_resource_cold_recovery.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
class RecoveryTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);folder=pathlib.Path(self.tmp.name)
  self.before=folder/'before';self.after=folder/'after'
  self.rows=['FILE '+json.dumps({'path':'/boot/file'+str(i),'bytes':1,'sha256':'abc'}) for i in range(25)]
  self.environment=['UNAME Linux board 6.1.99-rk3576 #8','BOOT_IDENTITY ["root=/dev/mmcblk0p3", "boot_part=2"]','RPMSG_DEVICES []','RPMSG_MODULES []','PROJECT_OCCUPANTS []']
  self.before.write_text('\n'.join(self.rows));self.write()
 def write(self):self.after.write_text('\n'.join(self.rows+self.environment))
 def test_valid(self):self.assertEqual(mod.validate(self.before,self.after),25)
 def test_wrong_root_or_boot(self):
  original=self.environment[1]
  for wrong in ('root=/dev/mmcblk0p4','boot_part=3'):
   self.environment[1]=original.replace('root=/dev/mmcblk0p3' if wrong.startswith('root') else 'boot_part=2',wrong);self.write()
   with self.assertRaises(AssertionError):mod.validate(self.before,self.after)
 def test_remaining_probe_module(self):
  for name in ('rk3576_i2c_resource_probe','rk3576_sensor'):
   self.environment[3]='RPMSG_MODULES '+json.dumps([name]);self.write()
   with self.assertRaises(AssertionError):mod.validate(self.before,self.after)
 def test_remaining_project_process(self):
  self.environment[4]='PROJECT_OCCUPANTS ["123 cockpit_ui"]';self.write()
  with self.assertRaises(AssertionError):mod.validate(self.before,self.after)
 def test_remaining_diagnostic_marker(self):
  for marker in ('i2c_resource_probe=I2C_RESOURCE_PROBE_V1','mpu_sensor=MPU_SENSOR_V1'):
   self.environment[1]='BOOT_IDENTITY '+json.dumps(['root=/dev/mmcblk0p3','boot_part=2',marker]);self.write()
   with self.assertRaises(AssertionError):mod.validate(self.before,self.after)
 def test_actual_reader_keeps_sensor_marker(self):
  source=(ROOT/'scripts/board/mpu6050/read_resource_boot_baseline.sh').read_text()
  remote=source.split("<<'REMOTE'\n",1)[1].rsplit('\nREMOTE',1)[0]
  tree=ast.parse(remote)
  calls=[n for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Name) and n.func.id=='print' and n.args and isinstance(n.args[0],ast.Constant) and n.args[0].value=='BOOT_IDENTITY']
  self.assertEqual(len(calls),1)
  expression=ast.Expression(calls[0].args[1]);ast.fix_missing_locations(expression)
  values=json.loads(eval(compile(expression,'actual reader','eval'),{'json':json,'args':['root=/dev/mmcblk0p3','boot_part=2','mpu_sensor=MPU_SENSOR_V1','unused=1']}))
  self.assertIn('mpu_sensor=MPU_SENSOR_V1',values);self.assertNotIn('unused=1',values)
 def test_changed_resolved_or_symlink_rejected(self):
  rows=[json.loads(x[5:]) for x in self.rows]
  for row in rows:row.update(resolved=row['path'],symlink=False)
  baseline=['FILE '+json.dumps(x) for x in rows];self.before.write_text('\n'.join(baseline))
  for key,wrong in (('resolved','/boot/other'),('symlink',True)):
   changed=[dict(x) for x in rows];changed[0][key]=wrong
   self.rows=['FILE '+json.dumps(x) for x in changed];self.write()
   with self.assertRaises(AssertionError):mod.validate(self.before,self.after)
if __name__=='__main__':unittest.main()
