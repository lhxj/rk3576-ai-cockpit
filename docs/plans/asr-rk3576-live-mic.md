# ASR RK3576 live microphone integration

## Goal and baseline

Build the bounded live chain `ALSA capture → audio_srv IAudioCapture → PcmChunk queue → voice_srv LiveAsrPipeline → SherpaAsrBackend → VoiceSessionController → ASR_PARTIAL/ASR_FINAL` from frozen `2b15102739a63a36d5fa26d58552d3d18ecb15c4`. Keep Sherpa/model loaded across sessions and preserve file ASR.

## Observed evidence and unknowns

- File recognition, AArch64 Sherpa v1.11.3/ONNX Runtime 1.17.1, cancellation and Host CI passed at the baseline; do not redo dependency acquisition.
- Read-only live inventory on 2026-10-01: `hw:0,0` is currently the card 0 ES8323 capture PCM, subdevice 1/1. PulseAudio and PipeWire run. `fuser` on the capture node did not identify a holder, though PulseAudio holds other `/dev/snd` nodes. No audio service will be stopped.
- `libasound2-dev` and `/usr/include/alsa/asoundlib.h` were missing on the board. `apt-get -s install libasound2-dev` proposed only one new package, Debian arm64 1.2.8-1+b1. The user explicitly approved this exact installation; `sudo -n apt-get install --no-install-recommends -y libasound2-dev` installed only that package and the header check passed.
- Board T0–T5 subsequently measured format, period/buffer, live recognition, cancellation and resources. Speech capture depends on a timely user cue; two runs with no usable speech returned empty FINAL and were not counted as PASS.

## Scope and files

- Add optional `AlsaAudioCapture` only in `apps/audio_srv`, with device chosen by constructor/CLI and no system configuration changes.
- Add `PcmChunk` domain metadata and `LiveAsrPipeline` in `apps/voice_srv`, using existing `BoundedQueue`, `IAsrBackend`, `VoiceSessionController`, and `Status`.
- Add bounded live test CLI, Host Mock tests, optional CMake targets and board scripts/docs. Keep `COCKPIT_ENABLE_ALSA_CAPTURE=OFF` by default.
- The current `IAudioCapture` lacks a way to report **negotiated** hardware format. Add only `actual_format() const` to that interface and its Mock; this is required to reject wrong hardware rate and size queue capacity from actual period. Preserve all existing start/read/stop/state semantics.

## Out of scope

No playback, VAD, wake word, intent, vehicle command, TTS, RKLLM, RKNN, camera, AMP or RT-Thread. No raw audio is stored by default; no global ALSA/PulseAudio/PipeWire settings, service stop or system library replacement.

## Permissions and resources

Host edits, CMake, mocks and CI are L0. Read-only target audio inventory is L1. User explicitly authorized bounded user-space microphone testing on this board under L2, subject to current device occupancy and the shared board lock. `sudo apt install libasound2-dev` is L3 and must not occur before explicit approval. Capture tests are short (T1 2 s; T2/T4/T5 5 s; T3 cancel after 2 s), with bounded logs, a 120 s outer safety limit, normal stop/join, and no raw PCM dump unless explicitly requested.

## Steps and gates

1. Record current ALSA environment, package state, and target PCM occupancy. If package approval is withheld, stop before native ALSA build while completing L0 code/tests.
2. Implement ALSA nonblocking capture: exact 16 kHz/mono/S16 negotiation, period/buffer observation, 50–100 ms finite waits, xrun count/recovery and close on stop. `-EBUSY` returns `AUDIO_DEVICE_BUSY` without stopping other services.
3. Implement bounded PcmChunk producer/consumer path, explicit overflow ERROR/cancel, session fencing, cancel and joined worker lifecycle. Host test normal flow, queue close, overflow, replacement, capture/backend errors and shutdown.
4. Run default Host CI and optional x86 Sherpa file integration. Deploy a **minimal** native source context under `/home/cat/cockpit/asr-live`, reusing the previously validated model/runtime after SHA256 checks. Re-run board file ASR before opening capture.
5. In order: T0 open/negotiation/close; T1 2 s capture only; T2 one 5 s spoken phrase → FINAL; T3 10 s session cancelled at 2 s; T4 new session after cancel; T5 three 5 s sessions/model load once. Stop at the first failed gate. Record actual phrase/text, queue/xrun, latency, RSS/PSS, CPU and temperature. No device or service changes.

## Validation, recovery, result

`ASR_BOARD_LIVE_MIC_PASS` observed on 2026-10-01: T0/T1 negotiated and captured real `hw:0,0` PCM; T2 human speech produced partial and final; T3 cancelled without old FINAL; T4 cancelled then recognized in a new session; T5 loaded once and recognized three consecutive sessions. File ASR regression passed before microphone work, Host CI passed 10/10 CTest and 6/6 Python, and optional x86 Sherpa integration passed 2/2. Logs remained ignored under `build/`; no PCM was dumped. One initial T1 zero-frame failure was repaired by explicitly starting the nonblocking PCM, then retested. A file-ASR monitor `/proc` race was also repaired and its full test rerun. See `docs/bringup/asr-live/` for outputs and measurement limits. Stop here; do not start VAD, wake word or intent work.
