# Core integration result

Status: `BOARD_TESTED_T5_PASS`.

Date: 2026-10-02. Command (exit 0):

```sh
timeout 30s build/apps/media_srv/media_cam0_probe \
  --mode core --device /dev/video11 \
  --snapshot-dir /home/cat/cockpit/media-cam0-20261002-01/snapshots
```

The in-process production chain was `InProcessVehicleCoreClient -> VehicleCore ->
RealMediaServiceAdapter -> MediaService -> V4l2MplaneCameraCapture`.

| command | ACK lifecycle | RESULT lifecycle | result |
|---|---:|---:|---|
| CAMERA_SELECT(front) | 1 | 2 | OK, `front CAM0 selected` |
| CAMERA_PREVIEW_START | 3 | 4 | OK, epoch 1, actual format/buffers recorded |
| CAMERA_SNAPSHOT | 5 | 6 | OK, real PPM path and frame metadata |
| CAMERA_PREVIEW_STOP | 7 | 8 | OK, STREAMOFF after four frames |

Canonical revision advanced to 2 with Preview `STREAMING/RUNTIME`, then to 4 with
Preview `STOPPED/RUNTIME`. ACK and RESULT remained separate lifecycle events.

Host and native AArch64 tests additionally verify Rear returns UNAVAILABLE without
changing Front selection; Recording/RTSP are synchronously rejected as not
implemented; repeat Start/Stop is idempotent; timeout becomes canonical Error; and
a later successful service callback is fenced without reverting Error state.
