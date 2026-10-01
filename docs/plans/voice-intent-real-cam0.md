# Voice Intent + Real CAM0 integration plan

## Goal

Merge the fixed `agent/intent-vehicle-core-integration` commit `c50aa29` into the
validated real-CAM0 base `914b5a5`, then prove the bounded chain:

```text
synthetic ASR_FINAL -> DeterministicIntentRouter -> CandidateAction
-> VehicleCommandSinkAdapter -> VehicleCore -> RealMediaServiceAdapter
-> MediaService -> CAM0 -> RESULT -> canonical state
```

The target grade is `VOICE_INTENT_REAL_CAM0_PASS`. It does not include live
microphone input, live VAD callback, wake word, RKLLM, Recording, RTSP, CAM1, RKNN,
AMP or RT-Thread.

## Baseline and merge

- Branch/worktree: `agent/voice-intent-real-cam0` in
  `/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-voice-cam0`.
- First parent: real-CAM0 commit `914b5a5`.
- Merged parent: voice intent/Core commit `c50aa29`.
- Merge base: Vehicle Core foundation `8445677`.
- The merge keeps Core ACK/RESULT separation, canonical revisioned state,
  deadline/late-result fencing, request idempotency, typed CandidateAction and the
  RealMediaServiceAdapter.

## Implementation

- Add typed `CLOSE_CAMERA` now that the merged protocol contains exact
  `CAMERA_PREVIEW_START` and `CAMERA_PREVIEW_STOP` commands.
- Map only fixed typed actions: `OPEN_CAMERA -> CAMERA_PREVIEW_START`,
  `CLOSE_CAMERA -> CAMERA_PREVIEW_STOP`, and existing typed camera selections.
- Keep ASR text outside VehicleCommand parameters.
- Add `cockpit_voice_media_test`: FakeCamera on Host and an explicitly selected
  V4L2 backend on RK3576. Both variants use the same router/dispatcher/Core/real
  media adapter path.
- Add MediaService operation counters so duplicate FINAL behavior is measured at
  the service boundary rather than inferred from UI state.

## Validation

1. `bash scripts/dev/host_ci.sh` including the FakeCamera integration suite.
2. Host cases: open/close, negation, duplicate FINAL, Rear unavailable,
   PARTIAL/NO_MATCH, timeout and late success fencing.
3. Native RK3576 configure/build/CTest with `COCKPIT_ENABLE_V4L2_CAMERA=ON`.
4. Re-resolve CAM0 from the live sensor/media graph, then run one finite board
   suite as user `cat`. The tool must close CAM0, exit, and permit a reopen check.

## Permissions and recovery

Host work is L0. The user-authorized board stage is limited to the existing `cat`
account, one finite CAM0 owner, synthetic text input, project files under
`/home/cat/cockpit/`, and no ALSA/VAD/live microphone. No CAM1, sudo, packages,
boot/kernel/DT changes or service changes are allowed. On failure, stop only the
known test PID, retain bounded logs, verify `/dev/video*` ownership, and do not
retry an error storm.

## Result

The ordinary merge commit is `36863cf`. Its first combined Host CI passed 26/26
CTest and 6/6 Python tests. After the typed preview mapping and integration tool,
Host CI passes 27/27 CTest and 6/6 Python tests. RK3576 results remain pending at
this point and will be appended without rewriting failed or incomplete evidence.
