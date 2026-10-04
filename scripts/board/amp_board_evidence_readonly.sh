#!/usr/bin/env bash
# Ordinary filesystem/proc/sys read-only evidence. No raw block or MMIO access.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
umask 077
out="$ROOT/artifacts/local/amp-board-evidence-$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir -p -- "$out"
set +e
timeout --signal=TERM --kill-after=2s 60s ssh "${SSH_ARGS[@]}" lubancat 'sh -s' >"$out/inventory.txt" 2>&1 <<'REMOTE'
section() { printf '\n===== %s =====\n' "$1"; }
section VERSIONS
uname -a
cat /proc/version
cat /proc/cmdline
section BOOT_MOUNT_AND_FILES
findmnt /boot /boot/uEnv 2>&1 | head -n 20
find /boot -maxdepth 3 -type f \( -iname '*uboot*' -o -iname '*u-boot*' -o -iname '*bl31*' -o -iname '*trust*' -o -iname '*loader*' -o -iname '*idblock*' -o -iname '*fit*' -o -iname '*amp*' \) -printf '%p %s bytes\n' 2>/dev/null | head -n 100
ls -l /boot/uEnv/uEnv.txt /boot/boot.cmd /boot/boot.scr /boot/Image /boot/dtb/rk3576-lubancat-3-v2.dtb 2>&1
sha256sum /boot/uEnv/uEnv.txt /boot/boot.cmd /boot/boot.scr /boot/Image /boot/dtb/rk3576-lubancat-3-v2.dtb 2>&1
section PARTITIONS
lsblk -o NAME,TYPE,PARTLABEL,PARTUUID,FSTYPE,SIZE,MOUNTPOINTS 2>&1 | head -n 45
ls -l /dev/disk/by-partlabel 2>&1 | head -n 45
ls -l /dev/mmcblk0p1 2>&1
section CANDIDATE_INSTALLED_IMAGES
find /usr/lib /lib/firmware /opt -maxdepth 5 -type f \( -iname '*uboot*.img' -o -iname '*uboot*.bin' -o -iname '*u-boot*.img' -o -iname '*bl31*.elf' -o -iname '*bl31*.bin' -o -iname '*trust*.img' -o -iname '*loader*.bin' -o -iname '*idblock*.img' \) -printf '%p %s bytes\n' 2>/dev/null | head -n 100
section PACKAGES
dpkg-query -W -f='${binary:Package} ${Version}\n' 'u-boot*' 'rkbin*' 'rockchip*' 'linux-image*' 2>&1 | head -n 70
section AMP_AND_FIRMWARE_LOGS
dmesg 2>/dev/null | grep -Ei 'bl31|uboot|u-boot|amp|rpmsg|mcu|sgrf|mailbox|secure boot|verified boot|fit image' | tail -n 100
section ORDINARY_EXPOSED_NODES
find /sys/firmware /sys/devices/platform /proc/device-tree -maxdepth 3 \( -iname '*amp*' -o -iname '*rpmsg*' -o -iname '*mcu*' -o -iname '*sgrf*' \) -print 2>/dev/null | head -n 80
section END
REMOTE
rc=$?
set -e
printf 'ssh_or_timeout_exit_code=%s\n' "$rc" >"$out/result.txt"
printf 'Local evidence: %s\n' "$out"
exit "$rc"
