# Board live ASR gates

Baseline file regression: `scripts/board/test_asr_file.sh --full` passed before opening capture, including one recognition, three repeated recognitions, cancel, and negative inputs. Text remained `昨天是 MONDAY TODAY IS THEY AFTER TOMORROW是星期三`. An initial monitoring-script `/proc/<pid>/smaps_rollup` exit race was fixed; the full retry ended `BOARD_FILE_ASR_VALIDATION_PASS`.

| Gate | Evidence | State |
| --- | --- | --- |
| T0 open/close | `hw:0,0`, 16 kHz mono S16_LE, 320/1280 frames | PASS |
| T1 capture 2 s | 32000 frames / 2.014 s, XRUN 0 after explicit PCM start fix | PASS |
| T2 live 5 s to FINAL | User repeatedly said `打开摄像头`; `ASR_FINAL 摄像头打开摄像头`, 6 PARTIAL, 80000 frames, XRUN/overflow 0 | PASS |
| T3 cancel at 2 s | Old session had no FINAL; capture stop 13.06 ms, full stop 13.88 ms | PASS |
| T4 cancel then new session | Old session cancelled without FINAL; user repeatedly said `开始录像`; new `ASR_FINAL 开始录像开始录像` | PASS |
| T5 three sessions | One `MODEL_LOADED`, three nonempty FINALs, no cross-session events or residual process | PASS |

**Result: `ASR_BOARD_LIVE_MIC_PASS` — live microphone ASR only.** The actual path was `AlsaAudioCapture → IAudioCapture → BoundedQueue<PcmChunk> → LiveAsrPipeline → SherpaAsrBackend/IAsrBackend → VoiceSessionController → ASR_PARTIAL/ASR_FINAL`. The 100-chunk queue holds about 2 seconds at 20 ms periods. File ASR regression, Host CI and optional x86 Sherpa integration passed; no model or PCM was committed.

T2 had two earlier empty-text runs: one user-timed utterance was not captured as recognizable speech; the next had no spoken phrase because the chat cue arrived too late. Both are retained as diagnostic failures, with 80000 frames each, no XRUN/overflow and low aggregate RMS. The final T2 used a pre-announced 20-second repeated phrase, placing genuine speech inside the 5-second capture window. Likewise, an initial T4 received no spoken phrase and produced empty text; a synchronized repeat passed. The test tool does not substitute a canned transcript. These observations show that test cue timing and microphone distance matter; they are not a WER benchmark.

T5 actual FINAL texts, with the user repeatedly saying `打开摄像头`:

1. Session 1: `摄像头打开摄像头打开摄像头打开`
2. Session 2: `打开摄像头打开摄像头打开摄`
3. Session 3: `打开摄像头打开摄像头打开摄像`

No camera, recording, vehicle action, playback, VAD or wake-word function was invoked. Model/test WAV remain `LICENSE_UNVERIFIED_FOR_DISTRIBUTION`. This result does not establish command accuracy or a voice assistant.
