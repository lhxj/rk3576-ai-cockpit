#!/usr/bin/env bash
# Copy installed build headers from the board to ignored Host evidence only.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
umask 077
out="$ROOT/artifacts/local/amp-kernel-headers-$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir -p -- "$out"
timeout --signal=TERM --kill-after=2s 150s ssh "${SSH_ARGS[@]}" lubancat \
    'tar -C /usr/src -cf - linux-headers-$(uname -r)' >"$out/headers.tar"
sha256sum "$out/headers.tar" >"$out/headers.tar.sha256"
tar -tf "$out/headers.tar" >"$out/paths.txt"
printf 'Local evidence: %s\n' "$out"
