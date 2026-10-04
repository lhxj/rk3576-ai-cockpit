#!/usr/bin/env bash
# Ordinary sysfs sector metadata only; no raw device read or write.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
remote=(ssh "${SSH_ARGS[@]}")
if [[ -n ${AMP_SSH_HOSTNAME:-} ]]; then remote+=(-o "Hostname=$AMP_SSH_HOSTNAME"); fi
if [[ -n ${AMP_SSH_PROXYCOMMAND:-} ]]; then remote+=(-o "ProxyCommand=$AMP_SSH_PROXYCOMMAND"); fi
remote+=(lubancat)
timeout -k 2s 15s "${remote[@]}" 'sh -s' <<'REMOTE'
set -eu
for n in mmcblk0p1 mmcblk0p2 mmcblk0p3; do
    printf '%s start=' "$n"
    cat "/sys/class/block/$n/start"
    printf 'size='
    cat "/sys/class/block/$n/size"
done
REMOTE
