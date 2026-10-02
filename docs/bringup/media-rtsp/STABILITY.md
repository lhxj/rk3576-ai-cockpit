# CAM0 RTSP stability record

## Host

- Full Debug CI: 29/29 CTest and 6/6 Python tests PASS.
- ASan+UBSan with leak detection for `media_recording` and `media_rtsp`: 2/2
  PASS after the final SDP correction.
- Slow fake UDP transport causes explicit bounded-queue drops and IDR resync;
  recording remains independent.
- Tests cover both consumer start orders, stopping either consumer while the
  other retains the encoder, forced RTSP sink failure isolation, timeout and
  late-result fencing, STOPPING publication, reconnect, and clean shutdown.

## RK3576 bounded results

| Case | Duration | Capture/encode | RTP | Recording | Result |
|---|---:|---|---|---|---|
| RTSP only | 25 s | 29.875/29.895 fps | 591 packets, 0 drop | inactive | PASS |
| Reconnect | 35 s | 29.876/29.888 fps | 1,439 packets, 0 drop | inactive | PASS |
| Preview + RTSP | 300 s client | 29.876/29.878 fps | 256,323 packets, 0 drop | inactive | PASS |
| Preview + Recording + RTSP | 300 s client | 29.876/29.878 fps | 256,133 packets, 0 drop | peak 1, overflow 0 | PASS |
| RTSP start/stop | 20 cycles | one active encoder per cycle | sockets released | inactive | PASS |

For both five-minute runs: encoder queue peak=1, encoder overflow/errors=0,
V4L2 sequence gaps=0, poll timeouts=0 and DQBUF/QBUF errors=0. Preview+RTSP
probe RSS/PSS stabilized at 26,572/23,726 kB; the combined run at
26,620/23,770 kB. Probe CPU was about 44% and 43% respectively. Maximum sampled
temperature was 53.615 C.

## Fault found during bring-up

The first Preview+RTSP attempt crashed before stability timing. RK3576 ASan
identified stack-use-after-scope in `media_rtsp_probe.cpp`: the preview worker
captured its local mailbox shared_ptr by reference. Capturing it by value fixed
the issue. Board ASan/UBSan and the full five-minute run passed afterward. The
failure is retained here rather than erased from the record.

These are bounded bench measurements, not long-term or multi-client evidence.
