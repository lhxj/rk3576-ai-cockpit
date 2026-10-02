# CAM0 RTSP integration plan

## Goal

Reach `MEDIA_CAM0_RTSP_PASS` without regressing the verified CAM0 preview and
Annex-B recording baseline. Recording-only, RTSP-only, and combined operation
must use one capture and one MPP encoder.

## Scope

Implement a single-client, unauthenticated LAN RTSP server for CAM0 H.264 with
UDP unicast RTP. Split encoding from file output, add encoded-packet fan-out,
bounded network backpressure, SPS/PPS/IDR join handling, Vehicle Core runtime
semantics, UI runtime availability, Host tests, sanitizers, and staged RK3576
tests.

CAM1, audio RTSP, MP4, Internet streaming, multi-client support, VAD intent,
RKNN, RGA/zero-copy, AMP, and RT-Thread are outside this plan.

## Existing evidence

- Base commit `08b7a4b` has the prior `MEDIA_CAM0_RECORDING_PASS` evidence.
- The old `MppH264Recorder` owns both MPP and the `.h264` file, so it cannot
  feed RTSP without either duplication or refactoring.
- CAM0 historical target format is NV12 1632x1224 at about 30 fps.

## Implementation

1. Replace the recorder boundary with `IH264Encoder`, `EncodedPacket`, and a
   separately queued `FileRecordingSink`.
2. Add Annex-B parsing, RFC 6184 packetization, `RtpSender`, and the minimum
   RTSP control server.
3. Let `MediaService` own shared encoder lifetime and fan-out. Extend the real
   adapter and Vehicle Core RTSP START/STOP state transitions.
4. Enable the existing Qt RTSP button in the real CAM0 profile through the same
   backend chain. Add configurable port/path and shutdown metrics.
5. Add Host lifecycle, packetization, slow-network, reconnect, timeout, and
   shutdown tests; run the full Host CI and ASan/UBSan.
6. Run board T0 through T5 serially with live camera-node resolution, bounded
   process durations, an existing client, and one-owner checks.

## Validation and recovery

Host: `bash scripts/dev/host_ci.sh` plus a separate ASan/UBSan build of
`media_recording_test` and `media_rtsp_test`.

Board: build natively with V4L2 and MPP enabled, then run `media_rtsp_probe` in
`rtsp-only`, `preview-rtsp`, `shared`, and `restart` modes. Use the current
`wlan0` address; do not hard-code a historical address. Every process has an
external timeout and only its exact PID is stopped. The test does not install
packages or change the network, desktop, boot files, kernel, or device tree.

If RTSP fails, stop its exact probe process, confirm CAM0 has no remaining
owner, preserve bounded logs, and rerun the prior recording probe before any
code change. A client or network failure must not invalidate the recording
file or cause a second encoder to be created.

## Result so far

- Host Debug build: PASS.
- Host RTSP/recording tests: PASS, including real local TCP RTSP control and
  two client sessions with a fake UDP transport.
- ASan/UBSan RTSP and recording tests: PASS.
- Board T0: BLOCKED on 2026-10-02 because four bounded `ssh lubancat`
  attempts timed out before login; the last attempt was at 11:54:14 +08:00.
- Final grade remains below `MEDIA_CAM0_RTSP_PASS` until all board gates pass.
