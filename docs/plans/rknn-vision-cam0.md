# RKNN Vision Integration — CAM0 to UI

## Goal

Reach `VISION_RKNN_CAM0_PASS` with one real CAM0 owner, a bounded real RKNN
classification path, structured current-epoch results and truthful Qt state.

## Scope

- MobileNetV1 RK3576, external board-installed artifact.
- Shared `MediaService` CAM0 frames.
- CPU NV12-to-RGB letterbox preprocessing.
- One load-once RKNN context and a latest-frame-wins queue of two.
- AI/Vision model, result and rate fields through the existing UI backend boundary.
- Host lifecycle/preprocessing tests and RK3576 fixed-image, CAM0, stability and
  media-concurrency tests.

## Out of scope

CAM1, detection/tracking, a second model, RGA, DMA-BUF zero-copy, RKLLM vision,
Voice-to-Vision commands, model conversion and model redistribution.

## Implementation

1. Add structured frame/tensor/result contracts and a serial inference scheduler.
2. Add `VisionRuntime` with sampling, capacity two, epoch fencing and metrics.
3. Add optional `RknnVisionBackend`; keep normal Host CI independent of RKNN.
4. Add a `MediaService` vision consumer without adding a capture owner or encoder.
5. Decorate `VehicleCoreUiBackend` with real vision state; preserve the Qt queued
   callback boundary.
6. Add a target-only probe for controlled real-hardware combinations.

## Validation gates

- T0: runtime, driver, model, ABI, hash and license inventory.
- T1: official fixed image through the installed RKNN runtime.
- T2: CAM0 to RKNN, no preview or encoder.
- T3: five-minute CAM0 plus vision stability and resource sampling.
- T4: preview plus vision.
- T5: preview, recording, RTSP and vision with one capture and one encoder.
- UI: real Qt AI page displays runtime model/rate/current result.
- Host CI and ASan/UBSan remain green.

## Result

The result is recorded in `docs/bringup/vision/`. A page skeleton or successful
model load alone is insufficient for PASS.
