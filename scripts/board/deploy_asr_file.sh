#!/usr/bin/env bash
# Deploy only the file-ASR native build context and external v1.11.3 assets.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/_common.sh"

mode=dry-run model_dir= wav= sherpa_lib_dir= sherpa_header=
usage() {
    echo 'Usage: deploy_asr_file.sh [--dry-run|--apply] --model-dir DIR --wav FILE --sherpa-lib-dir DIR --sherpa-header FILE'
}
while (($#)); do
    case "$1" in
        --dry-run) mode=dry-run; shift ;;
        --apply) mode=apply; shift ;;
        --model-dir|--wav|--sherpa-lib-dir|--sherpa-header)
            (($# >= 2)) || { usage >&2; exit 2; }
            case "$1" in
                --model-dir) model_dir=$2 ;;
                --wav) wav=$2 ;;
                --sherpa-lib-dir) sherpa_lib_dir=$2 ;;
                --sherpa-header) sherpa_header=$2 ;;
            esac
            shift 2 ;;
        *) usage >&2; exit 2 ;;
    esac
done
[[ -n "$model_dir" && -n "$wav" && -n "$sherpa_lib_dir" && -n "$sherpa_header" ]] || { usage >&2; exit 2; }
models=(encoder-epoch-99-avg-1.int8.onnx decoder-epoch-99-avg-1.onnx
        joiner-epoch-99-avg-1.int8.onnx tokens.txt bpe.model)
for name in "${models[@]}"; do [[ -f "$model_dir/$name" ]] || { echo "Missing model: $name" >&2; exit 3; }; done
[[ -f "$wav" && -f "$sherpa_header" ]] || { echo 'Missing WAV or C API header' >&2; exit 3; }
for name in libsherpa-onnx-c-api.so libonnxruntime.so; do
    [[ -f "$sherpa_lib_dir/$name" ]] || { echo "Missing AArch64 library: $name" >&2; exit 3; }
    file "$sherpa_lib_dir/$name" | grep -q 'ARM aarch64' || { echo "Library is not AArch64: $name" >&2; exit 3; }
done

remote=/home/cat/cockpit/asr-target
echo "Mode: $mode; board: lubancat; destination: $remote"
echo 'Source context: CMakeLists.txt, selected libs/apps/tools/tests, config/models/asr (no assets)'
for name in "${models[@]}"; do printf 'Model input: %s/%s\n' "$model_dir" "$name"; done
printf 'Libraries: %s/{libsherpa-onnx-c-api.so,libonnxruntime.so}\n' "$sherpa_lib_dir"
printf 'C API header: %s\nWAV: %s\n' "$sherpa_header" "$wav"
echo 'Writes: src/, lib/, include/, models/, testdata/, config/, checksums.sha256 under destination only.'
[[ "$mode" == apply ]] || exit 0

board_lock
stage="$ROOT/build/asr-target-deploy"
mkdir -p -- "$stage"
cd "$ROOT"
tar -czf "$stage/src.tar.gz" CMakeLists.txt libs/common libs/protocol libs/ipc libs/audio \
    apps/audio_srv apps/voice_srv apps/infer_srv tools/host_smoke tools/asr_file_test \
    tests/unit tests/integration config/models/asr

hash_line() {
    local digest
    digest=$(sha256sum -- "$1")
    printf '%s  %s\n' "${digest%% *}" "$2"
}
{
    hash_line "$stage/src.tar.gz" src.tar.gz
    hash_line "$sherpa_header" include/sherpa-onnx/c-api/c-api.h
    hash_line "$wav" testdata/0.wav
    hash_line "$sherpa_lib_dir/libsherpa-onnx-c-api.so" lib/libsherpa-onnx-c-api.so
    hash_line "$sherpa_lib_dir/libonnxruntime.so" lib/libonnxruntime.so
    for name in "${models[@]}"; do hash_line "$model_dir/$name" "models/$name"; done
} > "$stage/checksums.sha256"

ssh "${SSH_ARGS[@]}" lubancat 'mkdir -p /home/cat/cockpit/asr-target/{src,bin,build,lib,include/sherpa-onnx/c-api,models,testdata,config}'
scp -q "${SSH_ARGS[@]}" "$stage/src.tar.gz" "$stage/checksums.sha256" "lubancat:$remote/"
scp -q "${SSH_ARGS[@]}" "$sherpa_header" "lubancat:$remote/include/sherpa-onnx/c-api/c-api.h"
scp -q "${SSH_ARGS[@]}" "$sherpa_lib_dir/libsherpa-onnx-c-api.so" "$sherpa_lib_dir/libonnxruntime.so" "lubancat:$remote/lib/"
scp -q "${SSH_ARGS[@]}" "$wav" "lubancat:$remote/testdata/0.wav"
for name in "${models[@]}"; do scp -q "${SSH_ARGS[@]}" "$model_dir/$name" "lubancat:$remote/models/$name"; done
ssh "${SSH_ARGS[@]}" lubancat 'cd /home/cat/cockpit/asr-target && sha256sum -c checksums.sha256 && tar -xzf src.tar.gz -C src && cp src/config/models/asr/sherpa_file_asr.conf config/'
echo 'Deployment and board-side SHA256 verification passed.'
