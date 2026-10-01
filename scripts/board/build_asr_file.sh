#!/usr/bin/env bash
# Compile the main project's file-ASR targets natively as the unprivileged cat user.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
board_lock
ssh "${SSH_ARGS[@]}" lubancat 'bash -s' <<'REMOTE'
set -euo pipefail
base=/home/cat/cockpit/asr-target
cd "$base"
cmake -S src -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCOCKPIT_ENABLE_SHERPA_ASR=ON \
  -DSHERPA_ONNX_INCLUDE_DIR="$base/include" \
  -DSHERPA_ONNX_LIBRARY="$base/lib/libsherpa-onnx-c-api.so" \
  -DCOCKPIT_SHERPA_MODEL_DIR="$base/models" \
  -DCOCKPIT_SHERPA_TEST_WAV="$base/testdata/0.wav"
cmake --build build --target cockpit_asr_file_test sherpa_file_cancel_test -j2
cp build/apps/voice_srv/cockpit_asr_file_test build/apps/voice_srv/sherpa_file_cancel_test bin/
file bin/cockpit_asr_file_test bin/sherpa_file_cancel_test
readelf -d bin/cockpit_asr_file_test | grep -E 'NEEDED|RUNPATH|RPATH'
LD_LIBRARY_PATH="$base/lib" ldd bin/cockpit_asr_file_test
LD_LIBRARY_PATH="$base/lib" bin/cockpit_asr_file_test --help
REMOTE
