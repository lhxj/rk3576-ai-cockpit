#!/usr/bin/env bash
# Copy ordinary installed module files to Host for ABI audit; no sudo/writes.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
umask 077
out="$ROOT/artifacts/local/p026-board-modules-$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir -p -- "$out"
remote=(ssh "${SSH_ARGS[@]}")
if [[ -n "${AMP_SSH_HOSTNAME:-}" ]]; then remote+=(-o "Hostname=$AMP_SSH_HOSTNAME"); fi
if [[ -n "${AMP_SSH_PROXYCOMMAND:-}" ]]; then remote+=(-o "ProxyCommand=$AMP_SSH_PROXYCOMMAND"); fi
remote+=(lubancat)
timeout --signal=TERM --kill-after=2s 55s "${remote[@]}" 'sh -s' >"$out/modules.tar" 2>"$out/copy.stderr" <<'REMOTE'
set -eu
release=$(uname -r)
test "$release" = 6.1.99-rk3576 || { echo 'Kernel release changed; stop' >&2; exit 2; }
test -d "/lib/modules/$release"
tar -C /lib/modules -cf - "$release"
REMOTE
printf 'ordinary_module_copy=PASS\n' >"$out/result.txt"
sha256sum "$out/modules.tar" >"$out/hash.sha256"
printf 'Local evidence: %s\n' "$out"
