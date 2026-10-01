#!/usr/bin/env bash
# Isolated user-directory deployment. No system files, package management, or service changes.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
mode=${1:---dry-run}
[[ "$mode" == --dry-run || "$mode" == --apply ]] || {
    echo 'Usage: deploy_asr_vad.sh [--dry-run|--apply]' >&2; exit 2;
}
remote=/home/cat/cockpit/asr-vad
vad_model="$ROOT/build/vad-deps/silero_vad_v5.onnx"
wav="$ROOT/../../reference/LLM_Voice_Flow/voice/models/sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16/test_wavs/1.wav"
vad_hash=6b99cbfd39246b6706f98ec13c7c50c6b299181f2474fa05cbc8046acc274396
wav_hash=8bfb42c963e623ebab31b81ff4404867d07d3102507c87ac14577c4c61663b8c
echo "Mode: $mode; destination: $remote"
echo 'Copies selected source, pinned 2.3 MB Silero v5 model, and one local research WAV.'
echo 'Reuses /home/cat/cockpit/asr-target runtime and ASR weights after checksum verification.'
echo 'Writes no system paths and does not overwrite the validated asr-live binary.'
[[ "$mode" == --apply ]] || exit 0
test -f "$vad_model" && test -f "$wav"
printf '%s  %s\n' "$vad_hash" "$vad_model" "$wav_hash" "$wav" | sha256sum -c -
board_lock
stage="$ROOT/build/vad-deploy"
mkdir -p -- "$stage"
tar -czf "$stage/src.tar.gz" -C "$ROOT" CMakeLists.txt libs/common libs/protocol libs/ipc \
    libs/audio apps/audio_srv apps/voice_srv apps/infer_srv tools/host_smoke \
    tools/asr_file_test tools/live_asr_test tools/vad_fixture_test \
    tests/unit tests/integration config/models/asr
sha256sum "$stage/src.tar.gz" | awk '{print $1}' > "$stage/src.sha256"
ssh "${SSH_ARGS[@]}" lubancat 'bash -s' <<'REMOTE'
set -euo pipefail
cd /home/cat/cockpit/asr-target
sha256sum -c checksums.sha256
test -f /usr/include/alsa/asoundlib.h
mkdir -p /home/cat/cockpit/asr-vad/{src,build,bin,models,testdata,logs}
REMOTE
scp -q "${SSH_ARGS[@]}" "$stage/src.tar.gz" "$stage/src.sha256" \
    "lubancat:$remote/"
scp -q "${SSH_ARGS[@]}" "$vad_model" "lubancat:$remote/models/silero_vad_v5.onnx"
scp -q "${SSH_ARGS[@]}" "$wav" "lubancat:$remote/testdata/1.wav"
ssh "${SSH_ARGS[@]}" lubancat 'bash -s' <<'REMOTE'
set -euo pipefail
cd /home/cat/cockpit/asr-vad
printf '%s  src.tar.gz\n' "$(cat src.sha256)" | sha256sum -c -
printf '%s  models/silero_vad_v5.onnx\n' 6b99cbfd39246b6706f98ec13c7c50c6b299181f2474fa05cbc8046acc274396 | sha256sum -c -
printf '%s  testdata/1.wav\n' 8bfb42c963e623ebab31b81ff4404867d07d3102507c87ac14577c4c61663b8c | sha256sum -c -
tar -xzf src.tar.gz -C src
echo ASR_VAD_SOURCE_DEPLOYED
REMOTE
