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

## Result

- Host final CI: 29/29 CTest and 6/6 Python PASS.
- Host ASan/UBSan recording+RTSP: 2/2 PASS.
- RK3576 native Debug CTest: 23/23 PASS; release RTSP probe and Qt5 real-media
  target built as AArch64 executables.
- T1 actual board client identified H.264 High 1632x1224 at 30 fps. WSL reached
  OPTIONS/DESCRIBE through wlan0 and validated the final SDP.
- T2 reconnect, T3 five-minute Preview+RTSP, T4 five-minute
  Preview+Recording+RTSP, and T5 twenty start/stop cycles PASS.
- One probe-only stack-use-after-scope was found by board ASan, fixed by
  capturing the preview mailbox by value, and retested.
- Final grade: `MEDIA_CAM0_RTSP_PASS` for the frozen single-client scope.
  Off-board RTP decode, CAM1, audio, authentication, Internet/NAT and long-term
  streaming remain outside this result.
