#!/usr/bin/env bash
# Ordinary interface/permission inventory only. No OTP/MMIO reads or sudo.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
umask 077
out="$ROOT/artifacts/local/p026-interfaces-$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir -p -- "$out"
remote=(ssh "${SSH_ARGS[@]}")
if [[ -n "${AMP_SSH_HOSTNAME:-}" ]]; then remote+=(-o "Hostname=$AMP_SSH_HOSTNAME"); fi
if [[ -n "${AMP_SSH_PROXYCOMMAND:-}" ]]; then remote+=(-o "ProxyCommand=$AMP_SSH_PROXYCOMMAND"); fi
remote+=(lubancat)
set +e
timeout --signal=TERM --kill-after=2s 45s "${remote[@]}" 'sh -s' >"$out/inventory.txt" 2>"$out/inventory.stderr" <<'REMOTE'
set -u
run() { printf '\nCOMMAND:'; printf ' %s' "$@"; printf '\n'; "$@"; s=$?; printf 'EXIT_CODE=%s\n' "$s"; return 0; }
run uname -a
run cat /proc/cmdline
run id
run lsblk -b -o NAME,SIZE,TYPE,PARTLABEL,FSTYPE,MOUNTPOINTS
run ls -l /dev/mmcblk0p1 /dev/disk/by-partlabel/uboot /dev/tee0 /dev/teepriv0
run sh -c 'for p in /sys/bus/nvmem/devices/* /sys/bus/nvmem/devices/*/nvmem; do test -e "$p" && ls -ld "$p"; done'
run sh -c 'for p in /sys/kernel/debug/pinctrl /sys/kernel/debug/pinctrl/*/pinmux-pins; do test -e "$p" && ls -ld "$p"; done'
run sh -c 'for x in tee-supplicant optee_example_hello_world teec_test xtest busybox dd sha256sum; do command -v "$x" || true; done'
run sh -c 'pgrep -x tee-supplicant || true'
run sh -c 'r=$(uname -r); du -sk "/lib/modules/$r"; find "/lib/modules/$r" -maxdepth 1 -type f -printf "%f %s bytes\n"; test ! -r /dev/mmcblk0p1 || printf "RAW_UBOOT_READABLE_AS_CAT\n"'
exit 0
REMOTE
rc=$?
set -e
printf 'ordinary_inventory_ssh_exit=%s\n' "$rc" >"$out/result.txt"
sha256sum "$out/inventory.txt" >"$out/hash.sha256"
printf 'Local evidence: %s\n' "$out"
exit "$rc"
