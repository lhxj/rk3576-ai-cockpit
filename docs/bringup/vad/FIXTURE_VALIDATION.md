# Host fixture validation

2026-10-01, branch `agent/asr-rk3576-vad` based on `f2ddb76`. Local fixture comes from the existing external research model directory; it is not added to Git. `1.wav`: 5.1 s, SHA-256 `8bfb42c963e623ebab31b81ff4404867d07d3102507c87ac14577c4c61663b8c`, distribution license still unverified. The wrapper feeds 1 s synthetic silence, the file, then 1 s silence.

Host real Sherpa v1.11.3 + Silero v5.0 + `SherpaAsrBackend` + `VoiceSessionController`: one VAD start, one end, one new session, one `ASR_FINAL`: `这是第一种第二种叫呃与 ALWAYS什么意思啊`. Repeating the exact fixture twice with a long silence produces two starts/ends, two unique sessions, two FINALs with the same observed text; both models load once. VAD peak pre-roll was 4800 frames; sequence gaps 0. The longer `0.wav` legitimately contains multiple phrases and was split into three VAD utterances, so it is unsuitable as a one-utterance assertion; no decoder text was changed.

Deterministic Host test doubles cover silence, low noise, one utterance, two utterances, a short pause remaining one utterance, short burst rejection, onset sample present in the ASR pre-roll, maximum duration finalization, cancel without FINAL, restart after cancel, sequence gap error, queue overflow, normal worker shutdown, and 100 repeated utterances without model reload or ring growth. These tests verify the protocol/state machine; they do not measure Silero accuracy. Real model integration is separate and optional when assets are absent.

The original fixed-duration `LiveAsrPipeline` was also driven by a paced file-only `IAudioCapture` test adapter and the real Sherpa recognizer. It delivered one FINAL without queue overflow on Host and RK3576, preserving the fixed path without a new spoken test or changing ALSA ownership.
