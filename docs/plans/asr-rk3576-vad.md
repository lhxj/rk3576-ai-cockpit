# RK3576 VAD utterance segmentation

## Goal

Add automatic speech boundaries to the already validated live microphone ASR path. Keep the fixed duration mode intact. The target grade is based on actual Host and RK3576 evidence, not speech recognition accuracy.

## Scope

Sherpa-ONNX v1.11.3 Silero VAD C API, an external small VAD model, an `IVadBackend` boundary, bounded pre-roll, per utterance ASR sessions, an ordered PCM consumer, cancellation, fixture tests, and a bounded board run. The ASR and VAD models load once. No wake word, intent, playback, or hardware action is connected.

## Interface change

The foundation's unused `IVadBackend::detect(PcmBuffer) -> bool` cannot express speech start/end, timestamps, flush, or reset. Replace that placeholder with `configure/reset/accept_samples/flush/state` and structured `VadEvent`. Leave `IAsrBackend`, `VoiceSessionController`, `SessionToken`, and `BoundedQueue` contracts unchanged.

## Dependencies and license

Use the deployed Sherpa-ONNX v1.11.3 C API and ONNX Runtime 1.17.1. The optional Sherpa target remains disabled by default. The VAD model is fetched into an ignored build directory and later a board user directory; it is never committed. Silero model distribution clearance is tracked separately from Apache-2.0 Sherpa code.

## Implementation

`audio_srv` retains sole ALSA ownership. A new VAD pipeline owns one capture worker and one ordered processing worker with the existing bounded PCM queue. A synchronous utterance processor handles VAD events, a fixed size pre-roll ring, per utterance session/stream creation, finalization, and cancellation. The existing fixed duration `LiveAsrPipeline` remains available.

## Validation

Run deterministic mock VAD/ASR fixtures, an optional real Sherpa VAD+ASR WAV fixture, Host CI, board fixture, board ALSA startup/shutdown, and bounded stability. No new human speech test is required.

## Result

Implemented on an isolated worktree. Default Host CI: 11/11 CTest and 6/6 Python; optional x86 Sherpa integration: 4/4. Deterministic Host VAD test also passed with ASan/UBSan. The pinned Silero v5.0 model produced one and two automatic utterances from the known external `1.wav` fixture on both Host and RK3576; each produced actual `ASR_FINAL` through the session controller. Board native AArch64 build, existing file ASR and paced fixed `LiveAsrPipeline` regressions, ALSA open/close, and 200 prerecorded utterances passed. A five-second live run observed one speech start but no end before its limit. A later 20-second quiet live run consumed real PCM without a false start, queue overflow or XRUN. Neither proves a controlled live speech-end/FINAL. The supported grade is **`VAD_FILE_PIPELINE_PASS`**. No new user speech, wake/intent/TTS/LLM work was requested or started.
