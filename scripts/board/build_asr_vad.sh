#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
ssh "${SSH_ARGS[@]}" lubancat 'bash -s' <<'REMOTE'
set -euo pipefail
vad=/home/cat/cockpit/asr-vad
base=/home/cat/cockpit/asr-target
cmake -S "$vad/src" -B "$vad/build" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCOCKPIT_ENABLE_ALSA_CAPTURE=ON -DCOCKPIT_ENABLE_SHERPA_ASR=ON \
  -DSHERPA_ONNX_INCLUDE_DIR="$base/include" \
  -DSHERPA_ONNX_LIBRARY="$base/lib/libsherpa-onnx-c-api.so" \
  -DCOCKPIT_SHERPA_MODEL_DIR="$base/models" \
  -DCOCKPIT_SHERPA_TEST_WAV="$base/testdata/0.wav" \
  -DCOCKPIT_VAD_MODEL="$vad/models/silero_vad_v5.onnx" \
  -DCOCKPIT_VAD_TEST_WAV="$vad/testdata/1.wav"
cmake --build "$vad/build" -j2
cp "$vad/build/apps/voice_srv/cockpit_live_asr_test" "$vad/bin/"
cp "$vad/build/apps/voice_srv/cockpit_vad_fixture_test" "$vad/bin/"
file "$vad/bin/cockpit_live_asr_test" "$vad/bin/cockpit_vad_fixture_test"
readelf -d "$vad/bin/cockpit_vad_fixture_test" | grep -E 'NEEDED|RUNPATH|RPATH'
LD_LIBRARY_PATH="$base/lib" ldd "$vad/bin/cockpit_vad_fixture_test"
LD_LIBRARY_PATH="$base/lib" "$vad/bin/cockpit_live_asr_test" --help
REMOTE
