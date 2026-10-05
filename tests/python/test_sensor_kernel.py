import pathlib, subprocess, tempfile, unittest, sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[2]/"scripts/amp"))
from validate_host_proposal import fdt, cells
ROOT=pathlib.Path(__file__).resolve().parents[2]
class SensorKernelTest(unittest.TestCase):
 def test_actual_module_factory(self):
  with tempfile.TemporaryDirectory() as tmp:
   exe=pathlib.Path(tmp)/"kernel"
   subprocess.run(["gcc","-std=c11","-D_DEFAULT_SOURCE","-Wall","-Wextra","-Wno-unused-parameter","-Wno-sign-compare","-Werror","-I"+str(ROOT/"tests/mpu6050/sensor_kernel_fixture"),str(ROOT/"tests/mpu6050/test_sensor_kernel.c"),"-o",str(exe)],check=True)
   subprocess.run([str(exe)],check=True)

 def test_pinned_real_dt_contract(self):
  dt=ROOT/"artifacts/local/mpu-i2c9-owner-dt-final/mpu-i2c9-candidate.dtb"
  if not dt.exists():self.skipTest("external validated candidate DT not available")
  import hashlib
  self.assertEqual(hashlib.sha256(dt.read_bytes()).hexdigest(),"33dc67a8a0199d4d0b313e980657982297155f364509552ecc3a92d650c3064e")
  nodes=fdt(dt.read_bytes());self.assertEqual(nodes["/i2c@2ae80000"]["status"],b"disabled\0")
  self.assertNotIn("/i2c@4ae80000",nodes)
  pin=cells(nodes["/mcu-amp"]["pinctrl-0"])[1]
  props=next(v for v in nodes.values() if cells(v.get("phandle",b""))==(pin,))
  self.assertEqual(cells(props["rockchip,pins"])[0:3],(1,13,10));self.assertEqual(cells(props["rockchip,pins"])[4:7],(1,12,10))
