#!/usr/bin/env bash
# Bounded ordinary-file/package inventory; no raw block, MMIO, or sudo.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
umask 077
out="$ROOT/artifacts/local/amp-boot-package-$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir -p -- "$out"
set +e
timeout --signal=TERM --kill-after=2s 45s ssh "${SSH_ARGS[@]}" lubancat 'sh -s' >"$out/inventory.txt" 2>&1 <<'REMOTE'
section() { printf '\n===== %s =====\n' "$1"; }
section UBOOT_PACKAGE
dpkg-query -s u-boot 2>&1 | head -n 80
dpkg-query -L u-boot 2>&1 | head -n 100
section KERNEL_PACKAGE
dpkg-query -s linux-image-6.1.99-rk3576 2>&1 | head -n 65
section BOOT_SCRIPT_RELEVANT
grep -n -Ei 'amp|fit|load|uEnv|boot|fdt|dtb|overlay|mmc|part' /boot/boot.cmd 2>&1 | head -n 115
section IMAGE_METADATA_CANDIDATES
find /etc /usr/share/doc -maxdepth 4 -type f \( -iname '*rk3576*' -o -iname '*uboot*' -o -iname '*u-boot*' -o -iname '*bl31*' -o -iname '*rkbin*' \) -printf '%p %s bytes\n' 2>/dev/null | head -n 90
section FIRMWARE_SYSFS
find /sys/firmware -maxdepth 3 -type f \( -iname '*version*' -o -iname '*amp*' -o -iname '*mcu*' \) -print 2>/dev/null | head -n 60
section END
REMOTE
rc=$?
set -e
printf 'ssh_or_timeout_exit_code=%s\n' "$rc" >"$out/result.txt"
printf 'Local evidence: %s\n' "$out"
exit "$rc"
