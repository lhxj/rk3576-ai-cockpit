# Snapshot result

Status: `BOARD_TESTED_T4_PASS`.

Date: 2026-10-02. The project `MediaService` started CAM0 preview, waited for an
owned frame, converted stride-aware NV12 to RGB888, wrote PPM, and stopped cleanly.

- Path: `/home/cat/cockpit/media-cam0-20261002-01/snapshots/cam0_e1_s0.ppm`
- Metadata: camera `front`, epoch 1, sequence 0, V4L2 timestamp
  37717826871000 ns.
- File: Netpbm P6, 1632x1224 RGB, 5,992,721 bytes.
- SHA256: `d3bff1314601817484b35d93b432d93ca84c80ca5e32a071f87eb2454984fc2c`.

Static visual inspection of the copied artifact found a recognizable scene with
continuous geometry: no black/green frame, UV swap pattern or stride shearing was
visible. The image has a bright magenta-lit area and the physical camera orientation
is not treated as a software rotation defect. This one frame is not an image-quality
calibration result.

Snapshot-while-stopped returns `CAMERA_NOT_STREAMING`; this is covered by the native
AArch64 `media_service` CTest as well as Host tests.
