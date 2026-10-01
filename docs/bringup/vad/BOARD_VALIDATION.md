# RK3576 VAD board validation

Target: LubanCat-3 v2, RK3576, Debian 12 AArch64. Runtime: Sherpa-ONNX v1.11.3 C API with ONNX Runtime 1.17.1, reused from the already validated file ASR deployment in `/home/cat/cockpit/asr-target`. Task files reside only in `/home/cat/cockpit/asr-vad`; no system library or audio configuration changed. The board SHA-256 check matched the pinned VAD v5.0 model and `1.wav`, and rechecked the existing ASR model/runtime manifest.

Reproduction from this branch: `bash scripts/board/deploy_asr_vad.sh --dry-run`, then `--apply`; `bash scripts/board/build_asr_vad.sh`; `bash scripts/board/test_asr_vad.sh fixture|audio-probe|fixed-capture|live-idle-long|stability`. Each script takes the shared board lock, verifies asset hashes, and writes only below `/home/cat/cockpit/`. The long live mode has a 20 s in-app limit and a 50 s outer guard. No script saves microphone PCM.

2026-10-01 completed:

- Native AArch64 build succeeded. Executables are ELF AArch64 with `/lib/ld-linux-aarch64.so.1`; `ldd` resolved Sherpa and ONNX Runtime from the project user directory.
- Board `vad_pipeline_test`, file ASR regression, and VAD+Sherpa one-utterance fixture integration: 3/3 PASS.
- Separate two-utterance fixture: two VAD starts/ends, two ASR FINALs in distinct sessions, same observed text `这是第一种第二种叫呃与 ALWAYS什么意思啊`; ASR and VAD load counts 1 each; peak pre-roll 4800 frames, sequence gaps 0.
- ALSA `hw:0,0` open/negotiation/close: PASS at 16 kHz mono S16_LE, period 320 frames/20 ms, buffer 1280 frames/80 ms. No playback.
- A bounded 5 s live VAD run opened the same PCM and consumed 80,000 frames through the 100-chunk queue; peak depth 4, overflow 0, XRUN 0; clean exit. It produced one speech start and ASR partials but no speech end/FINAL before the whole-program time limit. This is **not** evidence of live automatic utterance completion. No user speech was requested or recorded.
- A 200-utterance board fixture run completed naturally: 200 starts, 200 ends, 200 FINALs, one ASR load, one VAD load, pre-roll peak 4800 frames, sequence gaps 0. No remaining `cockpit_vad_fixture_test` process was found after the run. This is prerecorded-file stability, not 30-minute live microphone stability.
- A second 200-utterance run with periodic PSS/MemAvailable/temperature sampling repeated the 200/200/200 result and one load per model. Sampled peak PSS 175,333 KB, lowest MemAvailable 2,901,552 KB, highest zone-0 temperature 55.461°C; see `PERFORMANCE.md`.
- After the final pipeline stop fix, the board suite passed 4/4: VAD unit, file ASR, real `LiveAsrPipeline` paced by the unchanged `0.wav` fixture, and Sherpa VAD single-utterance integration. The fixed live regression truly traversed `LiveAsrPipeline → SherpaAsrBackend → VoiceSessionController → FINAL` without a microphone or new human speech. The separate two-utterance VAD fixture again passed with two FINALs.
- Fixed-mode `--capture-only --duration 2` on real `hw:0,0` collected 32,000 frames in 2.018 s, XRUN 0. A later 20 s VAD microphone run remained Listening with 320,000 frames, queue peak 1/100, overflow 0, XRUN 0, and no false speech start. It did not contain a controlled spoken fixture, so it cannot demonstrate real mic speech-end → FINAL.

Final grade: `VAD_FILE_PIPELINE_PASS`. The missing live acceptance evidence is a controlled real-microphone speech start, speech end and FINAL under this VAD pipeline, plus a live cancel/restart observation. This task did not request more speech from the user. The existing fixed-duration real microphone ASR PASS from the parent branch remains historical evidence, while this branch's fixed code path was regressed with real Sherpa and prerecorded PCM.
