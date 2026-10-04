#!/usr/bin/env bash
# Read fixed boot files from the board into ignored host artifacts only.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
umask 077
out="$ROOT/artifacts/local/amp-boot-files-$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir -p -- "$out"
timeout --signal=TERM --kill-after=2s 30s ssh "${SSH_ARGS[@]}" lubancat \
    'cat /boot/dtb/rk3576-lubancat-3-v2.dtb' >"$out/rk3576-lubancat-3-v2.dtb"
timeout --signal=TERM --kill-after=2s 30s ssh "${SSH_ARGS[@]}" lubancat \
    'cat /boot/uEnv/uEnv.txt' >"$out/uEnv.txt"
timeout --signal=TERM --kill-after=2s 30s ssh "${SSH_ARGS[@]}" lubancat \
    'lsblk -o NAME,PARTLABEL,FSTYPE,SIZE; ls -l /dev/disk/by-partlabel/amp 2>&1' \
    >"$out/partitions.txt" 2>&1 || true
sha256sum "$out/rk3576-lubancat-3-v2.dtb" "$out/uEnv.txt" >"$out/hashes.txt"
dtc -I dtb -O dts -o "$out/rk3576-lubancat-3-v2.dts" \
    "$out/rk3576-lubancat-3-v2.dtb" 2>"$out/dtc.log"
printf 'Local evidence: %s\n' "$out"
