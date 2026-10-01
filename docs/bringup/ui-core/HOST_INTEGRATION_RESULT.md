# Host UI + Vehicle Core integration result

Date: 2026-10-01

Environment: WSL Ubuntu 22.04.5, GCC 11.4, Qt 5.15.3, x86_64. No board or
business-hardware device was accessed by these Host tests.

## Result

`bash scripts/dev/host_ci.sh` exited 0 after the integration and task-manifest
updates:

- CMake Debug configure/build: PASS;
- CTest: 16/16 PASS;
- Python repository tests: 6/6 PASS;
- shell syntax checks: PASS;
- final marker: `HOST_SCAFFOLD_CHECKS_PASSED (not hardware validation)`.

The two new integration tests are:

- `cockpit_ui_vehicle_backend`: pure C++ mapping, 10→12→11 revision rejection,
  UI action mapping, ACK pending, controlled success, failure, timeout plus late
  success, Rear unavailable, RTOS offline, RTOS simulated success and 20 clean
  runtime cycles;
- `cockpit_ui_qt_core_integration`: Qt offscreen clicks reach Mock adapters and
  asynchronous state/RESULT returns to widgets without a real X server.

Startup tests cover both default `--backend mock` and explicit
`--backend core --profile normal` with `QT_QPA_PLATFORM=offscreen`.

An early full-CI run exposed a race in the test itself: it advanced `FakeClock`
before confirming that the new timeout command had reached the adapter. The assertion
then tried to complete a request that had timed out before dispatch. The test was
corrected to wait for the invocation count associated with that request; no production
sleep or relaxed assertion was added. The corrected pure C++ integration test then
passed 50 consecutive CTest repetitions.

## Sanitizers

A separate GCC 11.4 Debug build used:

```text
-fsanitize=address,undefined -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=1
UBSAN_OPTIONS=print_stacktrace=1
```

`cockpit_ui_vehicle_backend_test` built and passed 1/1 with exit 0. Qt plugin code was
not included in the sanitizer claim. Existing TSan remains blocked by the recorded WSL
runtime mapping failure and was not reclassified.

## Evidence boundary

The result is `HOST_TESTED_MOCK_INTEGRATION`. It validates Qt/UI semantics,
`IVehicleCoreClient`, Vehicle Core and Mock service adapters. It does not validate
Camera, V4L2, Recording, RTSP, playback, ALSA, Voice models, RKNN/RKLLM, RTOS, RPMsg,
MPU6050 or GPIO.
