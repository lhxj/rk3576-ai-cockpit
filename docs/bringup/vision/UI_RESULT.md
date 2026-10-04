# Real RKNN Vision UI result

The native AArch64 `cockpit_ui` was run for ten seconds in the existing GNOME X11
session on 2026-10-02. Environment remained `DISPLAY=:0`, the existing user
Xauthority, `QT_QPA_PLATFORM=xcb` and the already required
`QT_XCB_GL_INTEGRATION=none`. No desktop or Qt setting was changed.

The command selected the real Core/CAM0/RKNN backends and opened the unified AI page:

```text
cockpit_ui --backend core --profile normal --media-backend cam0 \
  --camera-device /dev/video11 --vision-backend rknn \
  --vision-model /usr/share/model/RK3576/mobilenet_v1.rknn \
  --start-page AI --windowed --quit-after-ms=10000 ...
```

The application reported `VISION_BACKEND=RKNN`, runtime state `RUNNING`, model
`MobileNetV1 RK3576`, runtime 2.3.0, driver 0.9.8, 73 results at 7.46982 fps and
queue peak one. The process exited normally with zero V4L2 errors and no residual
camera owner.

Host tests separately verify that a RUNNING snapshot maps to Runtime/Online,
the real model name, rate, camera, class/confidence and `stream_epoch/sequence` text.
`MainWindow` transfers backend callbacks with a Qt queued invocation, so the NPU
worker does not update widgets directly.

This is an actual board X11 startup and runtime-state integration result. No new
manual touch/visual acceptance or screenshot is claimed in this task.
