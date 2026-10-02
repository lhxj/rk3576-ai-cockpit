# CAM0 RTSP stability record

## Host

- Debug build and `media_recording_test` / `media_rtsp_test`: PASS.
- ASan+UBSan with leak detection for the same two tests: PASS.
- Slow fake UDP transport caused explicit bounded-queue drops; the recording
  lifecycle test remained independent and passed.
- Encoder-sharing tests cover Recording then RTSP and RTSP then Recording.
  In each overlap, `EncoderStats.start_count` remains one for that active
  session; stopping either consumer retains the encoder for the other.
- Vehicle Core timeout and late-result fencing for RTSP start: PASS.
- Vehicle Core publishes RTSP `STOPPING` between STOP ACK and RESULT: PASS.
- A forced RTSP sink failure leaves Recording and the shared encoder active;
  sink failures are isolated from the other consumer: PASS.

## Board metrics pending

The following must be filled from bounded RK3576 runs: capture/encoded FPS,
RTP packets/drops/queue peak, reconnects, join-to-first-IDR, CPU, RSS/PSS,
temperature, V4L2 sequence gaps and DQBUF/QBUF errors, MPP errors, and recording
queue overflow. Five-minute and twenty-cycle evidence is not yet available
because T0 SSH access timed out.
