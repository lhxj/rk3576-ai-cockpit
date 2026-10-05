import pathlib, shutil, subprocess, tarfile, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
class ApplicationExportTest(unittest.TestCase):
 def test_actual_export_configures_and_missing_tools_is_rejected(self):
  with tempfile.TemporaryDirectory(dir=ROOT/"artifacts/local") as tmp:
   base=pathlib.Path(tmp);out=base/"packet"
   subprocess.run(["python3",str(ROOT/"scripts/dev/export_sensor_application.py"),"--output",str(out)],check=True,stdout=subprocess.DEVNULL)
   with tarfile.open(out/"application-source.tar.gz") as archive:archive.extractall(base/"extract")
   src=base/"extract/src"
   positive=subprocess.run(["cmake","-S",str(src),"-B",str(base/"positive"),"-DBUILD_TESTING=ON"],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
   self.assertEqual(positive.returncode,0,positive.stdout)
   shutil.rmtree(src/"tools")
   negative=subprocess.run(["cmake","-S",str(src),"-B",str(base/"negative"),"-DBUILD_TESTING=ON"],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
   self.assertNotEqual(negative.returncode,0)
   self.assertIn("tools/host_smoke/main.cpp",negative.stdout)
