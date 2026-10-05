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
timeout --signal=TERM --kill-after=5s 550s ssh "${opts[@]}" lubancat "sudo -n sh -c 'printf \"%s  %s\\n\" 83505f2f7eea4af5cc35d9dbf6562eae8c27a2ddc47285330556f1c2451e2742 /home/cat/cockpit/mpu-sensor-pa-arbitration-v2/run_sensor_pa_arbitrated_v2.py | sha256sum -c - && exec python3 /home/cat/cockpit/mpu-sensor-pa-arbitration-v2/run_sensor_pa_arbitrated_v2.py --source-age-seconds $age'"
