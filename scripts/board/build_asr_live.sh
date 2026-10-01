#!/usr/bin/env bash
# Native AArch64 build in task user directory; no system install.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
ssh "${SSH_ARGS[@]}" lubancat 'bash -s' <<'REMOTE'
set -euo pipefail
live=/home/cat/cockpit/asr-live
file_base=/home/cat/cockpit/asr-target
cmake -S "$live/src" -B "$live/build" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCOCKPIT_ENABLE_ALSA_CAPTURE=ON -DCOCKPIT_ENABLE_SHERPA_ASR=ON \
  -DSHERPA_ONNX_INCLUDE_DIR="$file_base/include" \
  -DSHERPA_ONNX_LIBRARY="$file_base/lib/libsherpa-onnx-c-api.so" \
  -DCOCKPIT_SHERPA_MODEL_DIR="$file_base/models" \
  -DCOCKPIT_SHERPA_TEST_WAV="$file_base/testdata/0.wav"
cmake --build "$live/build" --target cockpit_live_asr_test -j2
cp "$live/build/apps/voice_srv/cockpit_live_asr_test" "$live/bin/"
file "$live/bin/cockpit_live_asr_test"
readelf -d "$live/bin/cockpit_live_asr_test" | grep -E 'NEEDED|RUNPATH|RPATH'
LD_LIBRARY_PATH="$file_base/lib" ldd "$live/bin/cockpit_live_asr_test"
LD_LIBRARY_PATH="$file_base/lib" "$live/bin/cockpit_live_asr_test" --help
REMOTE
