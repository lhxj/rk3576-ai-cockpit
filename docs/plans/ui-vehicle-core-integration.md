# UI + Vehicle Core integration plan

## Goal

Connect the existing Qt shell to the Host-tested Vehicle Core control plane through
one UI-facing adapter, while preserving `MockUiBackend` and the truthful state model.
The completion target is `UI_VEHICLE_CORE_INTEGRATION_PASS`; real Camera, Audio,
Voice, AI, RTOS, RPMsg and sensor services remain outside this task.

## Evidence and fixed inputs

- Integration branch/worktree: `agent/ui-vehicle-core-integration` at
  `/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-ui-core`.
- Vehicle Core foundation: `8445677`; UI foundation: `58f69e5`; common ancestor:
  `75607d9`.
- The foundations were combined by ordinary merge at `61b8507`; the merged baseline
  passed 13/13 CTest before integration edits.
- The target uses Qt 5.15.8 and GNOME/X11. Existing UI bring-up requires
  `QT_XCB_GL_INTEGRATION=none`; this does not validate the GPU path.

## Scope

- Add `VehicleCoreUiBackend -> IVehicleCoreClient` with state and RESULT mapping.
- Generate request IDs, boot epoch, session IDs and deadlines inside the backend.
- Fetch a full snapshot before subscription, reject stale/equal revisions, and keep
  canonical Vehicle Core state as the only service truth.
- Deliver callbacks to Qt through queued invocation and provide bounded, joinable
  RESULT handling with deterministic shutdown.
- Route Camera, Recording, RTSP, Media, Voice session and simulated LED/Buzzer UI
  actions through Vehicle Core.
- Add `--backend mock|core` and deterministic Host profiles.
- Add pure C++ and Qt offscreen integration tests, sanitizer coverage where practical,
  Host CI evidence, then an AArch64/board userland build and launch record.

## Out of scope

No UDS, ZeroMQ or TCP transport; no V4L2, ALSA, Sherpa, RKNN, RKLLM, RPMsg,
MPU6050, MPP/RGA/RTSP implementation; no `/boot`, kernel, DTB, U-Boot or system
service changes. No code is imported from the IMX6ULL reference archive.

## Implementation sequence

1. Extend the client boundary with the current boot epoch and add UI-side mapping.
2. Implement backend lifecycle, revision gate, pending overlays and RESULT worker.
3. Move request construction out of `MainWindow`; wire start/stop and queued callbacks.
4. Update pages for start/stop and voice cancel semantics plus revision/service health.
5. Add runtime profiles and CLI backend selection.
6. Run pure C++ tests, Qt offscreen integration, repeated shutdown and Host CI.
7. Update architecture/status/task evidence and perform allowed board userland build.

## Validation

- `bash scripts/dev/host_ci.sh`
- focused CTest targets for backend mapping and Qt click-to-adapter flow
- 20 repeated backend/runtime construction and clean shutdown cycles
- ASan/UBSan build of pure C++ integration tests when the toolchain supports it
- native/compatible AArch64 build and bounded X11 launch under `/home/cat/cockpit/`
- physical touch remains pending until the user confirms the integration build

## Recovery and resource use

All edits are confined to the integration worktree. Vehicle Core and backend workers
are joinable and stopped in dependency order. Board work is userland-only, bounded,
and uses no camera, audio, model or RTOS resource.

## Result

Implemented on 2026-10-01. The ordinary merge at `61b8507` preserved both foundation
histories. Host CI exits 0 with 16/16 CTest and 6/6 Python tests; the pure C++ backend
test also passes ASan+UBSan and 50 consecutive normal repetitions. Native RK3576 build
and target CTest pass 16/16, and all four bounded core profiles plus one full-screen
normal run enter the existing X11 session and exit cleanly.

One initial CI run failed because the timeout test advanced its fake clock before it
had confirmed dispatch of that specific request. The test now waits on the new adapter
invocation count; no implementation timeout or assertion was weakened.

UI-CORE-01 through UI-CORE-07 are Host tested. UI-CORE-08 is
`BOARD_TOUCH_TESTED_MOCK_INTEGRATION`: the user confirmed the normal path, Rear
rejection, RTOS-offline `UNAVAILABLE` result and media-timeout `Error` result on the
physical panel. The task reaches `UI_VEHICLE_CORE_INTEGRATION_PASS` within its stated
Mock-adapter boundary. UI-CORE-09 remains planned. No real service or hardware backend
was opened.
