# CAM0 media pipeline

## Ownership and planes

`MediaService` is the only owner of CAM0. Qt and Vehicle Core never open V4L2 nodes.
The control plane is:

```text
Qt -> VehicleCoreUiBackend -> VehicleCommand -> VehicleCore
   -> RealMediaServiceAdapter -> MediaService operation -> RESULT/canonical state
```

The frame data plane is separate:

```text
OV8858 -> rkisp mainpath -> V4L2 MPLANE MMAP
  -> deep-copied CapturedFrame -> latest-frame mailbox
  -> Qt preview worker (NV12 to RGB) -> queued GUI update
```

No frame payload is placed in VehicleCommand, ACK, RESULT or VehicleState.

## V4L2 and frame ownership

The optional Linux backend uses `VIDEO_CAPTURE_MPLANE`, `open/ioctl/mmap/poll`, one
capture worker and no detached thread. The initial contract requires an actual
1632x1224 NV12 format with one memory plane and at least three MMAP buffers. Driver
format, plane count, stride, sizeimage and buffer count are accepted only after
`G_FMT/QUERYBUF` evidence.

An MMAP pointer is valid only between DQBUF and QBUF. The worker copies `bytesused`
into a RAII-owned `CapturedFrame`, requeues the driver buffer immediately, and only
then publishes the owned frame. Stop joins the worker, issues STREAMOFF, unmaps every
buffer and closes the fd through the service lifecycle.

## Epoch, sequence and timestamps

Every successful STREAMON creates a strictly increasing `stream_epoch`. Frames carry
camera id, actual format, stride, sizeimage, bytesused, V4L2 sequence, driver timestamp
and a steady-clock dequeue timestamp. Capture sequence gaps and preview drops are
separate counters. Consumers reject frames from an older epoch after restart.

## Preview and Qt boundary

Preview uses a capacity-one latest-frame mailbox. Replacing an unconsumed frame is an
explicit preview drop. The Qt preview bridge throttles conversion, converts NV12 with
the actual stride outside the GUI thread, coalesces pending image updates and enters
Qt widgets only through a queued invocation. Camera layout preserves the 4:3 image in
a letterboxed area at 800x480.

## Snapshot

Snapshot requires an active stream and uses the most recent owned frame. The first
format is RGB PPM written below the configured user-owned artifact directory. RESULT
detail records camera id, epoch, sequence, timestamp and path. Snapshot while stopped
returns `CAMERA_NOT_STREAMING`.

## Canonical state

Preview has independent `Starting`, `Streaming`, `Stopping`, `Stopped` and `Error`
states. Media service health, Camera availability and Preview activity remain distinct.
ACK only creates pending state; canonical Streaming is committed only after successful
STREAMON completion. Core deadline and late-result fencing remain authoritative.

## Deferred work

The original Preview/Snapshot baseline deferred Recording. That historical state
was superseded on 2026-10-02 by the MPP Annex-B path documented in
`CAM0_RECORDING_PIPELINE.md`. RTSP now has a Host-tested single-client
implementation documented in `CAM0_RTSP_PIPELINE.md`; its RK3576 board gates
remain pending while T0 access is blocked.
CAM1, RGA, DMA-BUF, DRM/EGL, JPEG containers and RKNN remain future work. A future
daemon may replace the in-process service without changing Camera page control
or data contracts.
