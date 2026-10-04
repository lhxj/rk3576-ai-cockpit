#!/usr/bin/env bash
# Same reviewed read-only diagnostic, using existing executable RAM tmpfs.
# The earlier /dev/shm attempt failed before execution because it is noexec.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
umask 077
[[ $# == 0 && ${AMP_P026_READ_APPROVED:-} == 1 ]] || exit 2
deadline=$(python3 -c 'from datetime import datetime; from zoneinfo import ZoneInfo; print(int(datetime(2026,10,3,12,tzinfo=ZoneInfo("Asia/Shanghai")).timestamp()))')
within_authorization() { (( $(date -u +%s) < deadline )) || { echo 'User authorization expired' >&2; return 2; }; }
within_authorization
binary="$ROOT/artifacts/local/p026-tee-flag-host-reviewed/read_verified_boot_flag"
expected=2b8929f02dbc86ff4f6f34eaecc0d878b9de1706956f8a8a33b84b8ca2e6beca
[[ $(sha256sum "$binary" | cut -d' ' -f1) == "$expected" ]] || exit 2
remote=(ssh "${SSH_ARGS[@]}")
if [[ -n ${AMP_SSH_HOSTNAME:-} ]]; then remote+=(-o "Hostname=$AMP_SSH_HOSTNAME"); fi
if [[ -n ${AMP_SSH_PROXYCOMMAND:-} ]]; then remote+=(-o "ProxyCommand=$AMP_SSH_PROXYCOMMAND"); fi
remote+=(lubancat)
stamp="$(date -u +%Y%m%dT%H%M%SZ)-$$"
out="$ROOT/artifacts/local/p026-tee-run-$stamp"
temp="/run/rk3576-p026-read-flag-$stamp"
mkdir -p -- "$out"
printf 'authorization=user reply: until 2026-10-03 12:00 Asia/Shanghai\nbinary_sha256=%s\n' "$expected" >"$out/result.txt"
# Refuse persistent filesystems or noexec; do not change mount policy.
timeout -k 2s 10s "${remote[@]}" \
    'findmnt -n -o FSTYPE,OPTIONS -T /run; findmnt -n -o FSTYPE,OPTIONS -T /dev/shm' \
    >"$out/mounts.txt" 2>"$out/mounts.stderr"
read -r fstype options <"$out/mounts.txt"
[[ "$fstype" == tmpfs && ",$options," != *,noexec,* ]] || exit 2
within_authorization
timeout -k 2s 10s "${remote[@]}" "sudo -n mkdir -m 700 '$temp'"
cleanup() {
    timeout -k 2s 10s "${remote[@]}" \
        "sudo -n rm -f -- '$temp/read_verified_boot_flag' && sudo -n rmdir -- '$temp'" \
        >"$out/cleanup.stdout" 2>"$out/cleanup.stderr"
}
trap cleanup EXIT
timeout -k 2s 15s "${remote[@]}" \
    "sudo -n tee '$temp/read_verified_boot_flag' >/dev/null && sudo -n chmod 700 '$temp/read_verified_boot_flag'" <"$binary"
timeout -k 2s 10s "${remote[@]}" "sudo -n sha256sum '$temp/read_verified_boot_flag'" >"$out/remote-binary.sha256"
[[ $(cut -d' ' -f1 "$out/remote-binary.sha256") == "$expected" ]] || exit 2
within_authorization
set +e
timeout -k 2s 20s "${remote[@]}" \
    "timeout -k 2s 15s sudo -n '$temp/read_verified_boot_flag' --read-vboot-flag" \
    >"$out/tee-read.stdout" 2>"$out/tee-read.stderr"
rc=$?
set -e
printf 'tee_read_exit_code=%s\n' "$rc" >>"$out/result.txt"
sha256sum "$out/tee-read.stdout" "$out/tee-read.stderr" "$out/mounts.txt" >"$out/hashes.sha256"
printf 'Local evidence: %s\nTEE read exit code: %s\n' "$out" "$rc"
# No automatic retry after an actual TEE request, including refusal/timeout.
exit "$rc"
