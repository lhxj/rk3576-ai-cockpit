# Cockpit UI foundation execution plan

Date: 2026-10-01

Task: P004 / UI-01 through UI-10

Branch: `agent/cockpit-ui-foundation`

## Goal

Create the first maintainable RK3576 `cockpit_ui` foundation: one Qt Widgets
process, one main shell, routed pages, a shared state model, a UI-facing backend
contract, a deterministic mock backend, and host tests that do not require a
display or real hardware.

## Existing evidence

- The IMX6ULL archive is `SHADOW_BUILD`, `SOURCE_INCOMPLETE`,
  `UI_REFERENCE_ONLY`, `REIMPLEMENT`, and `LICENSE_UNVERIFIED`.
- Historical board evidence covers HDMI, USB touch, CAM0, audio, and Wi-Fi only
  under the recorded test conditions. It does not make those services live in
  this UI.
- A 2026-10-01 read-only inventory found an active GNOME/X11 session and an
  800x480 framebuffer. The board has Qt 5.15.8 runtime libraries, but no
  `qmake`, `qtpaths`, Qt development package, or Qt pkg-config metadata.
- The WSL host has no Qt5/Qt6 development package. Its AArch64 compiler is
  available, but the repository toolchain deliberately requires a verified
  target sysroot, which is not present.

## Scope

- Pure C++17 `UiState`, navigation mapping, preview metadata, backend contract,
  and `MockUiBackend`.
- Optional Qt5/Qt6 Widgets executable with Home, Camera, Media, Vehicle / Sensor,
  AI, Monitor, and Settings pages.
- Responsive 800x480-oriented layouts and touch targets of at least 44 px for
  primary actions.
- Host tests for state truthfulness, navigation, request recording, unavailable
  Rear/RTOS paths, simulated controls, and failed-result behavior.
- Architecture, task, feature matrix, and bring-up evidence updates.

## Out of scope

Camera capture, MPP/RGA/RTSP, ALSA, voice models, RKNN/RKLLM, AMP, RT-Thread,
RPMsg, MPU6050, real system metrics, real settings changes, service IPC, package
installation, Qt installation, boot/display-manager changes, and legacy archive
code or assets.

## Files

- `apps/cockpit_ui/`
- root `CMakeLists.txt`
- `docs/architecture/COCKPIT_UI.md`
- `docs/architecture/FEATURE_MATRIX.md`
- `docs/bringup/ui/qt_target_inventory.md`
- `docs/tasks/P004.md`, `docs/tasks/ROADMAP.md`, `docs/tasks/manifest.json`
- this plan

## Permission level and resource use

- L0: local source, documentation, configure, build, and host tests.
- L1: one serialized, read-only SSH inventory using the existing `lubancat`
  alias and board lock. No capture, device write, package install, or service
  change.
- No real camera, sound, NPU, RTOS, I2C, GPIO, or display-session workload.

## Implementation

1. Preserve concurrent work by using an isolated Git worktree and UI branch.
2. Record host and target Qt/display facts.
3. Implement and test the Qt-independent state/backend/navigation core.
4. Implement the optional Qt Widgets shell and page skeletons against that core.
5. Make absence of Qt an explicit configure-time skip instead of a repository
   configure failure.
6. Update documentation and task status without claiming service integration.
7. Run `bash scripts/dev/host_ci.sh` and targeted tests.

## Validation

- `bash scripts/dev/host_ci.sh`
- direct CTest execution through the host preset
- optional Qt target build only when `Qt5::Widgets` or `Qt6::Widgets` exists
- no board UI execution unless an AArch64 Qt build exists and the graphical
  session access is established without guessing `DISPLAY`.

## Failure recovery

The work is isolated from the concurrent Voice/AI worktree. Build output remains
under ignored `build/`; raw SSH output remains under ignored `artifacts/local/`.
No board state is changed, so recovery is limited to deleting the isolated build
directory or discarding this branch.

## Result

Implemented on the isolated UI branch. `bash scripts/dev/host_ci.sh` ran in WSL
and exited 0: 3/3 CTest and 6/6 Python tests passed. CMake printed
`UI_HOST_BUILD_SKIPPED_QT_NOT_FOUND`; it built `cockpit_ui_core` and
`cockpit_ui_foundation_test`, not the Qt executable.

The serialized board inventory exited 0 and established GNOME/X11, an 800x480
framebuffer, and Qt 5.15.8 arm64 runtime libraries. It also established that
`qmake`, `qtpaths`, and Qt development metadata are absent. The Host lacks Qt
development files, and no verified target sysroot exists, so no AArch64 UI was
built or copied. The application was not launched on the board and no touch
validation was performed.

UI-03 is `MOCK_TESTED`. UI-01, UI-02, and UI-04 through UI-09 are `PARTIAL`
pending an actual Qt build and display review. UI-10 remains `PLANNED`. The
optional `VOICE_AI_FOUNDATION.md` input was absent from the committed baseline;
the combined AI page therefore follows the product/interface documents and this
task's frozen mock fields only.

### Development environment follow-up

The user subsequently authorized installation. Standard Qt5 development packages
were installed on Host and target. Host CI now builds the Qt shell and passes
4/4 CTest plus 6/6 Python tests. The board natively builds an ARM aarch64 UI and
passes 4/4 CTest. A timed full-screen launch entered the existing X11 session and
exited 0 with `QT_XCB_GL_INTEGRATION=none`.

UI-01 is now `BOARD_STARTUP_TESTED`. UI-10 is `PARTIAL`: process startup is
proven, while visual inspection and physical touch confirmation remain open.
