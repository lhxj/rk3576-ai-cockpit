import pathlib, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
class SensorServiceTest(unittest.TestCase):
 def test_actual_codec_service(self):
  with tempfile.TemporaryDirectory() as tmp:
   exe=pathlib.Path(tmp)/"test"
   subprocess.run(["gcc","-std=c11","-Wall","-Wextra","-Werror","-I"+str(ROOT/"libs/protocol/include"),"-I"+str(ROOT/"rtos/rpmsg"),"-I"+str(ROOT/"rtos/sensor"),str(ROOT/"libs/protocol/src/sensor_v1.c"),str(ROOT/"rtos/rpmsg/sensor_service.c"),str(ROOT/"tests/mpu6050/test_sensor_service.c"),"-o",str(exe)],check=True)
   subprocess.run([str(exe)],check=True)

 def test_actual_c_cpp_wire_compatibility(self):
  with tempfile.TemporaryDirectory() as tmp:
   obj=pathlib.Path(tmp)/"codec.o";exe=pathlib.Path(tmp)/"compat"
   include="-I"+str(ROOT/"libs/protocol/include")
   subprocess.run(["gcc","-std=c11","-Wall","-Wextra","-Werror",include,"-c",str(ROOT/"libs/protocol/src/sensor_v1.c"),"-o",str(obj)],check=True)
   subprocess.run(["g++","-std=c++17","-Wall","-Wextra","-Werror",include,str(ROOT/"libs/protocol/src/message.cpp"),str(ROOT/"tests/mpu6050/test_sensor_compat.cpp"),str(obj),"-o",str(exe)],check=True)
   subprocess.run([str(exe)],check=True)

 def test_actual_endpoint_factory(self):
  with tempfile.TemporaryDirectory() as tmp:
   exe=pathlib.Path(tmp)/"endpoint"
   subprocess.run(["gcc","-std=c11","-Wall","-Wextra","-Werror","-I"+str(ROOT/"tests/mpu6050/sensor_fixture"),"-I"+str(ROOT/"libs/protocol/include"),"-I"+str(ROOT/"rtos/rpmsg"),"-I"+str(ROOT/"rtos/sensor"),str(ROOT/"libs/protocol/src/sensor_v1.c"),str(ROOT/"rtos/rpmsg/sensor_service.c"),str(ROOT/"tests/mpu6050/test_sensor_endpoint.c"),"-o",str(exe)],check=True)
   for mode in range(7):subprocess.run([str(exe),str(mode)],check=True)
