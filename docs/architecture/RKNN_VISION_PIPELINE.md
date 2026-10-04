# CAM0 RKNN vision pipeline

Status date: 2026-10-02.

## Boundary

The first vision integration is a single CAM0 MobileNetV1 classification path:

```text
V4L2 MPLANE CAM0 (MediaService is the only owner)
  -> immutable owned NV12 CapturedFrame
  -> VisionRuntime latest-frame-wins queue (capacity 2, target 8 fps)
  -> stride-aware CPU NV12 to RGB + 224x224 letterbox
  -> one RknnVisionBackend / one rknn_context
  -> structured VisionResult
  -> VisionUiBackend
  -> Qt AI / Vision panel
```

`MediaService::VisionStart` retains the already existing capture producer. Preview,
recording, RTSP and vision therefore consume the same owned frame and never open a
second camera node. Vision does not create an MPP encoder. The first implementation
uses CPU preprocessing; RGA and DMA-BUF remain future optimizations.

## Backend and lifecycle

`IVisionBackend` exposes load, infer, unload, state and immutable model metadata.
`RknnVisionBackend` reads the external `.rknn` once, creates one RKNN context, queries
its input/output attributes and SDK/driver versions, then reuses that context for all
frames. `VisionRuntime` owns one joinable worker. Stop first rejects new frames and
clears the bounded queue, joins the worker, and then unloads the model. No detached
thread is used.

`SerialInferenceScheduler` is shared-facing infrastructure for future RKNN/RKLLM
serialization. This design does not assume NPU preemption or QoS.

## Frame identity and staleness

Each `VisionFrame` and `VisionResult` carries `camera_id`, V4L2 `sequence`,
`stream_epoch`, capture timestamp and an inference id. A higher epoch immediately
clears queued frames. A result produced after a newer epoch has arrived is counted as
stale and is not published. UI text includes the result epoch and sequence so a frame
from an old stream cannot silently look current.

## Bounded scheduling

Input sampling targets 8 fps. The pending queue capacity is two and discards the
oldest queued frame when full. Capture callbacks only copy metadata and a shared
ownership handle, then return. They never run preprocessing or inference. Metrics
distinguish sampling, queue and stale-epoch drops. This is a freshness policy, so
sampling drops are expected at a 30 fps capture rate.

## Preprocessing and output

The baseline accepts even-sized NV12 with one memory plane and an explicit
`bytes_per_line`. Y and interleaved UV addresses use that stride. It converts to RGB,
preserves aspect ratio by letterboxing, and passes NHWC UINT8 to RKNN. The selected
model embeds its mean/std quantization metadata, so no second application-side
normalization is applied. Detection coordinates are supported by the contract and
are mapped back through the saved letterbox transform, although MobileNetV1 returns
classification scores only.

`VisionResult` contains classifications, optional detections, source identity and
preprocess/inference/postprocess/end-to-end timing. The current artifact has no
separately verified label file; the UI therefore uses the auditable label
`ImageNet class <id>` instead of inventing class names.

## UI handoff and truthfulness

The real runtime reports `READY` only after `rknn_init` and tensor queries succeed,
and `RUNNING` only after a current-epoch result is published. `VisionUiBackend`
decorates the existing Vehicle Core UI backend. Its callback may originate on the
vision worker, while `MainWindow` already transfers backend callbacks to the Qt GUI
thread with a queued invocation. The AI page never calls V4L2 or RKNN.

No real backend selected means the existing NOT READY / MOCK state remains visible.
This integration does not establish object detection, tracking, CAM1, RGA/zero-copy,
RKLLM vision, Voice-to-Vision control, or model redistribution rights.
