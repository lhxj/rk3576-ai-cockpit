# Voice Intent + Real CAM0 integration

## Validated boundary

The validated chain is:

```text
synthetic ASR_FINAL
  -> VoiceIntentDispatcher
  -> DeterministicIntentRouter
  -> typed CandidateAction
  -> VehicleCommandSinkAdapter
  -> VehicleCore
  -> RealMediaServiceAdapter
  -> MediaService
  -> RK3576 V4L2 CAM0
  -> terminal RESULT
  -> canonical VehicleState
```

This is a real camera-control integration with synthetic final ASR text. It is
not evidence for microphone capture, live VAD, wake word, RKLLM, recording,
RTSP, CAM1, RKNN, AMP or RT-Thread.

## Fixed mapping

| Utterance rule | CandidateAction | VehicleCommand | Parameter | Resulting state |
|---|---|---|---|---|
| 打开摄像头 and exact aliases | `OPEN_CAMERA` | `CAMERA_PREVIEW_START` | none | `STREAMING` after RESULT success |
| 关闭摄像头 and exact aliases | `CLOSE_CAMERA` | `CAMERA_PREVIEW_STOP` | none | `STOPPED` after RESULT success |
| 切换前摄 and exact aliases | `SELECT_CAMERA(Front)` | `CAMERA_SELECT` | fixed `camera=front` | front remains selected |
| 切换后摄 and exact aliases | `SELECT_CAMERA(Rear)` | `CAMERA_SELECT` | fixed `camera=rear` | terminal unavailable; selection and stream stay unchanged |

The adapter creates only the fixed command and typed parameter shown above.
Raw ASR text never enters command parameters. Negated, PARTIAL and NO_MATCH
input cannot invoke `VehicleCore` or `MediaService`.

## Lifecycle semantics

ACK means `VehicleCore` accepted the command into its bounded queue. It does
not mean V4L2 succeeded. Only the terminal RESULT changes the authoritative
preview state to `STREAMING` or `STOPPED`. A failed or unavailable RESULT must
not be displayed as success. Deadline timeout is terminal, and a late service
success cannot advance the canonical revision.

Repeated identical FINAL events are suppressed by the dispatcher identity
cache. Core request idempotency is an independent second guard. The integration
test measures `MediaService::preview_start_requests()` to prove a duplicate
FINAL causes exactly one service call.

## Camera ownership

The real path uses the current sensor/media graph resolution and receives the
resolved video node explicitly; it does not treat an old `/dev/videoN` value as
a permanent camera identity. `MediaService` owns STREAMON/STREAMOFF and all
DQBUF/QBUF activity. Close, process exit and errors unwind buffers and file
descriptors. The finite board suite also reopens the device after shutdown to
verify release.

## Evidence level

Host validation uses FakeCamera and covers both successful and failure paths.
On 2026-10-02 a native RK3576 build passed 27/27 CTest, and the real OV8858 CAM0
suite passed open, close, restart, duplicate FINAL, rear unavailable, negation
and device-reopen checks. See
`../bringup/voice-intent-real-cam0/BOARD_RESULT.md` for exact commands and the
recorded pre-hardware resolver failure.
