"""Read-only exact source preflight shared by MPU I2C9 Host tools."""
import hashlib, json, pathlib, subprocess
ROOT=pathlib.Path(__file__).resolve().parents[2]
def verify_files(directory, hashes):
 directory=pathlib.Path(directory).resolve()
 for name,expected in hashes.items():
  path=directory/name
  if not path.resolve().is_relative_to(directory):raise ValueError(f"source symlink escapes: {name}")
  if hashlib.sha256(path.read_bytes()).hexdigest()!=expected:raise ValueError(f"source hash mismatch: {name}")
def verify_sources(rtos,hal=None):
 manifest=json.loads((ROOT/"patches/mpu6050/source-inputs.json").read_text())
 verify_files(rtos,manifest["rtos_files"])
 actual=subprocess.check_output(["git","-C",str(rtos),"rev-parse","HEAD"],text=True).strip()
 if actual!=manifest["rtos_head"]:raise ValueError("RTOS base identity mismatch")
 if hal is not None:
  verify_files(hal,manifest["hal_files"])
  actual=subprocess.check_output(["git","-C",str(hal),"rev-parse","HEAD"],text=True).strip()
  if actual!=manifest["hal_head"]:raise ValueError("HAL base identity mismatch")
 return manifest
def assert_sdk_links(output):
 output=pathlib.Path(output).resolve()
 if (output/"rtos/bsp/rockchip/common/hal").resolve()!=output/"hal":raise ValueError("HAL link is not isolated")
 for tree in (output/"rtos",output/"hal"):
  for path in tree.rglob("*"):
   if path.is_symlink() and not path.resolve().is_relative_to(output):raise ValueError(f"SDK link escapes output: {path}")
def build_environment(output,toolchain,parent):
 env=dict(parent)
 # Reject all inherited RT-Thread location and toolchain overrides.
 for name in ("RTT_ROOT","RTT_CC","RTT_EXEC_PATH","BSP_ROOT","PKGS_ROOT"):
  env.pop(name,None)
 env.update(RTT_ROOT=str(pathlib.Path(output).resolve()/"rtos"),RTT_CC="gcc",RTT_EXEC_PATH=str(pathlib.Path(toolchain).resolve()))
 return env
