#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../../.."
source scripts/board/_common.sh
board_lock
timeout --signal=TERM --kill-after=2s 45s ssh "${SSH_ARGS[@]}" -o HostName=10.232.249.223 lubancat 'python3 -' <<'REMOTE'
from pathlib import Path
import datetime
import hashlib
import json
import os
import subprocess

print('UTC', datetime.datetime.now(datetime.timezone.utc).isoformat())
print('UNAME', ' '.join(os.uname()))
args = Path('/proc/cmdline').read_text().split()
print('BOOT_IDENTITY', json.dumps([x for x in args if x.startswith(('root=', 'boot_part=', 'amp_test_stage=', 'amp_health_test=', 'i2c_resource_probe=', 'fwver='))]))
print('RESOURCE_PROBE_DEST_EXISTS', Path('/boot/amp-p029/i2c-resource-probe-v1').exists())
print('UPTIME', Path('/proc/uptime').read_text().split()[0])
print('RPMSG_DEVICES', sorted(p.name for p in Path('/sys/bus/rpmsg/devices').iterdir()))
print('RPMSG_DRIVERS', sorted(p.name for p in Path('/sys/bus/rpmsg/drivers').iterdir()))
print('RPMSG_MODULES', [x.split()[0] for x in Path('/proc/modules').read_text().splitlines() if any(tag in x.lower() for tag in ('rpmsg','amp_echo','amp_health','i2c_resource_probe'))])
paths = [
    '/boot/uEnv/uEnv.txt', '/boot/uEnv/uEnvLubanCat3-V2.txt',
    '/boot/extlinux/extlinux.conf', '/boot/boot.scr', '/boot/boot.cmd', '/boot/Image', '/boot/vmlinuz-6.1.99-rk3576',
    '/boot/initrd', '/boot/initrd.img-6.1.99-rk3576', '/boot/dtb/rk3576-lubancat-3-v2.dtb',
    '/boot/amp-p029/Image', '/boot/amp-p029/initrd', '/boot/amp-p029/stage-C.dtb',
    '/boot/amp-p029/rk3576_amp_echo_test.ko',
    '/boot/amp-p029/rpmsg-c-v1/stage-C.scr',
    '/boot/amp-p029/rpmsg-c-v1/stage-C.cmd',
    '/boot/amp-p029/rpmsg-c-v1/amp-signed.itb',
    '/boot/amp-p029/rpmsg-c-v1/preflight-c.py',
    '/boot/amp-p029/rpmsg-c-v1/MANIFEST.json',
    '/boot/amp-p029/rpmsg-c-v1/SHA256SUMS',
    *[str(p) for p in Path('/boot/amp-p029/system-health-v1').glob('*') if p.is_file()],
]
for name in paths:
    path = Path(name)
    try:
        resolved = path.resolve(strict=True)
        if not resolved.is_relative_to(Path('/boot')) or not resolved.is_file():
            print('FILE_REFUSED', name, 'outside-boot-or-not-regular')
            continue
        if resolved.stat().st_size > 128 * 1024 * 1024:
            print('FILE_REFUSED', name, 'size-limit')
            continue
        before = resolved.stat()
        with resolved.open('rb') as f:
            digest = hashlib.sha256()
            for chunk in iter(lambda: f.read(1024*1024), b''):
                digest.update(chunk)
        after = resolved.stat()
        if (before.st_ino,before.st_size,before.st_mtime_ns) != (after.st_ino,after.st_size,after.st_mtime_ns):
            raise RuntimeError('file changed during hash')
        print('FILE', json.dumps({'path': name, 'resolved': str(resolved), 'bytes': after.st_size,
                                'sha256': digest.hexdigest(), 'uid': after.st_uid, 'gid': after.st_gid,
                                'mode': oct(after.st_mode & 0o777), 'symlink': path.is_symlink()}))
    except (OSError, RuntimeError) as exc:
        print('FILE_UNREADABLE', name, type(exc).__name__)
try:
    fdt = Path('/sys/firmware/fdt').read_bytes()
    print('LIVE_FDT', len(fdt), hashlib.sha256(fdt).hexdigest())
except OSError as exc:
    print('LIVE_FDT_UNREADABLE', type(exc).__name__)
print('PAIRED_MODULE_TREE_EXISTS', Path('/lib/modules/6.1.99-rk3576-m0echo-p026').is_dir())
print('RELEVANT_BOOT_PATHS')
subprocess.run(['find', '/boot', '-maxdepth', '2', '-path', '/boot/lost+found', '-prune', '-o', '(', '-name', '*Image*', '-o', '-name', '*initrd*',
                '-o', '-name', '*lubancat-3-v2*', '-o', '-name', 'uEnv*.txt', ')', '-print'], check=True)
print('PROJECT_PROCESSES')
occupants=[]
for line in subprocess.check_output(['ps', '-eo', 'pid,comm'], text=True).splitlines():
    if any(x in line for x in ('cockpit', 'voice_runtime', 'cam0_', 'media_vision', 'media_rtsp','media_record','vision_rknn')):
        occupants.append(line.strip())
print('PROJECT_OCCUPANTS',json.dumps(occupants))
print('READONLY_BOOT_IDENTITY_DONE')
REMOTE
