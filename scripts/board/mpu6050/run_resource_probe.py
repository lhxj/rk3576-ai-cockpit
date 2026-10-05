#!/usr/bin/env python3
"""Single bounded diagnostic window. Lock is held by outer board controller.
Never retry insmod, reset M0, reboot, or shut down here.
"""
import json,subprocess,time
from pathlib import Path
BASE=Path('/boot/amp-p029/i2c-resource-probe-v1')
STATUS=Path('/sys/module/rk3576_i2c_resource_probe/parameters/health_status')
subprocess.run(['python3',str(BASE/'preflight-resource-probe.py')],check=True,timeout=10)
subprocess.run(['insmod',str(BASE/'rk3576_i2c_resource_probe.ko')],check=True,timeout=10)
start=time.monotonic();observed_done=False;failure=False
while time.monotonic()-start<25:
    text=STATUS.read_text().strip()
    print('RESOURCE_PROBE_LINUX',text,flush=True)
    fields=dict(x.split('=',1) for x in text.split())
    if fields.get('phase')=='FAILED' or fields.get('resource_probe_state')=='3':
        failure=True;break
    if fields.get('resource_probe_state')=='2' and int(fields.get('pong','0'))>=3:
        observed_done=True;break
    time.sleep(.5)
if not observed_done:
    print('RESOURCE_PROBE_STOP failed_or_missing_reply; preserve KO/CCF references for root review/cold recovery',flush=True)
    raise SystemExit(1)
print('RESOURCE_PROBE_LINUX_REPLY_CONFIRMED; M0 UART END and exact whitelist still required',flush=True)
# Normal unload stops Linux delayed work and releases ONLY extra I2C clock refs;
# mcu-amp retains original clocks/pinctrl until cold recovery.
subprocess.run(['rmmod','rk3576_i2c_resource_probe'],check=True,timeout=10)
if STATUS.exists():raise SystemExit('STOP module parameters remain')
print('RESOURCE_PROBE_LINUX_CLEAN_EXIT; M0 idle receiver, no writes/no data stream; cold recovery required',flush=True)
