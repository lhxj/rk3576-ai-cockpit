#!/usr/bin/env bash
# Bounded RK3576 tests. Fixture mode never opens ALSA; live mode never saves PCM.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
mode=${1:-}
[[ "$mode" == fixture || "$mode" == audio-probe || "$mode" == fixed-capture ||
   "$mode" == live-idle || "$mode" == live-idle-long || "$mode" == stability ]] || {
    echo 'Usage: test_asr_vad.sh fixture|audio-probe|fixed-capture|live-idle|live-idle-long|stability' >&2; exit 2;
}
board_lock
ssh "${SSH_ARGS[@]}" lubancat 'bash -s' -- "$mode" <<'REMOTE'
set -euo pipefail
mode=$1
vad=/home/cat/cockpit/asr-vad
base=/home/cat/cockpit/asr-target
export LD_LIBRARY_PATH="$base/lib"
cd "$vad"
printf '%s  models/silero_vad_v5.onnx\n' 6b99cbfd39246b6706f98ec13c7c50c6b299181f2474fa05cbc8046acc274396 | sha256sum -c -
printf '%s  testdata/1.wav\n' 8bfb42c963e623ebab31b81ff4404867d07d3102507c87ac14577c4c61663b8c | sha256sum -c -
cd "$base" && sha256sum -c checksums.sha256 && cd "$vad"
case "$mode" in
  fixture)
    ctest --test-dir build -R '^(vad_pipeline_test|sherpa_file_integration|sherpa_vad_fixture_integration|sherpa_fixed_live_file_regression)$' --output-on-failure
    timeout -k 5s 120s "$vad/bin/cockpit_vad_fixture_test" \
      --model-config "$vad/src/config/models/asr/sherpa_file_asr.conf" \
      --model-dir "$base/models" --vad-model "$vad/models/silero_vad_v5.onnx" \
      --wav "$vad/testdata/1.wav" --repeat 2 --expect-final 2
    ;;
  audio-probe)
    timeout -k 5s 15s "$vad/bin/cockpit_live_asr_test" --device hw:0,0 --probe
    ;;
  fixed-capture)
    timeout -k 5s 15s "$vad/bin/cockpit_live_asr_test" --mode fixed --device hw:0,0 \
      --capture-only --duration 2
    ;;
  live-idle)
    timeout -k 5s 30s "$vad/bin/cockpit_live_asr_test" --mode vad --device hw:0,0 \
      --model-config "$vad/src/config/models/asr/sherpa_file_asr.conf" \
      --model-dir "$base/models" --vad-model "$vad/models/silero_vad_v5.onnx" \
      --run-for 5 --max-utterances 3
    ;;
  live-idle-long)
    timeout -k 5s 50s "$vad/bin/cockpit_live_asr_test" --mode vad --device hw:0,0 \
      --model-config "$vad/src/config/models/asr/sherpa_file_asr.conf" \
      --model-dir "$base/models" --vad-model "$vad/models/silero_vad_v5.onnx" \
      --run-for 20 --max-utterances 3
    ;;
  stability)
    timeout -k 5s 900s "$vad/bin/cockpit_vad_fixture_test" \
      --model-config "$vad/src/config/models/asr/sherpa_file_asr.conf" \
      --model-dir "$base/models" --vad-model "$vad/models/silero_vad_v5.onnx" \
      --wav "$vad/testdata/1.wav" --repeat 200 --expect-final 200 \
      > "$vad/logs/stability.txt" 2>&1
    grep 'VAD_FIXTURE_METRICS\|RESOURCE_PHASE' "$vad/logs/stability.txt"
    ;;
esac
REMOTE
