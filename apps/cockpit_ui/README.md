# apps/cockpit_ui

`cockpit_ui` is a new RK3576 implementation. The IMX6ULL project is
reference-only (`UI_REFERENCE_ONLY`); the migration strategy is `REIMPLEMENT`.

目标页面：

```text
Home
Camera
Media / Music / Video
AI
Vehicle / Sensor
Monitor
Settings
```

应用采用单一主Qt shell，使用 `QStackedWidget` 或项目统一页面路由。实现遵循
当前 `docs/architecture/INTERFACES.md`，通过服务IPC消费数据和发出请求。

Do not copy:

- generated Qt files (`moc_*`, `ui_*`, `qrc_*`);
- ARM32 binaries or object files;
- IMX6ULL/FSL build configuration and sysroot paths;
- platform-specific device access such as AP3216C sysfs;
- the legacy QProcess child-application architecture;
- icons, music, or video whose license remains unverified.

Qt只负责display、interaction和state presentation。GUI线程不得：

- capture V4L2 frames；
- run inference；
- block on audio；
- block on RPMsg；
- perform long file or network operations。

数据路径：

```text
Camera: media_srv -> frame delivery -> cockpit_ui
Media:  cockpit_ui -> service IPC -> media_srv / audio_srv
Sensor: RT-Thread -> RPMsg -> vehicle_core -> cockpit_ui
AI:     infer_srv -> vehicle_core / IPC -> cockpit_ui
```

## Current foundation

The source tree now contains:

- a Qt-independent C++17 `UiState`, `PageId`, `PreviewFrameMetadata`,
  `IUiBackend`, and `MockUiBackend`;
- one optional Qt Widgets `QMainWindow` / `QStackedWidget` shell;
- Home, Camera, Media, Vehicle / Sensor, combined AI (Vision + Voice), Monitor,
  and Settings page skeletons;
- host tests for truthful mock defaults, navigation mapping, request recording,
  unavailable Rear/Recording behavior, simulated controls, and preview metadata.

`MockUiBackend` records Camera, Snapshot, Recording, RTSP, Media, Voice Session,
LED, and Buzzer requests. It does not open hardware, files, models, sockets, or
services. Every displayed state is marked `MOCK`, `DEMO`, or `SIMULATED`.

The integration build also contains `VehicleCoreUiBackend`. It maps UI actions to
`IVehicleCoreClient`, consumes revisioned canonical snapshots, and reports ACK and
terminal RESULT separately. The current composition is same-process and uses only
Mock service adapters:

```sh
cockpit_ui --backend mock
cockpit_ui --backend core --profile normal
cockpit_ui --backend core --profile media-failure
cockpit_ui --backend core --profile media-timeout
cockpit_ui --backend core --profile rtos-offline
```

`mock` remains the default for isolated visual development. Core profiles are bounded
integration-test fixtures, not real service configuration. No mode opens V4L2, ALSA,
RPMsg, model or sensor hardware.

Build with the repository preset:

```sh
bash scripts/dev/host_ci.sh
```

When Qt5/Qt6 Widgets development files are available, CMake also creates the
`cockpit_ui` executable. Without them, it emits
`UI_HOST_BUILD_SKIPPED_QT_NOT_FOUND` while still building and testing
`cockpit_ui_core`. The application is full screen by default; use
`cockpit_ui --windowed` for development. Tests may add
`--quit-after-ms=<milliseconds>` for a deterministic event-loop startup check.

Current foundation status: the Host Qt 5.15.3 build and offscreen startup are tested.
A native AArch64 Qt 5.15.8 build and the Mock shell's X11 startup passed. The user
subsequently confirmed the foundation layout and complete touch-navigation route on
the 800x480 panel. The integrated core build has separate evidence under
`docs/bringup/ui-core/`; foundation touch evidence does not automatically pass the
new integration binary. See `docs/architecture/COCKPIT_UI.md`,
`docs/architecture/UI_VEHICLE_CORE_INTEGRATION.md`, and `docs/tasks/P004.md`.
