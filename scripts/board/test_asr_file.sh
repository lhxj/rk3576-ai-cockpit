#!/usr/bin/env bash
# File-only ASR validation. --load-only must pass before --full is attempted.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
phase=${1:-}
[[ "$phase" == --load-only || "$phase" == --full ]] || {
    echo 'Usage: test_asr_file.sh --load-only|--full' >&2; exit 2;
}
board_lock
umask 077
mkdir -p "$ROOT/build/asr-target-tests"
log="$ROOT/build/asr-target-tests/${phase#--}-$(date -u +%Y%m%dT%H%M%SZ).log"
echo "Board file ASR phase=$phase; local log=$log"
ssh "${SSH_ARGS[@]}" lubancat "bash -s -- '$phase'" <<'REMOTE' | tee "$log"
set -euo pipefail
phase=$1
base=/home/cat/cockpit/asr-target
cd "$base"
export LD_LIBRARY_PATH="$base/lib"
bin="$base/bin/cockpit_asr_file_test"
cfg="$base/config/sherpa_file_asr.conf"
model="$base/models"
wav="$base/testdata/0.wav"
child_pid=
cleanup() {
    if [[ -n "$child_pid" ]] && kill -0 "$child_pid" 2>/dev/null; then
        kill -TERM "$child_pid" 2>/dev/null || true
        wait "$child_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo "ENV machine=$(uname -m) kernel=$(uname -r) glibc=$(getconf GNU_LIBC_VERSION)"
sha256sum -c checksums.sha256
file "$bin" "$base/lib/libsherpa-onnx-c-api.so" "$base/lib/libonnxruntime.so"
ldd "$bin"
"$bin" --help

run_observed() {
    local label=$1 rc rss pss available cpu temp start bytes
    shift
    local output="$base/build/$label.log"
    local peak_rss=0 peak_pss=0 min_available=999999999 peak_cpu=0 peak_temp=0
    "$@" > "$output" 2>&1 &
    child_pid=$!
    start=$SECONDS
    while kill -0 "$child_pid" 2>/dev/null; do
        if [[ -r "/proc/$child_pid/status" ]]; then
            rss=$(awk '/^VmRSS:/ {print $2}' "/proc/$child_pid/status")
            [[ "$rss" =~ ^[0-9]+$ ]] && ((rss > peak_rss)) && peak_rss=$rss
        fi
        if [[ -r "/proc/$child_pid/smaps_rollup" ]]; then
            pss=$(awk '/^Pss:/ {print $2; exit}' "/proc/$child_pid/smaps_rollup")
            [[ "$pss" =~ ^[0-9]+$ ]] && ((pss > peak_pss)) && peak_pss=$pss
        fi
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
        if ((bytes > 1048576)); then
            echo "LOG_LIMIT_EXCEEDED label=$label; terminating only this task process" >&2
            kill -TERM "$child_pid" 2>/dev/null || true
            wait "$child_pid" 2>/dev/null || true
            child_pid=
            return 125
        fi
        if ((SECONDS - start > 120)); then
            echo "SAFETY_LIMIT_EXCEEDED label=$label; terminating only this task process" >&2
            kill -TERM "$child_pid" 2>/dev/null || true
            wait "$child_pid" 2>/dev/null || true
            child_pid=
            return 124
        fi
        sleep 0.1
    done
    set +e
    wait "$child_pid"
    rc=$?
    set -e
    child_pid=
    cat "$output"
    echo "OBSERVED label=$label exit=$rc peak_rss_kb=$peak_rss peak_pss_kb=$peak_pss min_mem_available_kb=$min_available peak_cpu_percent=$peak_cpu peak_thermal_mC=$peak_temp"
    return "$rc"
}

if [[ "$phase" == --load-only ]]; then
    run_observed load "$bin" --model-config "$cfg" --model-dir "$model" --load-only
    exit 0
fi

run_observed recognition "$bin" --model-config "$cfg" --model-dir "$model" --wav "$wav"
[[ $(grep -c '^ASR_FINAL ' "$base/build/recognition.log") -eq 1 ]]
run_observed repeated "$bin" --model-config "$cfg" --model-dir "$model" --wav "$wav" --wav "$wav" --wav "$wav"
[[ $(grep -c '^MODEL_LOADED ' "$base/build/repeated.log") -eq 1 ]]
[[ $(grep -c '^ASR_FINAL ' "$base/build/repeated.log") -eq 3 ]]
run_observed cancel "$base/bin/sherpa_file_cancel_test" "$cfg" "$model" "$wav"
grep -q '^SHERPA_CANCEL_SESSION_PASS$' "$base/build/cancel.log"

if run_observed bad_model "$bin" --model-config "$cfg" --model-dir "$base/models/missing" --wav "$wav"; then
    echo 'Expected bad model path to fail' >&2; exit 1
fi
if run_observed bad_wav "$bin" --model-config "$cfg" --model-dir "$model" --wav "$base/testdata/missing.wav"; then
    echo 'Expected missing WAV to fail' >&2; exit 1
fi
python3 - <<'PY'
import wave
with wave.open('/home/cat/cockpit/asr-target/testdata/unsupported-8k.wav', 'wb') as wav:
    wav.setnchannels(1)
    wav.setsampwidth(2)
    wav.setframerate(8000)
    wav.writeframes(b'\0\0' * 800)
PY
if run_observed bad_rate "$bin" --model-config "$cfg" --model-dir "$model" --wav "$base/testdata/unsupported-8k.wav"; then
    echo 'Expected unsupported sample rate to fail' >&2; exit 1
fi
if ps -eo comm= | grep -qE '^(cockpit_asr_fil|sherpa_file_can)'; then
    echo 'Residual ASR task process detected' >&2; exit 1
fi
echo 'BOARD_FILE_ASR_VALIDATION_PASS'
REMOTE
