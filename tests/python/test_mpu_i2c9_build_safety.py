"""Offline source identity and SDK isolation guards; no BSP installation needed."""
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"scripts/dev"))
import mpu_i2c9_sources as guard
class MpuBuildSafetyTests(unittest.TestCase):
 def test_source_mismatch_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/"source.c";p.write_text("expected")
   expected=hashlib.sha256(p.read_bytes()).hexdigest()
   guard.verify_files(d,{"source.c":expected})
   p.write_text("changed")
   with self.assertRaisesRegex(ValueError,"hash mismatch"):guard.verify_files(d,{"source.c":expected})
 def test_source_identity_rejected(self):
  with patch.object(guard,"verify_files"),patch.object(guard.subprocess,"check_output",return_value="wrong-base\n"):
   with self.assertRaisesRegex(ValueError,"RTOS base"):guard.verify_sources("untrusted")
 def test_hal_identity_rejected(self):
  expected=__import__("json").loads((ROOT/"patches/mpu6050/source-inputs.json").read_text())["rtos_head"]
  with patch.object(guard,"verify_files"),patch.object(guard.subprocess,"check_output",side_effect=[expected,"wrong-hal"]):
   with self.assertRaisesRegex(ValueError,"HAL base"):guard.verify_sources("rtos","hal")
 def test_hostile_environment_isolated(self):
  env=guard.build_environment("/tmp/task-out","/tmp/toolchain",{"RTT_ROOT":"/frozen","RTT_CC":"hostile","RTT_EXEC_PATH":"/frozen/bin","BSP_ROOT":"/frozen","PKGS_ROOT":"/frozen"})
  self.assertEqual(env["RTT_ROOT"],"/tmp/task-out/rtos")
  self.assertEqual(env["RTT_CC"],"gcc")
  self.assertNotIn("BSP_ROOT",env);self.assertNotIn("PKGS_ROOT",env)
 def test_escaping_sdk_link_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   out=Path(d)/"out";(out/"rtos/bsp/rockchip/common").mkdir(parents=True);(out/"hal").mkdir()
   (out/"rtos/bsp/rockchip/common/hal").symlink_to(out/"hal")
   guard.assert_sdk_links(out)
   (out/"rtos/escape").symlink_to(Path(d)/"outside")
   with self.assertRaisesRegex(ValueError,"escapes output"):guard.assert_sdk_links(out)
if __name__=="__main__":unittest.main()
