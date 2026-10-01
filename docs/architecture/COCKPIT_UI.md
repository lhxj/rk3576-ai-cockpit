# Cockpit UI foundation

This document defines the first RK3576 `cockpit_ui` implementation boundary.
It describes source architecture and mock behavior, not real service or board
integration.

## Process and page architecture

`cockpit_ui` is one Qt Widgets process with one `QMainWindow` and one
`QStackedWidget` route:

```text
MainWindow
├── shared top status bar (Wi-Fi, RTOS, MOCK/DEMO marker)
├── QStackedWidget
│   ├── HomePage
│   ├── CameraPage
│   ├── MediaPage
│   ├── VehiclePage
│   ├── AiPage (Vision + Voice)
│   ├── MonitorPage
│   └── SettingsPage
└── shared bottom navigation
```

Pages are created once and owned through the QObject parent tree. They do not
start worker threads, child GUI executables, timers, or service processes. The
application runs full screen by default and accepts `--windowed` for development.

The IMX6ULL archive remains `UI_REFERENCE_ONLY`. No ARM32 binary, object,
generated Qt file, platform path, QProcess child-app design, icon, music, or
video from that archive is part of this implementation.

## Navigation

`PageId` is the single route enumeration. `kPageOrder`, `pageIndex()`, and the
stack insertion order define the mapping tested on Host. Home cards and the
bottom navigation both call the same `MainWindow::navigate()` path. Weather and
Map are outside this MVP.

The layout is designed around 800x480 but uses Qt layouts, size policies, and
stretch factors rather than page-wide absolute geometry. The top bar is 54 px
and primary buttons have a minimum height of 48 px. Actual readability and touch
target adequacy still require UI-10 board testing.

## State model

Every page consumes one `UiState`; pages do not maintain independent service
truth. `ServiceStatus` separates an availability value from its evidence source
and detail:

- values: `UNKNOWN`, `OFFLINE`, `STARTING`, `ONLINE`, `AVAILABLE*`, `ERROR`,
  `SIMULATED`, `NOT READY`, `UNAVAILABLE`, `TIMEOUT`;
- sources: `MOCK`, `HISTORICAL`, `RUNTIME`.

The current backend creates mock state only. Badges include `MOCK`; available
historical capabilities use `AVAILABLE*` with explanatory detail rather than
claiming a live service. Rear camera, RTOS, and MPU6050 start `OFFLINE`; Vision,
Voice, LLM, Recording, and RTSP start `NOT READY`; LED/buzzer controls are
`SIMULATED`.

## Backend boundary

Pages emit lightweight UI actions. `MainWindow` converts them to a `UiRequest`
and submits them through `IUiBackend`:

```text
page signal
  -> MainWindow request id + UiRequest
  -> IUiBackend
  -> UiResult and state callback
  -> MainWindow
  -> all pages consume the updated UiState
```

`MockUiBackend` is deterministic and records every request. Rear selection,
snapshot, recording, RTSP, and voice-session requests return `UNAVAILABLE` and
do not change state to success. Media requests are recorded as simulated
accepted actions without opening files. LED and buzzer update mock booleans and
return messages that explicitly contain `SIMULATED`.

`VehicleCoreUiBackend` now implements this UI-facing contract through
`IVehicleCoreClient`. Its `submit()` path returns an ACK-derived pending result
without waiting; terminal RESULT and revisioned canonical state arrive
asynchronously. `MockUiBackend` remains available for isolated visual work. See
`UI_VEHICLE_CORE_INTEGRATION.md` for the current same-process integration and future
IPC boundary.

## GUI thread rule

The GUI thread handles Qt events, state binding, drawing, and small request
construction only. It must not perform V4L2 `DQBUF`, ALSA blocking I/O, model
inference, RPMsg receive, large file reads, or blocking network work. A backend
may publish state from another thread; `MainWindow` marshals it to the Qt thread
with a queued invocation. No page owns an unmanaged worker or detached thread.

## Camera preview boundary

The current Camera page displays a placeholder and never opens a device.
`PreviewFrameMetadata` reserves:

- `camera_id`;
- `width`, `height`, and `format`;
- `sequence`;
- `stream_epoch`;
- `timestamp_ns`.

`stream_epoch` lets the consumer reject frames and inference results left over
from an earlier stream after a camera switch or reconnect. Pixel memory,
DMA-BUF/FD transfer, fences, and buffer return are deliberately unspecified
until `media_srv` proves the supported transport. The existence of V4L2 mmap or
a metadata object is not an end-to-end zero-copy claim.

## Page responsibilities

- Home: navigation and a compact view of shared state.
- Camera: preview placeholder, camera/record/RTSP status, and explicit failed
  mock results for unavailable operations.
- Media: control skeleton only; it does not scan media paths or start a player.
- Vehicle / Sensor: RTOS/MPU state, N/A sensor values, and clearly simulated
  LED/buzzer controls.
- AI: one page with separate Vision and Voice sections. It displays service,
  model/session, ASR, intent, LLM, TTS, transcript, rate, and latest result from
  state; it loads no model.
- Monitor: mock placeholders; future metrics come from a backend/monitor service.
- Settings: UI-only placeholders; it changes no brightness, mixer, network,
  boot, or kernel setting.

## Error behavior

Unavailable commands remain visible as failed `RESULT` text. A failed request
never optimistically changes Recording, Rear, RTOS, or AI to success. Timeout,
error, unavailable, not-ready, offline, and simulated states all have distinct
labels and badge styling.

## Build and validation boundary

`BUILD_COCKPIT_UI` defaults to `ON`. The Qt-independent core and tests always
build under this option. CMake first looks for Qt5 Widgets because the target
runtime is Qt 5.15.8, then accepts Qt6 Widgets for source-level Host development.
If neither development package exists, configure emits
`UI_HOST_BUILD_SKIPPED_QT_NOT_FOUND` and keeps the rest of the repository and
foundation tests buildable.

On 2026-10-01, distro Qt5 development packages were installed on Host and target.
The Host Qt 5.15.3 build passes 4/4 CTest, including deterministic offscreen
startup. A native board build using GCC 12.2 and Qt 5.15.8 also passes 4/4 CTest
and produces an ARM aarch64 ELF.

The Mock shell entered the board's existing GNOME/X11 session and exited cleanly
under `QT_QPA_PLATFORM=xcb` with `QT_XCB_GL_INTEGRATION=none`. This proves target
build, dynamic loading, Xauthority, XCB platform startup, and event-loop startup.
It does not prove visual quality or touch behavior. The default GLX probe emitted
Rockchip Mesa DRI errors, so GPU acceleration remains outside this validation.
