# UI preview result

Status: `BOARD_RUNTIME_T6_PASS / T7_TOUCH_PENDING / T8_FIVE_MINUTE_PASS`.

Date: 2026-10-02. The native AArch64 Qt 5.15.8 binary ran inside the existing GNOME
X11 session (`DISPLAY=:0`, existing user Xauthority) with
`QT_XCB_GL_INTEGRATION=none`. No display manager, GNOME or Qt library was changed.

The 20-second bounded T6 run used:

```sh
cockpit_ui --backend core --profile normal --media-backend cam0 \
  --camera-device /dev/video11 --snapshot-dir .../snapshots \
  --start-page camera --quit-after-ms=20000
```

It logged `MEDIA_BACKEND=CAM0_REAL`, captured 583 frames at 29.8767 fps with zero
sequence gaps, DQBUF/QBUF errors and poll timeouts, and delivered 253 Qt preview
updates at 13.0096 fps. The latest-frame path reported zero mailbox and GUI-coalescing
drops; 323 input frames were intentionally skipped by the preview throttle. The
process exited normally and released `/dev/video11`.

Host offscreen Qt verifies synthetic frame metadata, 8x4 `QImage` dimensions,
queued GUI-thread delivery, runtime source labels, restart epoch 1->2, explicit
Recording rejection and clean shutdown. This automated evidence does not replace
physical visual/touch acceptance. T7 remains pending explicit user observation.
No screenshot utility was installed on the board; a bounded attempt through the
existing GNOME Shell D-Bus screenshot API returned `AccessDenied: Screenshot is not
allowed`. No security/session setting was changed, so no automated screen capture is
claimed.

The subsequent 300-second bounded real-CAM0 run also exited normally; details are in
`PERFORMANCE.md`. It opened directly on Camera and no navigation/snapshot interaction
was recorded, so it provides runtime/display-delivery evidence but does not close T7.
