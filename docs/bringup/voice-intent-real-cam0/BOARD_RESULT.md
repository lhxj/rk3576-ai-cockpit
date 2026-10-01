# Voice Intent + Real CAM0 board result

## Scope

- Date: 2026-10-02 (Asia/Shanghai)
- Board: LubanCat-3 v2 / RK3576 / Debian 12 / kernel 6.1.99-rk3576
- User: `cat`
- Build: native Debug, GCC 12.2, Qt 5, V4L2 enabled
- Input: synthetic `ASR_FINAL` text only
- Camera: live OV8858 CAM0 resolved from its media graph
- Grade: `VOICE_INTENT_REAL_CAM0_PASS`

No microphone, live VAD, wake word, recording, RTSP, CAM1, RKNN, RKLLM, AMP or
RT-Thread path was exercised.

## Build and tests

The board configured and built the project with:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DCOCKPIT_ENABLE_V4L2_CAMERA=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Result: 27/27 CTest passed in 2.78 seconds.

## Live resolution

Read-only inventory identified `m00_b_ov8858 3-0036`. The successful resolver
walked `/dev/media1` `rkisp_mainpath` and returned `/dev/video11`. The service
then queried 1632x1224 NV12 MPLANE, one memory plane, stride 1632 and sizeimage
2996352. `/dev/video11` is recorded evidence for this run, not a permanent
front-camera identifier.

## Preserved failed attempt

The first suite invocation did not touch camera hardware. A caller-side shell
resolver accepted the diagnostic string `Entity 'rkisp_mainpath' not found`
from `/dev/media0` as though it were a device path. The executable rejected that
invalid path and returned failure. The resolver was corrected to accept only
values matching `/dev/video*`; one bounded retry then used the live graph result
and passed. This failure is retained because it demonstrates why diagnostic
output must not be parsed as a device node.

## Successful finite suite

The corrected run produced these relevant results:

```text
RESOLVED_MEDIA=/dev/media1
RESOLVED_NODE=/dev/video11
action=OPEN_CAMERA command=CAMERA_PREVIEW_START ack=1 result=2 preview=STREAMING
T1=PASS frames=3 epoch=1
action=CLOSE_CAMERA command=CAMERA_PREVIEW_STOP ack=3 result=4 preview=STOPPED
T2=PASS
action=OPEN_CAMERA command=CAMERA_PREVIEW_START ack=5 result=6 preview=STREAMING
T3=PASS epoch=2
T4=PASS duplicate_final_service_calls=1
T5=PASS rear=UNAVAILABLE selected=FRONT preview=STREAMING
T6=PASS negation_core_commands=0 hardware_actions=0
action=CLOSE_CAMERA command=CAMERA_PREVIEW_STOP ack=9 result=10 preview=STOPPED
camera_reopen=PASS
VOICE_INTENT_REAL_MEDIA_TEST_PASS
SUITE_EXIT=0
```

The post-run process/owner check was empty. The explicit reopen passed, proving
the finite test released the V4L2 device after STREAMOFF and shutdown.

## Interpretation

- Open and close each preserve ACK/RESULT separation and change canonical state
  only on terminal success.
- The restart increments `stream_epoch`, so stale frames/results can be fenced.
- Duplicate FINAL reaches `MediaService` exactly once.
- Rear selection returns unavailable without corrupting the active front stream.
- Negated input causes zero Core commands and zero hardware actions.
- This evidence does not upgrade live voice or CAM1 status.
