# RK3576 CAM0 RTSP result

Status: `BLOCKED_BY_BOARD_REACHABILITY` as of 2026-10-02.

## T0

Four bounded `ssh lubancat` read-only connection attempts timed out before
login; the last attempt was at 2026-10-02 11:54:14 +08:00. The
current `wlan0` IPv4 address, port 8554 availability, installed `ffprobe` /
`ffplay` / GStreamer / VLC tools, current camera graph, and native build have
not been observed in this branch.

## Pending gates

- T1 RTSP-only with an actual client identifying H.264 1632x1224 at 30 fps.
- T2 disconnect/reconnect with the second join decoding from a new IDR.
- T3 Preview + RTSP for at least five minutes.
- T4 Preview + Recording + RTSP with one CAM0 owner, one encoder, valid
  recording output, and recording queue overflow zero.
- T5 twenty RTSP start/stop cycles with sockets and camera released.

No board process was started, no camera was opened, and no board state
was changed during the failed T0 attempts. `MEDIA_CAM0_RTSP_PASS` is not claimed.
