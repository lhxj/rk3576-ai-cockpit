"""Synthetic DT contract failures; fixtures are never hardware evidence."""
import copy
from pathlib import Path
import struct
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"scripts/dev"))
from mpu_i2c9_dt import validate
C=lambda *values:struct.pack(">"+"I"*len(values),*values)
def fixture():
 n={"/":{},"/__symbols__":{k:v.encode()+b"\0" for k,v in {"cru":"/cru","xin24m":"/xin","uart5m0_xfer":"/pins/uart","i2c9m1_xfer":"/pins/i2c9","gpio1":"/gpio1"}.items()},"/cru":{"phandle":C(2)},"/xin":{"phandle":C(3),"clock-frequency":C(24000000)},"/pins/uart":{"phandle":C(4)},"/pins/i2c9":{"phandle":C(5),"rockchip,pins":C(1,13,10,6,1,12,10,6)},"/gpio1":{"phandle":C(8),"#gpio-cells":C(2)},"/mcu-amp":{"compatible":b"rockchip,mcu-amp\0","clocks":C(2,207,2,208,2,209,2,139,2,150),"pinctrl-0":C(4)},"/i2c@2ae80000":{"reg":C(0,0x2ae80000,0,0x1000),"status":b"disabled\0"},"/camera":{"status":b"okay\0"},"/rpmsg@47d00000":{"rockchip,link-id":C(4)},"/reserved-memory":{"reg":C(0x47800000)},"/spi1":{"status":b"disabled\0","pinctrl-0":C(5)},"/sdmmc1":{"status":b"disabled\0","pinctrl-0":C(5)}}
 t=copy.deepcopy(n);t["/mcu-amp"].update({"clocks":C(2,207,2,208,2,209,2,139,2,150,2,118,2,130),"assigned-clocks":C(2,150,2,130),"assigned-clock-parents":C(0,3),"assigned-clock-rates":C(24000000,24000000),"pinctrl-0":C(4,5)})
 return n,t
class MpuDtTests(unittest.TestCase):
 def test_valid_source_contract(self):n,t=fixture();self.assertFalse(validate(n,t)["deployable"])
 def test_camera_transport_memory_mutation_rejected(self):
  for node,key,value in [("/camera","status",b"disabled\0"),("/rpmsg@47d00000","rockchip,link-id",C(5)),("/reserved-memory","reg",C(0x47900000))]:
   n,t=fixture();t[node][key]=value
   with self.subTest(node=node),self.assertRaises(ValueError):validate(n,t)
 def test_i2c9_enabled_or_child_rejected(self):
  n,t=fixture();n["/i2c@2ae80000"]["status"]=t["/i2c@2ae80000"]["status"]=b"okay\0"
  with self.assertRaises(ValueError):validate(n,t)
  n,t=fixture();n["/i2c@2ae80000/mpu@68"]=t["/i2c@2ae80000/mpu@68"]={}
  with self.assertRaises(ValueError):validate(n,t)
 def test_wrong_clock_parent_rejected(self):
  n,t=fixture();t["/mcu-amp"]["assigned-clock-parents"]=C(0,2)
  with self.assertRaises(ValueError):validate(n,t)
 def test_enabled_spi_sdmmc_conflict_rejected(self):
  for node in ("/spi1","/sdmmc1"):
   n,t=fixture();n[node]["status"]=t[node]["status"]=b"okay\0"
   with self.subTest(node=node),self.assertRaisesRegex(ValueError,"pad conflict"):validate(n,t)
 def test_gpio_hog_conflict_rejected(self):
  for pin in (12,13):
   n,t=fixture();n["/gpio1/hog"]=t["/gpio1/hog"]={"gpio-hog":b"","gpios":C(pin,0)}
   with self.subTest(pin=pin),self.assertRaisesRegex(ValueError,"hog conflict"):validate(n,t)
 def test_gpio_consumer_conflict_rejected(self):
  n,t=fixture();n["/consumer"]=t["/consumer"]={"enable-gpios":C(8,12,0)}
  with self.assertRaisesRegex(ValueError,"consumer conflict"):validate(n,t)
if __name__=="__main__":unittest.main()
