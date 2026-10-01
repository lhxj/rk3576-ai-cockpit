#!/usr/bin/env bash
# Bounded, read-only boot-chain and kernel-source inventory. No register access.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
umask 077
out="$ROOT/artifacts/local/amp-platform-$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir -p -- "$out"
set +e
timeout --signal=TERM --kill-after=2s 45s ssh "${SSH_ARGS[@]}" lubancat 'sh -s' >"$out/read-only.txt" 2>&1 <<'REMOTE'
section() { printf '\n===== %s =====\n' "$1"; }
section VERSION
uname -a
cat /proc/version
cat /proc/cmdline
section BOOT_IDENTIFIERS
ls -l /boot/*uboot* /boot/*trust* /boot/*loader* /boot/*config* /boot/*Image* 2>&1 | head -n 80
ls -l /dev/mmcblk0p1 /dev/mmcblk0p2 2>&1
if [ -r /dev/mmcblk0p1 ]; then echo 'uboot_partition_readable_by_cat=yes'; else echo 'uboot_partition_readable_by_cat=no'; fi
if [ -r /proc/device-tree/chosen/bootargs ]; then tr '\000' '\n' </proc/device-tree/chosen/bootargs; fi
section KERNEL_SOURCE_HEADERS
ls -ld /usr/src /usr/src/* /lib/modules/$(uname -r) /lib/modules/$(uname -r)/build /lib/modules/$(uname -r)/source 2>&1 | head -n 100
du -sh /usr/src/linux-headers-$(uname -r) 2>&1
find /usr/src/linux-headers-$(uname -r) -maxdepth 3 \( -name Module.symvers -o -name .config -o -name Makefile -o -name auto.conf -o -name modpost -o -name gcc-version.sh \) -print 2>/dev/null | head -n 60
section AMP_KERNEL_CONFIG
if [ -r /boot/config-$(uname -r) ]; then grep -E '^(CONFIG_(ROCKCHIP_AMP|RPMSG|MAILBOX|KALLSYMS|IKCONFIG|MODULES|MODULE_SIG|LOCALVERSION)|# CONFIG_(RPMSG|MODULE_SIG))' /boot/config-$(uname -r) | head -n 100; fi
section AMP_DT_NODES
find /proc/device-tree -maxdepth 4 \( -iname '*amp*' -o -iname '*rpmsg*' -o -iname '*mailbox*' \) -print 2>/dev/null | head -n 80
section AMP_SYSFS
ls -ld /sys/rk_amp /sys/bus/rpmsg/devices /sys/class/remoteproc /sys/kernel/debug 2>&1
section LIMITED_LOGS
if dmesg >/dev/null 2>&1; then dmesg | grep -Ei 'bl31|ddr-v|uboot-|u-boot|rockchip.amp|rpmsg|mailbox|bus.mcu|sgrf|trust' | tail -n 100; else echo 'dmesg unavailable'; fi
section DONE
exit 0
REMOTE
rc=$?
set -e
printf 'ssh_or_timeout_exit_code=%s\n' "$rc" >"$out/result.txt"
printf 'Local evidence: %s\n' "$out"
exit "$rc"
