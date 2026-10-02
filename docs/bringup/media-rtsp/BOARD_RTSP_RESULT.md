# RK3576 CAM0 RTSP result

Status: **`MEDIA_CAM0_RTSP_PASS`** on 2026-10-02 for the bounded, single-CAM0,
single-client, unauthenticated UDP-unicast scope defined in
`CAM0_RTSP_PIPELINE.md`.

Evidence directory on the board:
`/home/cat/cockpit/media-cam0-rtsp-20261002-1207/results/`. It is local
bring-up evidence and is not committed to Git.

## T0 and native build

The earlier four `ssh lubancat` attempts to the former address
`10.34.122.223` timed out; that historical result is retained. At 2026-10-02
12:03:44 +08:00 the user supplied `10.232.249.223`. Direct
`cat@10.232.249.223` reached sshd but did not inherit the configured key.
`ssh -o HostName=10.232.249.223 lubancat` preserved the alias authentication
and succeeded.

Read-only inventory observed Debian 12, aarch64, kernel `6.1.99-rk3576`,
`wlan0=10.232.249.223/24`, no 8554 listener, and no media probe owner. Existing
clients were ffprobe/ffplay 5.1.8, GStreamer and mpv. MPP Debian packages were
1.5.0-1, pkg-config reported 1.3.9, and the runtime reported commit
`43a191ed`. The live graph resolved `m00_b_ov8858 3-0036` through rkisp0
mainpath to `/dev/video11`, 1632x1224 at 30 fps. This node is evidence for this
boot only.

The release `media_rtsp_probe`, Debug test suite, sanitizer probe, and Qt5
`cockpit_ui` with V4L2+MPP were built natively. Debug CTest passed 23/23. The
Qt executable is an AArch64 PIE. An initial full RelWithDebInfo *test* build
exposed pre-existing voice tests that use variables only inside `assert`, so
`NDEBUG` triggered `-Werror=unused-*`; the release RTSP probe itself built.

## T1 RTSP only and client identification

Board ffprobe connected to `rtsp://10.232.249.223:8554/cam0` and identified:

```text
codec_name=h264
profile=High
width=1632
height=1224
r_frame_rate=30/1
avg_frame_rate=30/1
```

The 25-second run captured 749 frames at 29.875 fps and encoded 748 at
29.895 fps. Encoder instances=1, RTP drops=0, sequence gaps=0, DQBUF/QBUF
errors=0, encoder errors=0, and join-to-first-IDR=36 ms.

WSL, as a separate host, reached the server over `10.232.249.223:8554` and
received OPTIONS/DESCRIBE 200. The final SDP contained H264/90000, SPS/PPS,
1632x1224 and `a=framerate:30`. WSL had no installed media decoder, so actual
H.264 decode evidence is the board's existing ffprobe/ffmpeg client; WSL proves
off-board RTSP TCP reachability, not off-board RTP decode.

## T2 reconnect

The service stayed running while two ffprobe sessions connected sequentially.
Both identified H.264 1632x1224 at 30 fps. Server stats reported two connects,
one reconnect, join-to-first-IDR=38 ms, one encoder instance, and no RTP,
capture, or encoder errors.

## T3 Preview + RTSP

ffmpeg copied the live stream to a null sink for 300 seconds and received
9,001 frames. The service captured 9,264 frames at 29.876 fps, delivered 9,263
preview frames, and encoded 9,263 frames at 29.878 fps. It used one encoder;
RTP drops, encoder overflow/errors, sequence gaps, poll timeouts and
DQBUF/QBUF errors were all zero. Queue peak was 176 RTP packets and
join-to-first-IDR was 27 ms.

The probe stabilized at about 44% CPU, RSS 26,572 kB and PSS 23,726 kB. The
highest sampled thermal-zone value was 53.615 C.

Before this successful run, ASan found a stack-use-after-scope in the board
probe's preview consumer: its thread captured a block-local mailbox shared_ptr
by reference. The mailbox is now captured by value. Host CI passed and a board
ASan+UBSan 8-second reproduction then exited cleanly before T3 was rerun.

## T4 Preview + Recording + RTSP

The combined run lasted 300 seconds. ffmpeg received 9,001 RTSP frames while
the same service captured 9,265 frames and sent 9,264 frames to preview,
recording and RTSP through **one** encoder instance. The Annex-B file
`recording_1.h264` was 309,342,219 bytes; ffprobe identified H.264 High,
1632x1224, 30 fps.

Recording queue peak=1 and overflow=0. RTP drops, encoder errors/overflow,
sequence gaps, poll timeouts and DQBUF/QBUF errors were zero. Probe steady-state
CPU was about 43%, RSS about 26,620 kB and PSS 23,770 kB. The ffmpeg client used
about 1.8% CPU and PSS 32,702 kB. Maximum sampled temperature was 53.615 C.

## T5 twenty start/stop cycles

Twenty cycles completed in 6.7 seconds with 40 admission ACKs and 40 separate
business RESULTs. All START results reported the listening endpoint ready and
all STOP results succeeded. After exit, port 8554 had no listener and no probe
process remained. Across cycles, sequence gaps and V4L2/encoder errors were
zero.

## Final targeted regression

After correcting SDP rate propagation, Host CI passed 29/29 CTest and 6/6
Python tests; Host ASan/UBSan recording+RTSP passed 2/2. On RK3576, Debug
`media_rtsp` passed, the real ASan+UBSan Preview+RTSP probe passed, board
ffprobe again identified 1632x1224@30, WSL asserted `a=framerate:30`, and the
service released port 8554 and PID 97424 normally.

This result does not cover CAM1, audio, authentication, TLS, Internet/NAT,
TCP-interleaved RTP, multi-client operation, H.265, or long-term streaming.
