#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../../.."
source scripts/board/_common.sh
board_lock
timeout --signal=TERM --kill-after=2s 30s ssh "${SSH_ARGS[@]}" -o HostName=10.232.249.223 lubancat 'python3 -' <<'REMOTE'
from pathlib import Path
import os,subprocess
args=Path('/proc/cmdline').read_text().split()
if os.uname().release=='6.1.99-rk3576':
    assert not any(x.startswith(('amp_test_stage=','amp_health_test=','i2c_resource_probe=')) for x in args)
else:
    assert os.uname().release=='6.1.99-rk3576-m0echo-p026'
    assert 'i2c_resource_probe=I2C_RESOURCE_PROBE_V1' in args
occupied=[]
for p in Path('/proc').iterdir():
    if not p.name.isdigit():continue
    try:
        n=(p/'comm').read_text().strip()
        if n.startswith(('cockpit_ui','voice_runtime','cam0_','media_rtsp','media_record','vision_rknn')):occupied.append((p.name,n))
    except OSError:pass
assert not occupied,'project occupants remain; no shutdown'
print('I2C_RESOURCE_PROBE_AUTHORIZED_NORMAL_SHUTDOWN_ONCE; user cold power cycle required',flush=True)
subprocess.run(['sudo','-n','shutdown','-h','now'],check=True)
REMOTE
