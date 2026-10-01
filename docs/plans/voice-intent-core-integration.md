# Voice Intent → Vehicle Core integration ExecPlan

## Goal and baseline

Merge Intent commit `97dbc29` with Vehicle Core foundation `8445677` on an isolated branch, then prove synthetic `ASR_FINAL → deterministic rule → typed candidate → bounded dispatcher → strict sink adapter → Vehicle Core → Mock service → ACK/RESULT/canonical state`. Keep both histories. Their merge base is `42c3c11`.

## Scope and permission

L0 Host source, CMake, docs and tests only. No live VAD callback hookup, real camera/audio/RPMsg/RTOS, UI merge, board access, system install, model or third-party download. Work only in `/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-intent-core`.

## Interfaces and risks

The existing voice candidate now uses typed `ActionParameter`, deadline and ASR sequence; the Core branch's sink still reads arbitrary `parameters` strings and equates `OPEN_CAMERA` to `CAMERA_SELECT front`. Resolve this explicitly. Only typed, exactly mapped actions may enter Core. `OPEN_CAMERA` is unsupported because Core has no preview-start command; camera close was already `NO_MATCH` in the router. The Vehicle wire must preserve voice generation/event sequence and Core idempotency must remain an independent guard.

`VoiceSessionController::deliver_event` invokes callbacks under its mutex. The callback may only copy the FINAL into a bounded queue. A joinable owner worker routes and submits after callback return. A bounded recent-FINAL cache fences duplicates by epoch/session/generation/ASR sequence before sending; the stable session request ID is reused at Core.

## Steps and validation

1. Resolve merge conflicts semantically and adapt obsolete Core voice bridge tests; preserve both histories in a normal merge commit.
2. Implement strict typed mapping, exact token/epoch/deadline checks and explicit unsupported actions. Carry generation/event sequence in the Core command schema and roundtrip.
3. Add bounded `VoiceIntentDispatcher` with start/stop/join, no callback reentrancy and bounded duplicate cache.
4. Add Host integration tests for success ACK/RESULT/state, negation/unknown/PARTIAL, duplicate FINAL, stale/cancel/timeout, failure/late success, simulated buzzer and shutdown.
5. Run `bash scripts/dev/host_ci.sh`, existing Intent/ASR/VAD/Core regression, `ctest --repeat until-fail:50` for the new integration, and ASan/UBSan. No board resource is reserved.

## Result

The ordinary merge retained both parents. `OPEN_CAMERA` returns `UNSUPPORTED_ACTION`; `关闭摄像头` remains `NO_MATCH`. The adapter maps only typed camera select, recording start/stop and simulated LED/buzzer to Core commands. Generation and ASR sequence survive the command wire roundtrip. A joinable dispatcher separates Controller callbacks from Router/Core and its queue and duplicate cache are finite.

`bash scripts/dev/host_ci.sh` passed 18/18 CTest and 6/6 Python; those include existing Intent, ASR, VAD and Core Host tests. The integration test passed 50 consecutive CTest repetitions. A separate ASan/UBSan build and the integration CTest passed 1/1. The Host synthetic success, rejection, duplicate, timeout/late result, simulated and shutdown cases meet `VOICE_INTENT_CORE_INTEGRATION_PASS`. No board or real service was contacted.

## Failure recovery

Repair only this worktree; retain any failed-test evidence. No reset/clean or silent action substitution. If a precise Core command is absent, return `UNSUPPORTED_ACTION`.
