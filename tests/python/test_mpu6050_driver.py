import pathlib, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
class MpuDriverTest(unittest.TestCase):
 def test_actual_driver_fake(self):
  with tempfile.TemporaryDirectory() as temp:
   exe=pathlib.Path(temp)/"test"
   subprocess.run(["gcc","-std=c11","-Wall","-Wextra","-Werror","-I"+str(ROOT/"rtos/sensor"),str(ROOT/"rtos/sensor/mpu6050.c"),str(ROOT/"tests/mpu6050/test_mpu6050.c"),"-o",str(exe)],check=True)
   subprocess.run([str(exe)],check=True)

 def test_actual_sensor_task_ticks(self):
  for hz in (100,1000):
   with tempfile.TemporaryDirectory() as temp:
    exe=pathlib.Path(temp)/"task"
    subprocess.run(["gcc","-std=c11","-Wall","-Wextra","-Werror",f"-DRT_TICK_PER_SECOND={hz}","-I"+str(ROOT/"tests/mpu6050/sensor_fixture"),"-I"+str(ROOT/"rtos/sensor"),str(ROOT/"rtos/sensor/mpu6050.c"),str(ROOT/"tests/mpu6050/test_sensor_task.c"),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
