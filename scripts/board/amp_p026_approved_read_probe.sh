#!/usr/bin/env bash
# One user-authorized read probe. No firmware/GPT/OTP/MMIO writes or M0 start.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
umask 077
[[ ${AMP_P026_READ_APPROVED:-} == 1 ]] || { echo 'Explicit read authorization required' >&2; exit 2; }
deadline=$(python3 -c 'from datetime import datetime; from zoneinfo import ZoneInfo; print(int(datetime(2026,10,3,12,tzinfo=ZoneInfo("Asia/Shanghai")).timestamp()))')
within_authorization() { (( $(date -u +%s) < deadline )) || { echo 'User authorization expired' >&2; return 2; }; }
within_authorization
resume=0
if [[ ${1:-} == --resume && $# == 2 ]]; then
    out=$(realpath -- "$2")
    [[ "$out" == "$ROOT"/artifacts/local/p026-approved-read-* && -d "$out" ]] || exit 2
    [[ $(stat -c %s "$out/uboot-original.raw") == 8388608 ]] || exit 2
    grep -qx 'raw_uboot_read=PASS' "$out/result.txt" || exit 2
    [[ ! -e "$out/tee-read.stdout" ]] || { echo 'TEE already attempted; no retry' >&2; exit 2; }
    cp -- "$out/pinmux-owners.txt" "$out/pinmux-owners.initial.txt"
    cp -- "$out/pinmux.stderr" "$out/pinmux.initial.stderr"
    resume=1
else
    [[ $# == 0 ]] || exit 2
    out="$ROOT/artifacts/local/p026-approved-read-$(date -u +%Y%m%dT%H%M%SZ)-$$"
    mkdir -p -- "$out"
fi
binary="$ROOT/artifacts/local/p026-tee-flag-host-reviewed/read_verified_boot_flag"
expected=2b8929f02dbc86ff4f6f34eaecc0d878b9de1706956f8a8a33b84b8ca2e6beca
[[ $(sha256sum "$binary" | cut -d' ' -f1) == "$expected" ]] || { echo 'Reviewed tool hash changed' >&2; exit 2; }
remote=(ssh "${SSH_ARGS[@]}")
if [[ -n "${AMP_SSH_HOSTNAME:-}" ]]; then remote+=(-o "Hostname=$AMP_SSH_HOSTNAME"); fi
if [[ -n "${AMP_SSH_PROXYCOMMAND:-}" ]]; then remote+=(-o "ProxyCommand=$AMP_SSH_PROXYCOMMAND"); fi
remote+=(lubancat)
if (( resume == 0 )); then
printf 'authorization=user reply: no separate approvals until 2026-10-03 12:00 Asia/Shanghai\nread_probe_sha256=%s\n' "$expected" >"$out/result.txt"
# Copy bounded known original firmware first; stdout remains a binary stream.
within_authorization
timeout --signal=TERM --kill-after=2s 25s "${remote[@]}" \
    'test "$(readlink -f /dev/disk/by-partlabel/uboot)" = /dev/mmcblk0p1 && test "$(sudo -n blockdev --getsize64 /dev/mmcblk0p1)" = 8388608 && sudo -n dd if=/dev/disk/by-partlabel/uboot bs=1048576 count=8 status=none' \
    >"$out/uboot-original.raw" 2>"$out/uboot-read.stderr"
[[ $(stat -c %s "$out/uboot-original.raw") == 8388608 ]] || { echo 'Incomplete raw backup' >&2; exit 2; }
printf 'raw_uboot_read=PASS\n' >>"$out/result.txt"
fi
within_authorization
timeout --signal=TERM --kill-after=2s 15s "${remote[@]}" 'sh -s' >"$out/pinmux-owners.txt" 2>"$out/pinmux.stderr" <<'REMOTE'
set -eu
paths=$(sudo -n find /sys/kernel/debug/pinctrl -maxdepth 2 -type f -name pinmux-pins -print)
test -n "$paths"
for path in $paths; do
    case "$path" in /sys/kernel/debug/pinctrl/*/pinmux-pins) ;; *) exit 2 ;; esac
    printf '\nFILE:%s\n' "$path"
    sudo -n cat "$path"
done
REMOTE
printf 'pinmux_owner_read=PASS\n' >>"$out/result.txt"
within_authorization
temp="/dev/shm/rk3576-p026-read-flag-$(date -u +%Y%m%dT%H%M%SZ)-$$"
# The task-created RAM file is the only board upload, removed after the probe.
timeout --signal=TERM --kill-after=2s 15s "${remote[@]}" \
    "mkdir -m 700 '$temp' && cat > '$temp/read_verified_boot_flag' && chmod 700 '$temp/read_verified_boot_flag'" <"$binary"
cleanup() {
    timeout --signal=TERM --kill-after=2s 10s "${remote[@]}" \
        "rm -f -- '$temp/read_verified_boot_flag'; rmdir -- '$temp'" \
        >"$out/cleanup.stdout" 2>"$out/cleanup.stderr" || true
}
trap cleanup EXIT
actual=$(timeout --signal=TERM --kill-after=2s 10s "${remote[@]}" "sha256sum '$temp/read_verified_boot_flag'" | cut -d' ' -f1)
[[ "$actual" == "$expected" ]] || { echo 'Board RAM copy hash mismatch' >&2; exit 2; }
within_authorization
set +e
timeout --signal=TERM --kill-after=2s 20s "${remote[@]}" \
    "timeout -k 2s 15s sudo -n '$temp/read_verified_boot_flag' --read-vboot-flag" \
    >"$out/tee-read.stdout" 2>"$out/tee-read.stderr"
rc=$?
set -e
printf 'tee_read_exit_code=%s\n' "$rc" >>"$out/result.txt"
sha256sum "$out/uboot-original.raw" "$out/pinmux-owners.txt" "$out/tee-read.stdout" >"$out/hashes.sha256"
printf 'Local evidence: %s\nTEE read exit code: %s\n' "$out" "$rc"
# Any failure stops this read route. No UUID/command/path retries.
exit "$rc"
