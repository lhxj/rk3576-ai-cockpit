#!/usr/bin/env bash
# Explicit optional x86 Host regression; external v1.11.3 runtime/model/WAV required.
set -euo pipefail
if (($# != 4)); then
    echo 'Usage: test_sherpa_file_integration.sh SHERPA_INCLUDE_DIR SHERPA_C_API_SO MODEL_DIR WAV' >&2
    exit 2
fi
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cmake -S "$root" -B "$root/build/asr-sherpa-host" \
    -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DCOCKPIT_ENABLE_SHERPA_ASR=ON \
    -DSHERPA_ONNX_INCLUDE_DIR="$1" -DSHERPA_ONNX_LIBRARY="$2" \
    -DCOCKPIT_SHERPA_MODEL_DIR="$3" -DCOCKPIT_SHERPA_TEST_WAV="$4"
cmake --build "$root/build/asr-sherpa-host" --target cockpit_asr_file_test sherpa_file_cancel_test -j2
ctest --test-dir "$root/build/asr-sherpa-host" -L sherpa-integration --output-on-failure
