# V4L2 negotiation

Status: `BOARD_TESTED_T1_PASS`.

Date: 2026-10-02. Environment: LubanCat-3 v2, Debian 12, kernel
`6.1.99-rk3576`, AArch64 build with GCC 12.2, user `cat`.

Command (exit 0):

```sh
timeout 15s build/apps/media_srv/media_cam0_probe \
  --mode negotiate --device /dev/video11
```

The project V4L2 backend opened, configured, allocated and released the device:

| field | actual |
|---|---:|
| driver | `rkisp_v10` |
| width x height | 1632 x 1224 |
| fourcc | NV12 |
| V4L2 type | VIDEO_CAPTURE_MPLANE |
| memory planes | 1 |
| bytesperline | 1632 |
| sizeimage | 2996352 |
| requested/actual MMAP buffers | 4 / 4 |

`VIDIOC_S_PARM/G_PARM` is attempted, but this mainpath does not expose a valid
time-per-frame. The backend therefore reports `frame_interval_supported=false`;
measured capture rate is recorded separately in `STREAM_RESULT.md`.

Negative board checks also passed: `/dev/does-not-exist` failed at `open` with
`ENOENT`, and a 640-wide request was rejected before streaming with
`CAM0 v1 requires 1632x1224 NV12`. No format substitution is accepted.
