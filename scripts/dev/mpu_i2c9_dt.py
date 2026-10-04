#!/usr/bin/env python3
"""L0 only: derive candidate DT and verify narrowly scoped ownership changes."""
import argparse, hashlib, json, pathlib, subprocess, sys
ROOT=pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"scripts/amp"))
from validate_host_proposal import fdt, cells
from mpu_i2c9_sources import verify_files
BASE_SHA="dd68818b7fd27e9abc56522d7c4782adb0b1dd49d274fe1daa75a32ad3366fbf"
AMP="/mcu-amp";I2C="/i2c@2ae80000"
ALLOWED={"clocks","assigned-clocks","assigned-clock-parents","assigned-clock-rates","pinctrl-0"}
def require(value,reason):
 if not value:raise ValueError(reason)
def symbol(nodes,name):return nodes["/__symbols__"][name].rstrip(b"\0").decode()
def phandle(nodes,name):return cells(nodes[symbol(nodes,name)]["phandle"])[0]
def enabled(nodes,path):
 while path:
  if nodes.get(path,{}).get("status",b"okay\0") not in (b"okay\0",b"ok\0"):return False
  path=path.rpartition("/")[0]
 return True
def validate(base,target):
 require(set(base)==set(target),"overlay changed node topology")
 for path,properties in base.items():
  for key in set(properties)|set(target[path]):
   if path==AMP and key in ALLOWED:continue
   require(properties.get(key)==target[path].get(key),f"unexpected DT change: {path}:{key}")
 cru=phandle(base,"cru");xin=phandle(base,"xin24m");uart=phandle(base,"uart5m0_xfer");pin=phandle(base,"i2c9m1_xfer")
 require(base[I2C]["status"]==b"disabled\0" and not any(p.startswith(I2C+"/") for p in base),"I2C9 baseline must be disabled/empty")
 require(cells(base[I2C]["reg"])==(0,0x2ae80000,0,0x1000),"wrong I2C9 address")
 require(base[AMP]["compatible"]==b"rockchip,mcu-amp\0" and enabled(base,AMP),"frozen AMP owner absent")
 require(cells(base[AMP]["clocks"])==tuple(x for i in (207,208,209,139,150) for x in (cru,i)),"unexpected frozen clock set")
 require(cells(target[AMP]["clocks"])==cells(base[AMP]["clocks"])+(cru,118,cru,130),"clock hold list changed")
 require(cells(target[AMP]["assigned-clocks"])==(cru,150,cru,130),"assigned clocks")
 require(cells(target[AMP]["assigned-clock-parents"])==(0,xin),"I2C9 fixed parent")
 require(cells(target[AMP]["assigned-clock-rates"])==(24000000,24000000),"clock rates")
 require(cells(target[AMP]["pinctrl-0"])==(uart,pin),"UART/I2C9 pin ownership")
 require(cells(base[symbol(base,"i2c9m1_xfer")]["rockchip,pins"])[::4]==(1,1),"I2C9 pin bank")
 pins=cells(base[symbol(base,"i2c9m1_xfer")]["rockchip,pins"])
 require(pins[1:3]==(13,10) and pins[5:7]==(12,10),"I2C9 M1 pad/mux")
 require(cells(base[symbol(base,"xin24m")]["clock-frequency"])==(24000000,),"xin24m is not fixed 24MHz")
 # No enabled alternate consumer may select either pad through any pinctrl state.
 by_handle={cells(p["phandle"])[0]:p for p in target.values() if "phandle" in p}
 for path,properties in target.items():
  if not enabled(target,path):continue
  for key,value in properties.items():
   if key.startswith("pinctrl-") and key!="pinctrl-names":
    for handle in cells(value):
     config=by_handle.get(handle,{})
     raw=cells(config.get("rockchip,pins",b""))
     for i in range(0,len(raw),4):
      require(path==AMP or raw[i:i+2] not in ((1,12),(1,13)),f"pad conflict: {path}:{key}")
 gpio1_path=symbol(target,"gpio1");gpio1=phandle(target,"gpio1")
 for path,properties in target.items():
  if not enabled(target,path):continue
  if "gpio-hog" in properties and path.rpartition("/")[0]==gpio1_path:
   raw=cells(properties.get("gpios",b""))
   require(all(pin not in (12,13) for pin in raw[::2]),f"GPIO1 hog conflict: {path}")
  else:
   for key,value in properties.items():
    if key=="gpios" or key.endswith("-gpios"):
     raw=cells(value)
     # Parse provider #gpio-cells rather than guessing across heterogeneous lists.
     i=0
     while i<len(raw):
      handle=raw[i]
      if handle==0:i+=1;continue
      provider=by_handle.get(handle,{})
      count=cells(provider.get("#gpio-cells",b""))
      require(len(count)==1 and i+count[0]<len(raw),f"unresolved GPIO provider: {path}:{key}")
      require(handle!=gpio1 or raw[i+1] not in (12,13),f"GPIO1 consumer conflict: {path}:{key}")
      i+=1+count[0]
 return {"kind":"I2C9_LINUX_OWNER_DT_HOST_VALIDATED","deployable":False,"ownership":"SOURCE_CONFIG_ONLY_RUNTIME_PENDING","changed_properties":sorted(ALLOWED),"preserves_all_other_nodes_and_properties":True,"i2c9_linux_disabled_empty":True,"clock_hold_ids":[118,130],"i2c9_clock_hz":24000000,"pinmux":"GPIO1_B5/B4 function10"}
def main():
 p=argparse.ArgumentParser();p.add_argument("--kernel",type=pathlib.Path,required=True);p.add_argument("--base-dtb",type=pathlib.Path,required=True);p.add_argument("--output",type=pathlib.Path,required=True);a=p.parse_args()
 source=json.loads((ROOT/"patches/mpu6050/kernel-resource-inputs.json").read_text());verify_files(a.kernel,source["files"])
 require(hashlib.sha256(a.base_dtb.read_bytes()).hexdigest()==BASE_SHA,"frozen DT hash mismatch")
 out=a.output.resolve();require(out.is_relative_to(ROOT/"artifacts/local") and not out.exists(),"fresh local output required");out.mkdir(parents=True)
 overlay=ROOT/"patches/mpu6050/i2c9-linux-owner.dtso"
 subprocess.run(["dtc","-@","-I","dts","-O","dtb","-o",str(out/"i2c9-linux-owner.dtbo"),str(overlay)],check=True)
 subprocess.run(["fdtoverlay","-i",str(a.base_dtb),"-o",str(out/"mpu-i2c9-candidate.dtb"),str(out/"i2c9-linux-owner.dtbo")],check=True)
 result=validate(fdt(a.base_dtb.read_bytes()),fdt((out/"mpu-i2c9-candidate.dtb").read_bytes()))
 result["kernel_sources"]=source
 result["files"]={"base_dtb":BASE_SHA,**{x:hashlib.sha256((out/x).read_bytes()).hexdigest() for x in ("i2c9-linux-owner.dtbo","mpu-i2c9-candidate.dtb")},"overlay_source":hashlib.sha256(overlay.read_bytes()).hexdigest()}
 (out/"DT_VALIDATION.json").write_text(json.dumps(result,indent=2)+"\n");print(out/"DT_VALIDATION.json")
if __name__=="__main__":main()
