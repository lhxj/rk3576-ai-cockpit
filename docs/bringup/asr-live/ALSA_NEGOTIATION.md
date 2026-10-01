# ALSA negotiation and capture gates

`bash scripts/board/test_asr_live.sh --phase T0 --device hw:0,0` passed: 16000 Hz, mono S16_LE, 320-frame period (20 ms), 1280-frame buffer (80 ms), open and close without a residual process.

Initial T1 failed with zero frames over 2.099 s and abnormal CPU. The nonblocking capture path waited before explicitly starting the PCM. `AlsaAudioCapture::start()` was corrected to call `snd_pcm_start()` after prepare. The T1 retry passed: 32000 frames in 2.014 s, XRUN 0, normal close. The failure and repair are retained as evidence; no microphone result was claimed from the first run.
