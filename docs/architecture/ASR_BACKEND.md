# ASR-01: Sherpa file backend

Status: `ASR_FILE_RECOGNITION_PASS` on Ubuntu 22.04 x86_64 Host with the official Sherpa v1.11.3 prebuilt C API package and an external reference model. This is a file pipeline result, not a live microphone, RK3576, accuracy benchmark or product distribution approval.

## Ownership and flow

`libs/audio::read_pcm_wav` validates and reads a RIFF/WAVE file; it has no device access. `tools/asr_file_test` creates a `VoiceSessionController` token (session ID, generation, request ID, boot epoch), transitions to `Recognizing`, and gives PCM to `SherpaAsrBackend : IAsrBackend`. The backend loads one recognizer for its lifetime, creates one Sherpa online stream per session, converts little-endian signed S16 samples to normalized floats, decodes chunks, and emits typed `AsrEvent{PARTIAL|FINAL|ERROR, token, sequence, text, status}`. The tool uses `deliver_event` to fence stale events and prints `ASR_FINAL` only after this check.

One backend instance serializes C API access. `push_audio` processes at most 16,000 samples per decode call and releases the backend mutex between chunks, allowing cancellation between calls. No detached thread or background worker is created. `cancel(token)` stops only the exact active stream; old-token cancellation cannot stop a new session. The controller rejects late old-generation/epoch events. The callback is synchronous, quick and non-reentrant; cancellation during a single Sherpa decode waits for that call, then prevents later chunks/final delivery. New orchestration must call both controller and backend cancellation before replacing a session. `unload()` closes stream then recognizer, and the destructor calls it.

Sherpa's streaming API yields changed nonempty intermediate text as `ASR_PARTIAL`; `finish_input` signals end of samples, drains decoding and emits one `ASR_FINAL`. No partial is fabricated for silence. The model is loaded once across repeated `--wav` arguments. The first version uses no endpoint/VAD/wake logic.

## WAV and errors

`read_pcm_wav` accepts bounded files up to 128 MiB with exact RIFF size, `WAVE`, one `fmt ` chunk, one nonempty `data` chunk and safe chunk-bound checks. It requires format 1 PCM, 16,000 Hz, one channel, 16 bits, block align 2 and byte rate 32,000. It handles unknown RIFF chunks with padding. It rejects truncated, oversized, malformed or unsupported files with `StatusCode` and specific detail such as `UNSUPPORTED_SAMPLE_RATE`; it never resamples. The backend also validates PCM format and session ID. Missing model files map to `UNAVAILABLE/MODEL_NOT_FOUND`, C API initialization to `UNAVAILABLE/MODEL_LOAD_FAILED`, bad audio to `INVALID_ARGUMENT`, cancellation to `CANCELLED`, and internal decoding failure to `INTERNAL_ERROR`.

## External dependency and test commands

Default `cmake --preset host-debug` and `bash scripts/dev/host_ci.sh` require no Sherpa. Set `COCKPIT_ENABLE_SHERPA_ASR=ON` and point `SHERPA_ONNX_ROOT` to an **absolute** directory containing `include/sherpa-onnx/c-api/c-api.h` and `lib/libsherpa-onnx-c-api.so`; explicit include/library CMake variables are also accepted. Missing dependency fails configure. Sherpa's runtime must find its adjacent `libonnxruntime.so`. No binary or model belongs in Git. For a separately prepared, licensed Host installation:

```sh
cmake -S . -B build/asr-sherpa-host -DCMAKE_BUILD_TYPE=Debug \
  -DCOCKPIT_ENABLE_SHERPA_ASR=ON -DSHERPA_ONNX_ROOT=/absolute/path/to/sherpa-prefix \
  -DCOCKPIT_SHERPA_MODEL_DIR=/absolute/path/to/model \
  -DCOCKPIT_SHERPA_TEST_WAV=/absolute/path/to/known.wav \
  -DCOCKPIT_SHERPA_EXPECT_CONTAINS=MONDAY
cmake --build build/asr-sherpa-host -j4
ctest --test-dir build/asr-sherpa-host -L sherpa-integration --output-on-failure
build/asr-sherpa-host/apps/voice_srv/cockpit_asr_file_test \
  --model-config config/models/asr/sherpa_file_asr.conf \
  --model-dir /absolute/path/to/model --wav /absolute/path/to/known.wav
```

The integration CTest exists only when the optional dependency and external model/WAV paths are configured. `--expect-contains` makes it fail if the actual final text lacks a known fragment. Multiple `--wav` arguments reuse the same recognizer. Output includes `MODEL_LOADED`, `AUDIO_INFO`, typed ASR lines and `HOST_BENCHMARK_ONLY` decode elapsed / audio duration (`RTF`). Host RTF says nothing about RK3576 performance.

The config manifest records expected file names/hashes and provenance; **ASR-01 does not verify hashes at runtime**. Validate provenance and the archive independently before product distribution. Code, model weights, bundled test audio and ONNX Runtime have separate license obligations; see the [dependency audit](../reviews/asr-sherpa-file/DEPENDENCY_AUDIT.md).

## Future boundary

Live capture belongs to `audio_srv` alone. A later task must negotiate actual format and resampling explicitly, pass session-tagged PCM to `voice_srv`, add a bounded worker/queue with deadlines, test cancellation during long decode, and validate model license, RAM, accuracy and latency on RK3576. ASR-01 does not route recognized text to IntentRouter or hardware.
