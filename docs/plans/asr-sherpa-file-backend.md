# P005 / VOICE-05: Sherpa file ASR backend

## Goal

Feed a validated 16 kHz mono S16 PCM WAV through our `IAsrBackend`, guarded by `VoiceSessionController`, and emit a typed `ASR_FINAL`. Keep Sherpa and model assets external to product Git.

## Existing evidence and unknowns

- Baseline: `agent/voice-ai-foundation` commit `42c3c11`; new branch `agent/asr-sherpa-file-backend` was created from it without changing pre-existing untracked audit files.
- Reference: local `LLM_Voice_Flow` HEAD `be82e87cc334ae6e222f83f7555531d1ddebaa8b`; Sherpa subtree declares 1.11.3. See `docs/reviews/asr-sherpa-file/DEPENDENCY_AUDIT.md`.
- Local ASR weights and WAV fixtures exist only under the reference tree. Sherpa C API library is not installed in the Host at planning time. Model package redistribution license and exact upstream vendored commit remain unverified. The [official v1.11.3 release](https://github.com/k2-fsa/sherpa-onnx/releases/tag/v1.11.3) lists `sherpa-onnx-v1.11.3-linux-x64-shared-no-tts.tar.bz2` (19,346,494 bytes) as a bounded Host dependency candidate.

## Scope and files

Add `libs/audio` WAV reader, `apps/voice_srv` Sherpa engine/backend and file runner, model manifest under `config/models/asr`, CMake option, unit/optional integration tests, architecture and task documentation. No IAsrBackend signature change is planned. No reference glue, model, or binary is copied. No board, audio device, ZeroMQ or other model service is touched.

## Permission and resource level

Local repository source/docs/tests and ignored Host build trees only. Reference tree read-only. Default CI uses CPU and no model. Optional Sherpa integration may use the official 19.3 MB release archive exclusively under ignored `build/asr-deps/`, after inspecting archive members; it uses the external model path and performs no system install or model download. No shell code from the archive is executed.

## Steps and validation

1. Record dependency and model evidence, including separate code/model license status.
2. Implement bounded RIFF PCM reader and tests; reject unsupported sample rate/channel/bit depth explicitly.
3. Implement an injected Sherpa engine boundary plus `SherpaAsrBackend` with recognizer load once, stream per session, serial decode, typed partial/final and cancellation fencing. Implement a real C API adapter behind an OFF-by-default CMake option.
4. Add a file CLI that creates a voice session, pushes PCM and reports the typed final result plus Host-only elapsed/RTF. Keep any real integration test separately labelled.
5. Run `bash scripts/dev/host_ci.sh`; try the bounded official Host package in ignored `build/asr-deps/` and run dependency-on configure/build and a real reference WAV if extraction and linking are safe. Record exact failures rather than promoting the completion grade.

Expected default result: Host CI passes without Sherpa. Dependency-on configure fails with a clear message if an external Sherpa installation is absent. Failure recovery: revert only task-owned commits/files, never reset or clean the shared tree. Remaining handoff: external Sherpa/ORT build, distribution license decision, real recognition fixture and then later live-audio work as separate tasks.

## Actual result

`ASR_FILE_RECOGNITION_PASS` for Host file input. `bash scripts/dev/host_ci.sh` passed (9/9 CTest and 6/6 Python). With no external package, `cmake -S . -B build/asr-dependency-check -DCOCKPIT_ENABLE_SHERPA_ASR=ON` failed intentionally with `Sherpa ASR requested but dependency not found`. A direct `curl` download stalled and failed with SSL EOF at 0 bytes; one alternative `gh release download v1.11.3 -R k2-fsa/sherpa-onnx` succeeded for the 19,346,494-byte no-TTS x64 shared archive. Its relative members were inspected before extraction into ignored `build/asr-deps/`; no installer ran. Archive SHA256: `84ce8e14e4d4aa692f8ecd9745b5a73a4b9103809cadbb7e645c0b5c9ea853bb`.

Sherpa ON CMake configure/build passed with an absolute external prefix. `ctest --test-dir build/asr-sherpa-host -L sherpa-integration --output-on-failure` passed 2/2, including real-engine cancellation of an old session followed by a new session. The external reference `test_wavs/0.wav` (10.053 s) produced `ASR_FINAL 昨天是 MONDAY TODAY IS THEY AFTER TOMORROW是星期三`, a decoded Host elapsed of 0.24 s and RTF about 0.024. `0.wav` and `1.wav` were then decoded in one process after a single recognizer load. The WAV reader independently parsed the same external `0.wav`. No real audio device, board or third-party code/model copy was used.

Handoff: confirm conversion-package and test-audio distribution licenses, add independent runtime hash verification if product provenance requires it, assess cancellation latency with a long fixture, prepare an approved external dependency location for other machines, and only then scope live capture as a separate task. Host RTF is not a RK3576 estimate.
