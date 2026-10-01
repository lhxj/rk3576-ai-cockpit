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

Build with the repository preset:

```sh
bash scripts/dev/host_ci.sh
```

When Qt5/Qt6 Widgets development files are available, CMake also creates the
`cockpit_ui` executable. Without them, it emits
`UI_HOST_BUILD_SKIPPED_QT_NOT_FOUND` while still building and testing
`cockpit_ui_core`. The application is full screen by default; use
`cockpit_ui --windowed` for development.

Current status: state/backend core `MOCK_TESTED`; Qt shell and pages `PARTIAL`
because the current Host and board have no Qt development package and the Qt
target has not been compiled. No AArch64 display or touch claim is made. See
`docs/architecture/COCKPIT_UI.md` and `docs/tasks/P004.md`.
