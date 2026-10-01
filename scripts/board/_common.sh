#!/usr/bin/env bash
# Source only. Existing SSH alias, no arbitrary host/command from external content.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
SSH_ARGS=(-o BatchMode=yes -o ConnectTimeout=5 -o ServerAliveInterval=5 -o ServerAliveCountMax=2 -o StrictHostKeyChecking=yes)
board_lock() {
    command -v ssh >/dev/null 2>&1 || { echo 'Missing ssh' >&2; return 2; }
    command -v timeout >/dev/null 2>&1 || { echo 'Missing timeout' >&2; return 2; }
    command -v flock >/dev/null 2>&1 || { echo 'Missing flock' >&2; return 2; }
    local lock_root="${XDG_RUNTIME_DIR:-$HOME/.cache}/rk3576-ai-cockpit-locks"
    umask 077
    mkdir -p -- "$lock_root"
    exec {BOARD_LOCK_FD}>"$lock_root/lubancat.lock"
    if ! flock -n "$BOARD_LOCK_FD"; then
        echo 'Another project board task holds the lock; do not start a concurrent test.' >&2
        return 75
    fi
}
