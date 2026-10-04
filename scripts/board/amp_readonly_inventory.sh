#!/usr/bin/env bash
# Bounded, read-only AMP boot inventory. Raw output is local and git-ignored.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
umask 077
out="$ROOT/artifacts/local/amp-boot-$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir -p -- "$out"
set +e
timeout --signal=TERM --kill-after=2s 45s ssh "${SSH_ARGS[@]}" lubancat 'sh -s' >"$out/amp-boot.txt" 2>&1 <<'REMOTE'
section() { printf '\n===== %s =====\n' "$1"; }
section SYSTEM
uname -a
cat /proc/version
cat /proc/cmdline
section DEVICE_TREE
if [ -r /proc/device-tree/model ]; then tr '\000' '\n' </proc/device-tree/model; fi
if [ -r /proc/device-tree/compatible ]; then tr '\000' '\n' </proc/device-tree/compatible; fi
if [ -r /sys/firmware/fdt ]; then sha256sum /sys/firmware/fdt; else echo 'running FDT not readable'; fi
section BOOT_FILES
ls -ld /boot /boot/uEnv /boot/uEnv/uEnv.txt 2>&1
ls -l /boot/Image /boot/initrd.img /boot/boot.cmd /boot/boot.scr /boot/dtb/rk3576-lubancat-3-v2.dtb 2>&1
readlink -f /boot/uEnv/uEnv.txt 2>&1
find /boot -maxdepth 2 -type f -printf '%p %s bytes\n' 2>/dev/null | sort | head -n 100
section BOOT_HASHES
for p in /boot/uEnv/uEnv.txt /boot/uEnv/uEnvLubanCat3-V2.txt /boot/dtb/rk3576-lubancat-3-v2.dtb /boot/boot.cmd /boot/boot.scr /boot/Image /boot/Image-$(uname -r) /boot/vmlinuz-$(uname -r); do
    if [ -r "$p" ] && [ -f "$p" ]; then sha256sum "$p"; fi
done
section UENV_AMP_RELEVANT
if [ -r /boot/uEnv/uEnv.txt ]; then grep -niE 'fdt|dtb|kernel|boot|amp|overlay|initrd|fit|itb' /boot/uEnv/uEnv.txt | head -n 80; fi
if [ -r /boot/boot.cmd ]; then grep -niE 'fdt|dtb|kernel|boot|amp|overlay|initrd|fit|itb' /boot/boot.cmd | head -n 100; fi
section KERNEL_CONFIG
if [ -r /boot/config-$(uname -r) ]; then grep -E 'CONFIG_(MAILBOX|ROCKCHIP_MBOX|RPMSG|REMOTEPROC|ROCKCHIP_AMP)' /boot/config-$(uname -r); else echo 'config unavailable'; fi
section AMP_SYSFS
ls -ld /sys/rk_amp /sys/bus/rpmsg /sys/class/remoteproc /sys/class/mailbox 2>&1
find /sys/rk_amp /sys/bus/rpmsg/devices /sys/class/mailbox -maxdepth 2 -type l -print 2>/dev/null | head -n 60
section DEVICE_TREE_RPMSG
find /proc/device-tree -maxdepth 3 -iname '*rpmsg*' -o -iname '*amp*' -o -iname '*mailbox*' 2>/dev/null | head -n 80
section IRQ_BOOT_LOG
grep -Ei 'mailbox|rpmsg|rockchip.amp|mcu|bl31|uboot|u-boot' /proc/interrupts | head -n 30
if dmesg >/dev/null 2>&1; then dmesg | grep -Ei 'mailbox|rpmsg|rockchip.amp|mcu|bl31|u-boot' | tail -n 80; else echo 'dmesg unavailable'; fi
section DONE
exit 0
REMOTE
rc=$?
set -e
printf 'ssh_or_timeout_exit_code=%s\n' "$rc" >"$out/result.txt"
printf 'Local evidence: %s\n' "$out"
exit "$rc"
