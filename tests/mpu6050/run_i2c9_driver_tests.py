#!/usr/bin/env python3
"""L0 only: apply patch to a temporary source slice and test actual C driver."""
import argparse, pathlib, shutil, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument("--rtos",type=pathlib.Path,required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2]
import sys
sys.path.insert(0,str(root/"scripts/dev"))
from mpu_i2c9_sources import verify_sources
verify_sources(a.rtos)
with tempfile.TemporaryDirectory(prefix="mpu-i2c9-") as name:
 d=pathlib.Path(name)
 for path in ("bsp/rockchip/common/drivers/drv_i2c.c","bsp/rockchip/common/drivers/drv_i2c.h","bsp/rockchip/rk3576-mcu/board/evb/iomux.c","bsp/rockchip/rk3576-mcu/drivers/Kconfig"):
  dst=d/path;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(a.rtos/path,dst)
 for name in ("0001-i2c9-deferred-held-clock.patch","0002-i2c9-deferred-reset-deassert.patch","0003-i2c9-intmux-gate-owner.patch"):
  subprocess.run(["patch","-p1","--batch","--fuzz=0","-i",str(root/"patches/mpu6050"/name)],cwd=d,check=True)
 shutil.copyfile(d/"bsp/rockchip/common/drivers/drv_i2c.c",d/"drv_i2c.c")
 for h in ("rthw.h","drv_pm.h","drv_i2c.h","hal_bsp.h","drv_clock.h","drivers/i2c.h","rtdef.h","iomux.h","hal_base.h"):
  dst=d/h;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_text('#include "fake_i2c9_platform.h"\n')
 for hz in (100,1000):
  binary=d/f"test-{hz}"
  subprocess.run(["cc","-std=c99","-g","-O1","-fsanitize=address,undefined","-fno-omit-frame-pointer",f"-DRT_TICK_PER_SECOND={hz}","-I",str(d),"-I",str(root/"tests/mpu6050"),str(root/"tests/mpu6050/test_i2c9_driver.c"),"-o",str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
 shutil.copyfile(d/"bsp/rockchip/rk3576-mcu/board/evb/iomux.c",d/"board_iomux.c")
 binary=d/"test-iomux"
 subprocess.run(["cc","-std=c99","-g","-O1","-fsanitize=address,undefined","-DRT_TICK_PER_SECOND=100","-I",str(d),"-I",str(root/"tests/mpu6050"),str(root/"tests/mpu6050/test_i2c9_iomux.c"),"-o",str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
 # Verify actual new Kconfig block in a bounded dependency fixture; full SDK graph
 # has a pre-existing LED self-dependency (recorded separately, not bypassed).
 import sys
 sys.path.insert(0,str(a.rtos.resolve()/"tools"))
 import kconfiglib
 text=(d/"bsp/rockchip/rk3576-mcu/drivers/Kconfig").read_text()
 block=text[text.index("config RT_USING_I2C9"):text.index("config RT_USING_TSADC")]
 cfg=d/"Kconfig"
 cfg.write_text('config RT_USING_I2C\n bool "I2C"\nconfig RT_USING_CRU\n bool "CRU"\n'+block)
 k=kconfiglib.Kconfig(str(cfg))
 k.syms["RT_USING_I2C9"].set_value(2);k.syms["MPU_SENSOR_V1_I2C9_OWNERSHIP"].set_value(2)
 assert k.syms["MPU_SENSOR_V1_I2C9_OWNERSHIP"].str_value=="n"
 k.syms["RT_USING_I2C"].set_value(2)
 assert k.syms["RT_USING_I2C9"].str_value=="y" and k.syms["MPU_SENSOR_V1_I2C9_OWNERSHIP"].str_value=="n"
 k.syms["RT_USING_CRU"].set_value(2)
 assert k.syms["MPU_SENSOR_V1_I2C9_OWNERSHIP"].str_value=="y"
 print("PASS new Kconfig dependency fixture (full SDK menu graph remains blocked)")
