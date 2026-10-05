#!/usr/bin/env bash
set -euo pipefail
[[ "$1" =~ ^[0-9]+([.][0-9]+)?$ ]] || exit 2
start=$SECONDS
opts=(-o HostName=10.232.249.223 -o BatchMode=yes -o ConnectTimeout=5 -o ServerAliveInterval=5 -o ServerAliveCountMax=2 -o StrictHostKeyChecking=yes)
deadline=$((SECONDS+45))
while (( SECONDS < deadline )); do
  if kernel=$(timeout --signal=TERM --kill-after=2s 7s ssh "${opts[@]}" lubancat 'uname -r'); then
    [[ "$kernel" == 6.1.99-rk3576-m0echo-p026 ]] || { echo 'STOP wrong runtime kernel' >&2; exit 1; }
    break
  fi
  sleep 1
done
[[ "${kernel:-}" == 6.1.99-rk3576-m0echo-p026 ]] || { echo 'STOP SSH45s deadline' >&2; exit 1; }
age=$(python3 -c 'import sys; print(float(sys.argv[1])+float(sys.argv[2])+1)' "$1" "$((SECONDS-start))")
timeout --signal=TERM --kill-after=5s 550s ssh "${opts[@]}" lubancat "sudo -n sh -c 'printf \"%s  %s\\n\" 3c429f2825a51a27ad25053d108111826c7ac29809b575532cf78468f3bf9d8b /home/cat/cockpit/mpu-sensor-entry-v4/run_sensor_t5_t6_entry_v4.py | sha256sum -c - && exec python3 /home/cat/cockpit/mpu-sensor-entry-v4/run_sensor_t5_t6_entry_v4.py --source-age-seconds $age'"
