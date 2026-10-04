#!/usr/bin/env python3
"""L0 build only. Independent SDK copies; never deploys or invokes resource_ready."""
import argparse, hashlib, json, os, pathlib, shutil, subprocess
p=argparse.ArgumentParser();p.add_argument("--rtos",type=pathlib.Path,required=True);p.add_argument("--hal",type=pathlib.Path,required=True);p.add_argument("--toolchain",type=pathlib.Path,required=True);p.add_argument("--output",type=pathlib.Path,required=True);a=p.parse_args()
from mpu_i2c9_sources import verify_sources, assert_sdk_links, build_environment
inputs=verify_sources(a.rtos,a.hal)
root=pathlib.Path(__file__).resolve().parents[2];out=a.output.resolve()
if not out.is_relative_to(root/"artifacts/local") or out.exists(): raise SystemExit("output must be a NEW artifacts/local subdirectory")
out.mkdir(parents=True)
ignore=shutil.ignore_patterns(".git","build","*.o","*.elf","*.bin","*.map",".sconsign*","*.pyc","__pycache__")
shutil.copytree(a.rtos,out/"rtos",symlinks=True,ignore=ignore);shutil.copytree(a.hal,out/"hal",symlinks=True,ignore=ignore)
link=out/"rtos/bsp/rockchip/common/hal"
if not link.is_symlink(): raise SystemExit("expected source HAL symlink")
link.unlink();link.symlink_to("../../../../hal")
assert_sdk_links(out)
patches=("0001-i2c9-deferred-held-clock.patch","0002-i2c9-deferred-reset-deassert.patch","0003-i2c9-intmux-gate-owner.patch")
for name in patches:
 subprocess.run(["patch","-p1","--batch","--forward","--fuzz=0","-i",str(root/"patches/mpu6050"/name)],cwd=out/"rtos",check=True)
bsp=out/"rtos/bsp/rockchip/rk3576-mcu"
config=(bsp/".config").read_text().replace("# CONFIG_RT_USING_I2C is not set","CONFIG_RT_USING_I2C=y")
config += "\nCONFIG_RT_USING_I2C9=y\nCONFIG_MPU_SENSOR_V1_I2C9_OWNERSHIP=y\n"
(bsp/".config").write_text(config)
# Retain adapter API in a diagnostic build: gc-sections would otherwise remove it.
with (bsp/"rtconfig.py").open("a") as f:f.write("\nLFLAGS += ' -Wl,-u,rockchip_i2c9_resource_ready '\n")
env=build_environment(out,a.toolchain,os.environ)
for label,args in (("config",["--useconfig=.config"]),("build",["-j4"])):
 with (out/f"native-{label}.log").open("w") as log:subprocess.run(["scons",*args],cwd=bsp,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
h=(bsp/"rtconfig.h").read_text()
for symbol in ("RT_USING_I2C","RT_USING_I2C9","MPU_SENSOR_V1_I2C9_OWNERSHIP"):
 if f"#define {symbol}\n" not in h:raise SystemExit(f"config omitted {symbol}")
for i in range(9):
 if f"#define RT_USING_I2C{i}\n" in h:raise SystemExit("unexpected I2C controller")
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
files=(".config","rtconfig.h","rtconfig.py","gcc_link.ld","rtthread.elf","rtthread.bin","rtthread.map")
manifest={"kind":"I2C9_DEFERRED_ADAPTER_DIAGNOSTIC_ONLY","deployable":False,"fit":"NOT_BUILT","source_rtos":str(a.rtos.resolve()),"source_hal":str(a.hal.resolve()),"input_config_sha256":sha(a.rtos/"bsp/rockchip/rk3576-mcu/.config"),"patch_sha256":sha(root/"patches/mpu6050/0001-i2c9-deferred-held-clock.patch"),"files":{name:sha(bsp/name) for name in files},"source_inputs":inputs,"patches":{name:sha(root/"patches/mpu6050"/name) for name in patches},"build_environment":{key:env[key] for key in ("RTT_ROOT","RTT_CC","RTT_EXEC_PATH")}}
for cmd in ("size","nm","readelf"):
 args=[str(a.toolchain/f"arm-none-eabi-{cmd}")]
 if cmd=="readelf":args +=["-l","-S"]
 result=subprocess.run([*args,str(bsp/"rtthread.elf")],capture_output=True,text=True,check=True)
 (out/f"elf-{cmd}.txt").write_text(result.stdout)
(out/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
print(out/"manifest.json")
