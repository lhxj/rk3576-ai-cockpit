# Deterministic intent router ExecPlan

## Goal and baseline

From VAD commit `b6f8e9a`, route only typed `ASR_FINAL` text through an explicit command whitelist to a `CandidateAction` submitted to `IVehicleCommandSink`. The current live ASR and VAD paths remain unchanged. The existing foundation action type has three commands and string parameters; extend it minimally for the currently mappable intents and typed camera/on-off parameters. Read-only inspection of the parallel `vehicle_core` branch found `CAMERA_SELECT` but no camera preview-stop command, so camera close remains `NO_MATCH`.

## Scope and permissions

L0 repository changes and Host tests only. No board, microphone, model, camera, RPMsg, hardware or real `vehicle_core` implementation. The sink in tests and CLI only records candidates. Work in an isolated worktree/branch; leave other branches untouched.

## Implementation

1. Add a centralized C++17 rule table, conservative UTF-8 text normalization, collision validation, negation and multi-command rejection.
2. Reuse `SessionToken`, `AsrEvent`, `VoiceSessionController`, `CandidateAction` and `IVehicleCommandSink`; add typed action parameters, deadline and a controller validation method. Match and dispatch are separate, synchronous operations.
3. Add a text-only CLI and TSV fixtures. Host tests cover every rule/alias, malformed and unknown input, partial, cancellation, generation/deadline, sink failure, repeated exact, collisions and no fallback.
4. Run default Host CI, ASan/UBSan, and optional existing Sherpa/VAD integration if external dependencies are present. Document the future event handoff from live ASR and vehicle_core mapping.

## Validation and recovery

Expected: Host build/test without Sherpa or ALSA, no action on rejected input, no worker or model in router. If a gate fails, fix task-owned source and rerun that gate. Do not alter VAD thresholds, Sherpa decoding or model assets. No board resource is reserved.

## Result

`DETERMINISTIC_INTENT_ROUTER_PASS` for the Host text-to-candidate boundary. The rule table has nine mappable intents, 20 aliases and zero normalization collisions. All exact/alias entries, safety rejections, PARTIAL exclusion, typed parameters, stale/cancelled/expired context, one-attempt sink failure and 1000 random unknown ASCII inputs were tested. `bash scripts/dev/host_ci.sh` passed 13/13 CTest and 6/6 Python; ASan/UBSan intent test passed 1/1; optional existing Sherpa file/fixed-live/VAD regression passed 4/4 with external assets. No board was touched. Real VAD FINAL handoff and `vehicle_core` sink translation are intentionally future integration tasks; the independent adapter currently assumes the old string parameter layout and must be updated when histories are joined.
