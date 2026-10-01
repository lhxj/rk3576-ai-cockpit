# ASR Target Validation — RK3576 file recognition

## Goal

Advance the existing `052a60e` Host file backend to an observed RK3576/Debian 12 AArch64 file recognition result through `SherpaAsrBackend`, `IAsrBackend`, `VoiceSessionController`, and `ASR_FINAL`.

## Scope and baseline

- Work in independent worktree `agent/asr-rk3576-file-validation` based on `052a60e`; preserve the original branch and its untracked review material.
- Use official Sherpa-ONNX v1.11.3 AArch64 CPU shared release and the already verified bilingual Zipformer model and `0.wav`, with SHA256 comparison on both machines.
- Use a small native CMake build context on the board, deployed entirely under `/home/cat/cockpit/asr-target`; keep library lookup scoped to the test process.
- Observe ELF/NEEDED/version requirements before executing. Validate startup, load, file recognition, cancel, repeat, input failures, exit, and resource use. Repeat Host CI and x86 integration.

## Out of scope

No real audio device, microphone, PortAudio, ALSA, TTS, RKLLM, RKNN, camera, vehicle actions, AMP or RT-Thread. No root privileges, system install, firmware edits, model download, or Git tracked model/runtime/WAV assets.

## Dependencies

Sherpa-ONNX official v1.11.3 AArch64 CPU shared C API, bundled ONNX Runtime 1.17.1, and the five externally stored Zipformer assets plus `test_wavs/0.wav`. The AArch64 release has no header; the official same-version C header is supplied externally. Code is Apache-2.0, while the converted weights and test WAV remain `LICENSE_UNVERIFIED_FOR_DISTRIBUTION`.

## Deployment

Preview and run `scripts/board/deploy_asr_file.sh`, native-build through `scripts/board/build_asr_file.sh`, and validate load before recognition through `scripts/board/test_asr_file.sh`. All writes stay under `/home/cat/cockpit/asr-target`; asset digests are checked after transfer. See `docs/bringup/asr/DEPLOYMENT.md` for exact inputs.

## Safety and recovery

Read-only board inventory first; use the existing board lock for each deployment/test transaction and refuse concurrent use. Preview the exact transfer set before copying; only task-owned paths under `/home/cat/cockpit/asr-target` are written. Test commands have explicit bounds, a natural exit is required, and logs remain local/size bounded. On failure, leave evidence, resolve the specific fault, and never reset/clean the original tree or replace system libraries.

## Validation and result

`ASR_BOARD_FILE_RECOGNITION_PASS` — file input only. The official v1.11.3 AArch64 CPU shared package supplied Sherpa C API and ONNX Runtime 1.17.1. Target GCC 12.2 compiled and started the main-project executable and cancel test. Board SHA256 matched Host for the five model assets and `0.wav`. Model-only load, actual `ASR_FINAL`, old-session cancellation/new-session result, three sequential recognitions after one load, and expected error exits all passed. `bash scripts/dev/host_ci.sh` passed 9/9 CTest plus 6/6 Python; explicit x86 Sherpa integration passed 2/2. See `docs/bringup/asr/` for ABI, exact hashes, deployment procedure, actual text, timings, sampled resources and limitations.

The optional board code path uses neither a microphone nor an ALSA/PortAudio dependency. No model, WAV, `.so`, or `.a` entered Git. Model/WAV distribution rights and live-audio behavior remain unverified.
