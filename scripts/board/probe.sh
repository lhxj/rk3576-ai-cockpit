#!/usr/bin/env bash
# Explicit invocation required. Read-only remote probe; no sudo/capture/boot edits.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
printf 'Read-only probe via existing SSH alias: lubancat\n'
timeout --signal=TERM --kill-after=2s 20s ssh "${SSH_ARGS[@]}" lubancat 'printf "SSH_OK\n"; id; uname -a; printf "Board model: "; if [ -r /proc/device-tree/model ]; then tr -d "\000" < /proc/device-tree/model; fi; printf "\n"'
