# CAM0 media service integration plan

## Goal

Replace only the Camera path of the existing Qt/Vehicle Core Mock integration with
an in-process `RealMediaServiceAdapter -> MediaService -> ICameraCapture` path and a
separate bounded preview data plane. The target is
`MEDIA_CAM0_CORE_INTEGRATION_PASS`; Recording, RTSP, CAM1, encoding, AI and zero-copy
remain outside this task.

## Baseline and evidence

- Stacked base: `agent/ui-vehicle-core-integration` at `0d4c6a1`.
- Worktree: `/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-media-cam0`.
- Branch: `agent/media-cam0-integration`.
- Historical CAM0 result: OV8858, 1632x1224 NV12 MPLANE, one memory plane,
  stride 1632, sizeimage 2996352 and about 29.88 fps.
- T0 read-only inventory on 2026-10-01 resolves sensor `m00_b_ov8858 3-0036`
  through `/dev/media0` and `rkisp-vir0` `/dev/media1` to mainpath `/dev/video11`.
  `/dev/video-camera0` currently resolves to that node. The mapping is runtime
  evidence, not a permanent code constant.

## Unknowns

- The mainpath rejects `VIDIOC_G_PARM`; measured capture rate must therefore be
  recorded separately from the sensor graph's 30 fps interval.
- Actual MMAP buffer count, current stream behavior, project-path frame rate,
  restart behavior, snapshot image and Qt preview performance require staged board
  tests.

## Scope and files

- `libs/media/`: owned frames, capture interface, fake capture, latest-frame mailbox,
  NV12 conversion and optional Linux V4L2 MPLANE backend.
- `apps/media_srv/`: process-independent MediaService plus Vehicle Core adapter.
- `apps/vehicle_core/`: minimal preview commands and canonical preview state.
- `apps/cockpit_ui/`: preview command mapping, Qt preview bridge and Camera page.
- root/app CMake, P003/task/status/architecture and bring-up evidence.

## Permissions and resource use

- L0 Host implementation, tests and documentation.
- User-authorized board work is limited to the `cat` account, one CAM0 owner at a
  time, finite frame counts and bounded UI/stability runs under `/home/cat/cockpit/`.
- No CAM1, sudo, package installation, boot/kernel/DT changes or sensor-register I/O.

## Sequence

1. Freeze T0 device resolution evidence and add Host-testable media contracts.
2. Implement and test owned-frame, mailbox, snapshot and adapter/control semantics.
3. Add optional native V4L2 MPLANE backend and a finite hardware probe.
4. Run Host CI and ASan/UBSan.
5. Deploy source to the board, build with `COCKPIT_ENABLE_V4L2_CAMERA=ON`, then run
   T1 through T8 serially, stopping expansion on the first failed gate.
6. Update evidence and task states, push the stacked branch and create a Draft PR
   based on `agent/ui-vehicle-core-integration`.

## Validation and recovery

- `bash scripts/dev/host_ci.sh`.
- Focused media/Core/Qt tests with synthetic frames and offscreen Qt.
- ASan/UBSan for MediaService, frame ownership and shutdown.
- Board test process has internal poll deadlines/frame limits plus an external
  timeout; only the task PID is managed. A failed stage closes/unmaps the device and
  the next stage is not attempted until evidence is understood.

## Result

T0-T8 completed on 2026-10-02. Current project evidence includes actual CAM0
format negotiation, >=300-frame stream, 20 restart cycles, owned-frame PPM snapshot,
Vehicle Core/RealMediaServiceAdapter ACK/RESULT/canonical state, Qt X11 preview and a
bounded 300-second run. Native AArch64 default CTest is 19/19. Host ASan/UBSan media
tests pass; Qt offscreen has only the separately recorded framework LSan exit
allocation. The user completed T7 on the physical 800x480 display and confirmed the
real preview, visual presentation, touch navigation, snapshot, unavailable actions
and preview restart path. The final grade is `MEDIA_CAM0_CORE_INTEGRATION_PASS`.
