#!/usr/bin/env bash
# One bounded board microphone gate per invocation. No playback or PCM storage.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
phase= device=hw:0,0
while (($#)); do
    case "$1" in
        --phase) phase=${2:-}; shift 2 ;;
        --device) device=${2:-}; shift 2 ;;
        *) echo 'Usage: test_asr_live.sh --phase T0|T1|T2|T3|T4|T5 [--device PCM]' >&2; exit 2 ;;
    esac
done
[[ "$phase" =~ ^T[0-5]$ && "$device" =~ ^[a-zA-Z0-9_,.:+-]+$ ]] || exit 2
board_lock
umask 077
mkdir -p "$ROOT/build/asr-live-tests"
log="$ROOT/build/asr-live-tests/${phase}-$(date -u +%Y%m%dT%H%M%SZ).log"
echo "Board live ASR phase=$phase device=$device; local text log=$log"
ssh "${SSH_ARGS[@]}" lubancat "bash -s -- '$phase' '$device'" <<'REMOTE' | tee "$log"
set -euo pipefail
phase=$1 device=$2
live=/home/cat/cockpit/asr-live
file_base=/home/cat/cockpit/asr-target
bin="$live/bin/cockpit_live_asr_test"
cfg="$file_base/config/sherpa_file_asr.conf"
model="$file_base/models"
cd "$file_base"
sha256sum -c checksums.sha256
echo "ENV machine=$(uname -m) device=$device"
cat /proc/asound/cards
arecord -l
if [[ "$device" == hw:0,0 ]]; then
    test -e /dev/snd/pcmC0D0c || { echo 'AUDIO_DEVICE_MISSING' >&2; exit 3; }
    if fuser /dev/snd/pcmC0D0c >/dev/null 2>&1; then
        echo 'AUDIO_DEVICE_BUSY: capture PCM held by another process' >&2; exit 4
    fi
fi
if ps -eo args= | grep -E '[/]cockpit_live_asr_test' | grep -v grep; then
    echo 'Another live ASR task process exists' >&2; exit 5
fi
test -f "$bin"
file "$bin"
export LD_LIBRARY_PATH="$file_base/lib"
"$bin" --help
echo "THERMAL_BEFORE $(cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | tr '\n' ',' || true)"
child_pid= tail_pid=
cleanup() {
    if [[ -n "$child_pid" ]] && kill -0 "$child_pid" 2>/dev/null; then
        kill -TERM "$child_pid" 2>/dev/null || true
        wait "$child_pid" 2>/dev/null || true
    fi
    if [[ -n "$tail_pid" ]]; then wait "$tail_pid" 2>/dev/null || true; fi
}
trap cleanup EXIT
run_observed() {
    local rc rss pss available cpu temp bytes start
    local peak_rss=0 peak_pss=0 min_available=999999999 peak_cpu=0 peak_temp=0
    local output="$live/build/$phase.log"
    "$@" > "$output" 2>&1 & child_pid=$!
    tail -n +1 -f --pid="$child_pid" "$output" & tail_pid=$!
    start=$SECONDS
    while kill -0 "$child_pid" 2>/dev/null; do
        rss=$(awk '/^VmRSS:/ {print $2}' "/proc/$child_pid/status" 2>/dev/null || true)
        [[ "$rss" =~ ^[0-9]+$ ]] && ((rss > peak_rss)) && peak_rss=$rss
        pss=$(awk '/^Pss:/ {print $2; exit}' "/proc/$child_pid/smaps_rollup" 2>/dev/null || true)
        [[ "$pss" =~ ^[0-9]+$ ]] && ((pss > peak_pss)) && peak_pss=$pss
        available=$(awk '/^MemAvailable:/ {print $2}' /proc/meminfo)
        [[ "$available" =~ ^[0-9]+$ ]] && ((available < min_available)) && min_available=$available
        cpu=$(ps -p "$child_pid" -o %cpu= 2>/dev/null | tr -d ' ' || true)
        [[ "$cpu" =~ ^[0-9]+([.][0-9]+)?$ ]] && peak_cpu=$(awk -v old="$peak_cpu" -v now="$cpu" 'BEGIN {if (now>old) print now; else print old}')
        for zone in /sys/class/thermal/thermal_zone*/temp; do
            [[ -r "$zone" ]] || continue
            read -r temp < "$zone"
            [[ "$temp" =~ ^[0-9]+$ ]] && ((temp > peak_temp)) && peak_temp=$temp
        done
        bytes=$(stat -c %s "$output")
        if ((bytes > 1048576 || SECONDS - start > 120)); then
            echo 'SAFETY_LIMIT_EXCEEDED; terminating only this task process' >&2
            kill -TERM "$child_pid" 2>/dev/null || true
            wait "$child_pid" 2>/dev/null || true
            child_pid=
            return 124
        fi
        sleep 0.1
    done
    set +e
    wait "$child_pid"; rc=$?
    wait "$tail_pid"; set -e
    child_pid= tail_pid=
    echo "OBSERVED phase=$phase exit=$rc peak_rss_kb=$peak_rss peak_pss_kb=$peak_pss min_mem_available_kb=$min_available peak_cpu_percent=$peak_cpu peak_thermal_mC=$peak_temp"
    return "$rc"
}
case "$phase" in
    T0) run_observed "$bin" --device "$device" --probe ;;
    T1) run_observed "$bin" --device "$device" --capture-only --duration 2 ;;
    T2) run_observed "$bin" --device "$device" --duration 5 --model-config "$cfg" --model-dir "$model" ;;
    T3) run_observed "$bin" --device "$device" --duration 10 --cancel-after 2 --model-config "$cfg" --model-dir "$model" ;;
    T4) run_observed "$bin" --device "$device" --duration 5 --cancel-after 2 --session-count 2 --model-config "$cfg" --model-dir "$model" ;;
    T5) run_observed "$bin" --device "$device" --duration 5 --session-count 3 --model-config "$cfg" --model-dir "$model" ;;
esac
if [[ "$phase" == T1 ]]; then grep -q '^AUDIO_CAPTURE_METRICS ' "$live/build/$phase.log"; fi
if [[ "$phase" == T2 ]]; then [[ $(grep -c '^ASR_FINAL ' "$live/build/$phase.log") -eq 1 ]]; fi
if [[ "$phase" == T3 ]]; then
    [[ $(grep -c '^SESSION_CANCELLED ' "$live/build/$phase.log") -eq 1 ]]
    ! grep -q '^ASR_FINAL ' "$live/build/$phase.log"
fi
if [[ "$phase" == T4 ]]; then [[ $(grep -c '^ASR_FINAL ' "$live/build/$phase.log") -eq 1 ]]; fi
if [[ "$phase" == T5 ]]; then
    [[ $(grep -c '^MODEL_LOADED ' "$live/build/$phase.log") -eq 1 ]]
    [[ $(grep -c '^ASR_FINAL ' "$live/build/$phase.log") -eq 3 ]]
fi
echo "THERMAL_AFTER $(cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | tr '\n' ',' || true)"
if ps -eo args= | grep -E '[/]cockpit_live_asr_test' | grep -v grep; then
    echo 'Residual live ASR process' >&2; exit 6
fi
echo "${phase}_PASS"
REMOTE
