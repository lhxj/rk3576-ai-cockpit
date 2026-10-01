# Live cancellation result

Host Mock lifecycle, stale-event fencing, overflow and error tests passed under `live_asr_test`. Board T3 ran a nominal 10-second session, requested cancel after about 2 seconds and produced no old `ASR_FINAL`. Measured request-to-capture-stop: 13.06 ms; request-to-complete-stop: 13.88 ms. The program exited naturally with no residual capture process, XRUN 0 and overflow 0. These are measured engineering latencies, not hard real-time guarantees.

T4 cancelled session 1 after about 2 seconds (partial `开` was seen, no old FINAL), then opened a fresh ALSA capture in the same process and got session 2 `ASR_FINAL 开始录像开始录像`. Capture stop on the cancelled session took 16.59 ms; full stop 17.51 ms. New-session delivery carried its own token; the old token did not produce a FINAL. An earlier T4 run without user speech had an empty new-session FINAL and was not counted as human-speech recognition.
