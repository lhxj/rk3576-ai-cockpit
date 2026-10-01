#!/usr/bin/env bash
# Minimal native-build context. Reuse previously verified ASR assets by path.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"
mode=${1:---dry-run}
[[ "$mode" == --dry-run || "$mode" == --apply ]] || {
    echo 'Usage: deploy_asr_live.sh [--dry-run|--apply]' >&2; exit 2;
}
remote=/home/cat/cockpit/asr-live
echo "Mode: $mode; destination: $remote"
echo 'Copies: CMakeLists.txt, selected libs/apps/tools/tests/config source only.'
echo 'Reuses: /home/cat/cockpit/asr-target/{lib,include,models,config,testdata} after SHA256 check.'
echo 'Does not copy model, WAV, binary runtime, or any system configuration.'
[[ "$mode" == --apply ]] || exit 0
board_lock
stage="$ROOT/build/asr-live-deploy"
mkdir -p -- "$stage"
tar -czf "$stage/src.tar.gz" -C "$ROOT" CMakeLists.txt libs/common libs/protocol libs/ipc \
    libs/audio apps/audio_srv apps/voice_srv apps/infer_srv tools/host_smoke \
    tools/asr_file_test tools/live_asr_test tests/unit tests/integration config/models/asr
sha256sum "$stage/src.tar.gz" | awk '{print $1}' > "$stage/src.sha256"
ssh "${SSH_ARGS[@]}" lubancat 'bash -s' <<'REMOTE'
set -euo pipefail
file_base=/home/cat/cockpit/asr-target
live=/home/cat/cockpit/asr-live
cd "$file_base"
sha256sum -c checksums.sha256
test -f /usr/include/alsa/asoundlib.h
mkdir -p "$live/src" "$live/build" "$live/bin" "$live/artifacts" "$live/logs"
REMOTE
scp -q "${SSH_ARGS[@]}" "$stage/src.tar.gz" "$stage/src.sha256" "lubancat:$remote/"
ssh "${SSH_ARGS[@]}" lubancat 'bash -s' <<'REMOTE'
set -euo pipefail
cd /home/cat/cockpit/asr-live
printf '%s  src.tar.gz\n' "$(cat src.sha256)" | sha256sum -c -
tar -xzf src.tar.gz -C src
echo ASR_LIVE_SOURCE_DEPLOYED
REMOTE
