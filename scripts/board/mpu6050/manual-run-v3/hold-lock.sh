#!/usr/bin/env bash
set -euo pipefail
source /home/ywx/rk3576-work/worktrees/rk3576-mpu6050/project/scripts/board/_common.sh
board_lock
echo MANUAL_BOARD_LOCK_READY
deadline=$((SECONDS+1200))
while (( SECONDS < deadline )); do
  [[ -f "$1" ]] && exit 0
  sleep 0.2
done
echo 'MANUAL_BOARD_LOCK_DEADLINE' >&2
exit 1
