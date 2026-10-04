#!/usr/bin/env bash
# Copy only ordinary boot/DT evidence to Host; never MMIO, sudo or board writes.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
umask 077
out="$ROOT/artifacts/local/p025-post-recovery-$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir -p -- "$out"
remote=(ssh "${SSH_ARGS[@]}")
if [[ -n "${AMP_SSH_HOSTNAME:-}" ]]; then
    remote+=(-o "Hostname=$AMP_SSH_HOSTNAME")
fi
if [[ -n "${AMP_SSH_PROXYCOMMAND:-}" ]]; then
    remote+=(-o "ProxyCommand=$AMP_SSH_PROXYCOMMAND")
fi
remote+=(lubancat)
set +e
timeout --signal=TERM --kill-after=2s 45s "${remote[@]}" 'sh -s' >"$out/inventory.txt" 2>"$out/inventory.stderr" <<'REMOTE'
set -u
run() {
    printf '\nCOMMAND:'
    printf ' %s' "$@"
    printf '\n'
    "$@"
    status=$?
    printf 'EXIT_CODE=%s\n' "$status"
    return 0
}
run uname -a
run cat /proc/version /proc/cmdline
run cat /proc/device-tree/model
run lsblk -o NAME,SIZE,TYPE,PARTLABEL,FSTYPE,MOUNTPOINTS
run ls -ld /boot /boot/uEnv/uEnv.txt /boot/Image /boot/rk-kernel.dtb
run readlink -f /boot/uEnv/uEnv.txt
run sha256sum /boot/Image /boot/boot.cmd /boot/boot.scr /boot/uEnv/uEnv.txt /boot/dtb/rk3576-lubancat-3-v2.dtb /boot/config-$(uname -r)
run cat /proc/iomem
run cat /sys/kernel/security/lockdown
run ls -ld /sys/rk_amp /sys/bus/rpmsg/devices /sys/class/remoteproc
run sh -c 'grep -E "^(CONFIG_(ROCKCHIP_AMP|RPMSG|MAILBOX|MODULE_SIG|LOCALVERSION)|# CONFIG_(RPMSG|MODULE_SIG))" /boot/config-$(uname -r)'
run sh -c 'find /proc/device-tree -maxdepth 4 \( -iname "*amp*" -o -iname "*rpmsg*" -o -iname "*secure*" -o -iname "*signature*" \) -print 2>/dev/null | head -n 100'
run sh -c 'if dmesg >/dev/null 2>&1; then dmesg | grep -Ei "bl31|ddr-v|uboot-|u-boot|secure.boot|verified|verification|rockchip.amp|rpmsg|mailbox|bus.mcu|sgrf" | tail -n 100; else echo "dmesg unavailable"; fi'
run sh -c 'du -sk /boot; ls -ld /lib/modules/$(uname -r)/build /lib/modules/$(uname -r)/source'
exit 0
REMOTE
rc=$?
set -e
printf 'inventory_ssh_exit_code=%s\n' "$rc" >"$out/result.txt"
if (( rc != 0 )); then
    printf 'Local evidence: %s\n' "$out"
    exit "$rc"
fi
timeout --signal=TERM --kill-after=2s 45s "${remote[@]}" 'tar -C /proc/device-tree -cf - .' >"$out/running-device-tree.tar" 2>"$out/running-tree.stderr"
# Explicit boot members: no whole rootfs or arbitrary filesystem traversal.
timeout --signal=TERM --kill-after=2s 55s "${remote[@]}" 'sh -s' >"$out/boot-originals.tar" 2>"$out/boot-copy.stderr" <<'REMOTE'
set -eu
cd /boot
release=$(uname -r)
set -- Image "Image-$release" "config-$release" "System.map-$release" "initrd.img-$release" rk-kernel.dtb boot.cmd boot.scr uEnv extlinux dtb/rk3576-lubancat-3-v2.dtb
if test -e initrd || test -L initrd; then
    set -- "$@" initrd
fi
for member do
    test -e "$member" || test -L "$member" || { echo "Missing boot member: $member" >&2; exit 2; }
done
tar -cf - "$@"
REMOTE
printf 'ordinary_boot_and_running_dt_copy=PASS\n' >>"$out/result.txt"
sha256sum "$out/inventory.txt" "$out/running-device-tree.tar" "$out/boot-originals.tar" >"$out/hashes.sha256"
printf 'Local evidence: %s\n' "$out"
