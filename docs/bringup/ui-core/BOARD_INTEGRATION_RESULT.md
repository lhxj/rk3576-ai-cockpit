# RK3576 UI + Vehicle Core integration bring-up

Date: 2026-10-01

Target: LubanCat-3 v2 / RK3576, Debian 12, Qt 5.15.8, GNOME/X11. Deployment was
limited to `/home/cat/cockpit/ui-core-integration-20261001-01/`. No sudo, package
installation, system Qt replacement, desktop-service change, boot change or hardware
service access was used.

## Native build

The integration source was copied without `.git`, Host build output or local artifacts.
Native CMake Release build used GCC 12.2.0 and Qt5 Widgets. Results:

- configure/build: exit 0;
- target CTest: 16/16 PASS;
- binary: dynamically linked ARM aarch64 ELF;
- SHA256:
  `d5863b4fa763577bdf580d292148ca6046462eef192e1f5742ef9d19ca471632`.

## X11 startup

The existing session was confirmed before launch: user `cat`, GNOME Shell present,
Xauthority readable, and no `cockpit_ui` process running. Launches used:

```text
DISPLAY=:0
XAUTHORITY=/run/user/1000/gdm/Xauthority
XDG_RUNTIME_DIR=/run/user/1000
QT_QPA_PLATFORM=xcb
QT_XCB_GL_INTEGRATION=none
```

Bounded windowed launches of `normal`, `media-failure`, `media-timeout` and
`rtos-offline` profiles all exited 0. A bounded full-screen launch of `normal` also
exited 0. A final `pgrep` check found no residual `cockpit_ui` process. The XCB GL
workaround is retained; this task did not investigate Mesa/GLX.

## Boundary

This proves AArch64 compilation, target linkage, target CTest and actual X11 event-loop
startup for the integrated binary. Automated startup does not prove visual layout,
physical touch, or the visible state sequences from button interaction. Those require
the separate user confirmation in `MANUAL_TOUCH_VALIDATION.md`.

No V4L2, camera, ALSA, Sherpa, RKNN, RKLLM, RPMsg, RT-Thread, I2C, GPIO or sensor was
opened.
