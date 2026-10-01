# CAM0 real recording integration plan

## Goal

Add one Rockchip MPP H.264 Annex-B recording consumer to the existing single-owner
CAM0 `MediaService`. Preview and recording must share one V4L2 capture, while UI
and synthetic intent commands continue through the same Vehicle Core adapter.
The target grade is `MEDIA_CAM0_RECORDING_PASS`.

## Baseline and evidence

- Branch: `agent/media-cam0-recording`, based on `b8b27ec`.
- Existing real path: OV8858 CAM0, 1632x1224 NV12 MPLANE, about 29.88 fps.
- Existing preview/snapshot, ACK/RESULT, timeout fencing and synthetic intent
  paths are retained.
- T0 on 2026-10-02 found installed arm64 packages
  `librockchip-mpp-dev`, `librockchip-mpp1` and `rockchip-mpp-demos` 1.5.0-1.
  Headers, `librockchip_mpp.so.1`, `mpi_enc_test` and encoder symbols are present.
  The packaged `rockchip_mpp.pc` reports 1.3.9; this metadata conflict requires
  an actual build and encoder probe and must not be silently normalized.

## Implementation

1. Add an `IMediaRecorder` boundary, Host `FakeMediaRecorder`, and optional
   board-only `MppH264Recorder` selected by CMake.
2. Refactor `MediaService` into independent preview and recording consumers over
   one capture owner. Recording-only starts capture without changing Preview.
3. Use a bounded nonblocking recorder queue. Overflow stops the recording with
   `RECORDING_BACKPRESSURE`; the capture thread never waits for the encoder.
4. Complete START only after the first encoded packet. STOP stops submissions,
   drains/EOS, closes the file, and then completes.
5. Extend the existing RealMediaServiceAdapter, UI runtime and synthetic intent
   harness; do not create a second recording control path.

## Fixed baseline

- Format: H.264/AVC Annex-B elementary stream.
- Input: NV12 1632x1224 at measured CAM0 cadence.
- Rate control: CBR, 8,000,000 bit/s target, 7,500,000 minimum,
  8,500,000 maximum.
- GOP: 60 frames; High profile, level 4.0; input/output 30/1 fps.
- Queue: 12 owned frames. Preview remains latest-frame-wins; recording never
  silently drops.

## Permissions and limits

Host implementation/tests are L0. The user request authorizes the staged board
validation as `cat`, one CAM0 owner, bounded files under `/home/cat/cockpit/`, and
the specified 10-second, 20-cycle and 300-second runs. No sudo, package install,
CAM1, RTSP, MP4, FFmpeg/GStreamer/OpenCV, RGA, zero-copy, AMP or RT-Thread work.

## Validation

- `bash scripts/dev/host_ci.sh`.
- Focused Host recorder lifecycle, ownership, intent and failure tests.
- ASan/UBSan for MediaService/recorder lifecycle.
- RK3576 native build/CTest with V4L2 and MPP enabled.
- T1 synthetic NV12 encode; T2 10-second real capture; T3 20 cycles; T4 shared
  preview+recording; T5 recording-only; T6 synthetic intent; bounded 300-second
  stability and NAL/file/resource inspection.

## Recovery

Every board test has an internal duration/frame bound and external timeout. Only
the known test PID may be stopped. On MPP/V4L2 error, stop expansion, retain the
bounded log, verify camera ownership, and confirm device reopen before one
evidence-based retry. Output streams remain board-local and are not committed.

## Result

Completed on 2026-10-02 with grade `MEDIA_CAM0_RECORDING_PASS`.

- Host CI passed 28/28 CTest plus 6/6 Python tests and shell checks. Focused
  ASan/UBSan recorder and intent tests passed 2/2.
- RK3576 native build with V4L2+MPP enabled and native CTest 28/28 passed.
- Synthetic MPP, 10-second CAM0 recording, 20 start/stop cycles, shared
  Preview+Recording, Recording-only, and synthetic ASR FINAL gates passed.
- The final five-minute shared run captured 8,974 frames and encoded 8,964 at
  29.8755/29.8778 fps, queue peak 1, with zero overflow, sequence gap, V4L2 or
  encoder error. The 301,328,134-byte file was closed and validated.
- The first five-minute attempt reached the data volume but the validation tool
  timed out while scanning the entire 300 MB file and collected no resources.
  Bounded NAL scanning and PID sampling were corrected; that interrupted file
  is not PASS evidence. See `docs/bringup/media-recording/STABILITY.md`.
- The installed MPP runtime retains one process-level FD after first use, but
  its count is stable from cycle 1 through 20; threads return to baseline.

This result covers Annex-B H.264 recording only. MP4, RTSP, CAM1, live voice,
RGA and zero-copy remain outside scope.
