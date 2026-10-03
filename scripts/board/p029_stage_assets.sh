#!/usr/bin/env bash
# Feed this reviewed Host script through SSH stdin ONLY after Stage A approval.
# No SSH, reboot, persistent environment, default boot edits, or M0 commands.
set -euo pipefail
if [[ ${1:-} != --approved-stage-a ]]; then
    echo 'STOP: pending L3 approval; expected --approved-stage-a after approval' >&2
    exit 2
fi
[[ $# == 3 && $EUID == 0 && $2 =~ ^[0-9a-f]{64}$ && $3 == /dev/shm/amp-p029 ]] || {
    echo 'STOP: root, trusted Host checksum-list SHA256 and fixed RAM source required'; exit 2;
}
trusted_sums_sha=$2
source_packet=$3
command -v python3 >/dev/null
release=6.1.99-rk3576-m0echo-p026
boot_dest=/boot/amp-p029
module_dest=/lib/modules/$release
[[ $(uname -r) == 6.1.99-rk3576 ]] || { echo 'STOP: require original running Linux'; exit 2; }
grep -q 'uboot-f8b4554' /proc/cmdline || { echo 'STOP: require observed P028 U-Boot'; exit 2; }
[[ $(findmnt -n -o SOURCE -T /boot) == /dev/mmcblk0p2 ]] || { echo 'STOP: unexpected boot mount'; exit 2; }
[[ $(findmnt -n -o SOURCE -T /) == /dev/mmcblk0p3 ]] || { echo 'STOP: unexpected root mount'; exit 2; }
[[ ! -e $boot_dest && ! -L $boot_dest && ! -e $module_dest && ! -L $module_dest ]] || {
    echo 'STOP: new stage directories already exist; do not overwrite or retry'; exit 2;
}
# Snapshot only the exact expected files; preserve symlinks for rejection.
# Code arrives from reviewed Host stdin, not from the cat-owned source packet.
umask 077
packet=$(mktemp -d /dev/shm/amp-p029-root.XXXXXXXX)
cleanup_ram() {
    [[ $packet == /dev/shm/amp-p029-root.* && -d $packet && ! -L $packet ]] || return
    rm -r -- "$packet"
}
trap cleanup_ram EXIT
python3 - "$source_packet" "$packet" <<'PY'
from pathlib import Path
import os, stat, sys
source, dest = map(Path, sys.argv[1:])
(dest/'boot').mkdir()
names = ['MANIFEST.json', 'SHA256SUMS', 'boot.sha256', 'modules.sha256', 'modules.tar',
         'README.md', 'stage-assets.sh', 'cam0.dtbo', 'stage-A.cmd', 'stage-B.cmd', 'stage-C.cmd']
names += ['boot/'+name for name in ['Image', 'initrd', 'stage-A.dtb', 'stage-B.dtb', 'stage-C.dtb',
          'stage-A.scr', 'stage-B.scr', 'stage-C.scr', 'amp-host.itb', 'rk3576_amp_echo_test.ko']]
for name in names:
    fd = os.open(source/name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(fd, 'rb') as inp:
        info = os.fstat(inp.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size > 128*1024*1024:
            raise SystemExit('STOP snapshot requires bounded regular files')
        remaining = info.st_size
        with (dest/name).open('xb') as out:
            while remaining:
                chunk = inp.read(min(1048576, remaining))
                if not chunk:
                    raise SystemExit('STOP source shortened during snapshot')
                out.write(chunk)
                remaining -= len(chunk)
PY
[[ $(sha256sum "$packet/SHA256SUMS" | cut -d' ' -f1) == "$trusted_sums_sha" ]] || {
    echo 'STOP snapshot checksum-list differs from trusted Host digest'; exit 2;
}
cd -- "$packet"
sha256sum -c SHA256SUMS
# Root-owned target parents must not be writable by non-root users.
modules_parent=$(realpath -e -- /lib/modules)
[[ $modules_parent == /usr/lib/modules || $modules_parent == /lib/modules ]] || { echo 'STOP modules parent'; exit 2; }
module_dest=$modules_parent/$release
for parent in /boot "$modules_parent"; do
    [[ $(stat -c %u -- "$parent") == 0 ]] || { echo 'STOP target parent owner'; exit 2; }
    mode=$(stat -c %a -- "$parent")
    (( (8#$mode & 8#022) == 0 )) || { echo 'STOP writable target parent'; exit 2; }
done
# Known original default file contents, resolving original symlinks for reads only.
check_factory() {
    (cd /boot && sha256sum -c <<'FACTORY'
e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e  Image
425d2a68a4a67e807518c067cfefc1111d0a33af02236761ddaa3794c3205397  initrd
76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90  rk-kernel.dtb
c498d9be3e8dc91883124cc734be54c42c271fad0501555c1a69f7ea7fc38325  boot.scr
9f0262e807a8188ec5dffe52411401bd82b5fb0d8e81af8107284725e2a7db55  boot.cmd
4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64  uEnv/uEnv.txt
FACTORY
    )
}
check_factory
# Host verified member types/paths and all module hashes. Check again before root extraction.
python3 - "$packet/modules.tar" "$release" <<'PY'
from pathlib import PurePosixPath
import sys, tarfile
prefix = ('lib', 'modules', sys.argv[2])
with tarfile.open(sys.argv[1]) as t:
    seen = set()
    for m in t:
        p = PurePosixPath(m.name)
        if p.is_absolute() or '..' in p.parts or p.parts[:3] != prefix or p in seen or not (m.isfile() or m.isdir()):
            raise SystemExit('STOP unsafe module archive member')
        seen.add(p)
PY
boot_need=$(du -sb boot | awk '{print $1}')
module_need=$(tar -tvf modules.tar | awk '$1 ~ /^-/ {s += $3} END {printf "%.0f", s}')
boot_free=$(df -B1 --output=avail /boot | tail -n1)
root_free=$(df -B1 --output=avail / | tail -n1)
(( boot_free >= boot_need + 10485760 && root_free >= module_need + 10485760 )) || {
    echo 'STOP: require full new package plus 10MiB margin; never delete user data'; exit 2;
}
trap 'echo "STOP partial passive install; default boot untouched; keep evidence and inspect new directories" >&2' ERR
# mkdir fails atomically on any existing path, including a newly inserted link.
mkdir -m 0755 -- "$boot_dest"
mkdir -m 0755 -- "$module_dest"
for name in Image initrd stage-A.dtb stage-B.dtb stage-C.dtb stage-A.scr stage-B.scr stage-C.scr amp-host.itb rk3576_amp_echo_test.ko; do
    [[ -f boot/$name && ! -L boot/$name ]]
    install -m 0644 -- "boot/$name" "$boot_dest/$name"
done
tar --no-same-owner --no-overwrite-dir --strip-components=3 -xf modules.tar -C "$module_dest"
(cd "$boot_dest" && sha256sum -c "$packet/boot.sha256")
(cd / && sha256sum -c "$packet/modules.sha256")
check_factory
install -m 0644 -- MANIFEST.json "$boot_dest/.p029-MANIFEST.json"
install -m 0644 -- boot.sha256 "$boot_dest/.p029-boot.sha256"
install -m 0644 -- modules.sha256 "$boot_dest/.p029-modules.sha256"
sync
echo 'P029_STAGE_ASSETS_INSTALLED; DEFAULT_UNCHANGED; M0_NOT_STARTED'
echo 'No automatic boot change. Wait for the separately approved Stage A cold test.'
