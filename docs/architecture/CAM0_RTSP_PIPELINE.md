# CAM0 RTSP pipeline

Status date: 2026-10-02. The Host implementation is tested. RK3576 native and
network-client evidence is recorded separately under `docs/bringup/media-rtsp/`.

## One capture and one encoder

`MediaService` remains the only CAM0 owner. Recording and RTSP are consumers of
one shared H.264 encoder:

```text
V4L2 CAM0 (one capture)
        |
        +----> PreviewMailbox
        |
        +----> IH264Encoder (one instance while either consumer is active)
                    |
                    +---- EncodedPacket (Annex-B access unit)
                          | camera_sequence / stream_epoch / RTP timestamp
                          |
                          +----> FileRecordingSink -> recording_<id>.h264
                          |
                          +----> RtspServer -> bounded RtpSender queue -> UDP RTP
```

`MppH264Recorder` was split because it combined MPP encoding with file output.
The production path is now `MppH264Encoder` plus `FileRecordingSink`. Host
tests substitute `FakeH264Encoder` and `FakeRtspServer`. Starting a second
consumer attaches another sink and requests an IDR; it does not reopen CAM0 or
start another encoder. The encoder stops only after both Recording and RTSP are
stopped. Preview has an independent lifetime.

## RTSP subset

The first implementation is intentionally small:

- one CAM0 H.264 stream, one client, LAN only, no authentication;
- RTSP over TCP with `OPTIONS`, `DESCRIBE`, `SETUP`, `PLAY`, and `TEARDOWN`;
- unicast RTP/H.264 over UDP; no audio, RTCP processing, TCP interleaving,
  TLS, multicast, H.265, multi-client, or NAT traversal;
- default URL `rtsp://<current-wlan0-ip>:8554/cam0`;
- port and path are configurable through `RtspConfig` and the UI executable's
  `--rtsp-port` / `--rtsp-path` options.

The project uses its own minimum server because the 2026-10-02 Host inventory
did not find an embeddable live555 or gstreamer-rtsp-server development package.
This choice does not claim general RTSP protocol compliance.

## SPS, PPS, IDR and SDP

The server parses Annex-B NAL units and caches the latest SPS and PPS. SDP uses
`H264/90000`, `packetization-mode=1`, `profile-level-id`, and
`sprop-parameter-sets`. `RTSP_START` waits until the listening socket, encoder,
and SPS/PPS are ready before returning success.

Each new `PLAY` sets the RTP sender to `waiting_for_idr`, requests an IDR from
the encoder, drops intervening P frames, and sends cached SPS/PPS before the
first IDR. MPP is configured with `MPP_ENC_HEADER_MODE_EACH_IDR`. The sender
records `client_join_to_first_idr_ms`.

## RTP packetization

`H264RtpPacketizer` implements the required RFC 6184 subset. A NAL that fits
the configured payload is sent as one RTP packet. Larger NALs use FU-A with
correct start/end bits. All NALs from one encoded access unit share a 90 kHz
timestamp; only the final RTP packet of the access unit has the marker bit.
Sequence numbers increase for every generated RTP packet.

## Backpressure

Network transmission runs on the `RtpSender` worker, outside capture, encoder,
and file-sink threads. Its default queue holds at most 1024 RTP packets. Queue
overflow or a UDP send failure is counted in `rtp_drop_count`; queued packets
are discarded, the sender enters `waiting_for_idr`, and output resumes only
from a later IDR with SPS/PPS. No network operation blocks the V4L2 capture or
file writer.

`FileRecordingSink` has a separate bounded encoded-packet queue. Recording
overflow is a business failure and is never silently converted into an RTSP
drop. Encoder failures stop both consumers, while a sink-specific failure
detaches only that sink and preserves the other consumer. Board concurrent
validation must show recording overflow zero.

## Command and state semantics

```text
UI -> VehicleCoreUiBackend -> VehicleCore -> RealMediaServiceAdapter
   -> MediaService -> RTSP listening socket / shared encoder
```

`RTSP_START` produces ACK after command admission. Vehicle Core publishes a
RUNTIME/STARTING state while the asynchronous operation runs. RESULT success
means the service is connectable and has encoder parameters; it does not mean a
client is connected. Canonical RTSP becomes RUNTIME/ONLINE/ON only from the
real adapter result.

`RTSP_STOP` publishes STOPPING, closes the client and
listening sockets, stops the RTP sender, retains the encoder when Recording is
still active, and returns RESULT before canonical RUNTIME/ONLINE/OFF. A timeout
is terminal for that request; a late adapter result is fenced by Vehicle Core.

## Metrics

The probe and UI shutdown output expose capture and encoded FPS, encoder queue
depth/overflow/error and instance count, RTP packets/drops/queue peak, client
connect/reconnect count, join-to-first-IDR, recording queue overflow, V4L2
sequence gaps and DQBUF/QBUF errors. CPU, RSS/PSS, temperature and media-driver
logs are board-test measurements, not synthetic Host values.
