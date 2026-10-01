# Live microphone ASR pipeline

This stage connects the existing file-tested Sherpa recognizer to a live capture source. It does not implement VAD, wake word, intent execution, playback, TTS or a voice assistant.

## Ownership and data path

`AlsaAudioCapture` in `audio_srv` alone opens a capture PCM. The device name is supplied at runtime. `voice_srv` receives `IAudioCapture` and cannot call ALSA. `LiveAsrPipeline` creates one `VoiceSessionController` token, starts the already loaded `IAsrBackend`, and runs one capture producer and one serial decode consumer. `PcmChunk` carries token session ID, sequence, monotonic capture time, format, frame count and owned bytes. The queue is the existing `BoundedQueue<PcmChunk>`; default capacity is two seconds based on the negotiated period, capped at 1000 chunks. PCM is not saved by default.

ALSA requests 16 kHz, mono, S16_LE, interleaved; it reads the negotiated values and rejects a mismatch. The requested 320-frame period and four-period buffer may be adjusted by the device and are reported. `snd_pcm_start` begins nonblocking capture before finite `snd_pcm_wait`/`snd_pcm_readi` calls. The read wait is at most 50 ms per iteration. `stop()` sets the stop flag, drops only this object's PCM, closes it and allows the producer to join. EPIPE and ESTRPIPE are counted and recovery attempted; tests require zero XRUN. No resampling or hidden channel conversion occurs. `SherpaAsrBackend` already converts S16 samples to float at its ASR boundary.

## Session, error and cancellation

The model is loaded once before multiple sessions. Each `start()` opens capture, obtains its actual format, starts a new session/generation/request token and a Sherpa stream, then starts two owned joinable workers. `finish()` stops capture, joins the producer, closes the queue, lets the consumer drain and call `finish_input()`, and requires a real `ASR_FINAL`. The callback uses `VoiceSessionController::deliver_event`, so old token events cannot be delivered after cancel or replacement.

`cancel()` fences the token first, stops capture, closes the queue, joins both workers and cancels the Sherpa stream. Queue FULL emits `ASR_ERROR AUDIO_QUEUE_OVERFLOW`, cancels the session and ends the stream; it never silently drops PCM or blocks forever. Capture/ASR errors likewise close the queue and make the session terminal. The test tool reports capture stop and complete session time; the initial engineering targets are 500 ms and 1 s respectively, not hard real-time guarantees. No detached threads or infinite receive timeout are used.

`IAsrBackend`, `VoiceSessionController`, `SessionToken`, `BoundedQueue`, and `Status` retain the file-ASR contract. The only foundation interface addition is `IAudioCapture::actual_format()` so callers can validate actual ALSA negotiation. `COCKPIT_ENABLE_ALSA_CAPTURE` defaults OFF, and `COCKPIT_ENABLE_SHERPA_ASR` also defaults OFF; ordinary Host CI requires neither library. Real microphone input is a later boundary for VAD/wake; it is absent from this implementation.
