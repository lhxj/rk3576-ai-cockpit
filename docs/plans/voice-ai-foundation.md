# Voice/AI Foundation ExecPlan

## Goal

Record the completed LLM_Voice_Flow audit as `REFERENCE_ONLY` and build our own C++17 protocol, IPC, audio, voice and inference interfaces with Host tests.

## Evidence and unknowns

- The fixed source audit at `be82e87cc334ae6e222f83f7555531d1ddebaa8b` concludes `REFERENCE_ONLY`; the audit reports are evidence snapshots and remain unchanged.
- The current repository contains only placeholder READMEs in the five target directories. Existing CMake/CTest and `scripts/dev/host_ci.sh` are Host only.
- Real ASR/TTS/RKLLM/RKNN models, audio devices, transport choice and target SDK compatibility remain outside this task.

## Scope and permission

L0 repository edits and Host tests only. Edit maintained reference/task/architecture/status docs, root CMake, five target modules and `tests/unit/`. Do not alter historical audit reports, reference source, board, system packages or existing unrelated untracked files.

## Implementation

1. Update maintained reference decision and roadmap without claiming product model functionality.
2. Implement explicit bounded control-message codec and stateful request freshness checks.
3. Implement bounded closeable queue and in-memory transport with owned worker lifetime.
4. Add audio mock ownership boundary, voice session and candidate-action boundary, and cancellable mock language backend.
5. Add Host tests for malformed messages, queue/transport shutdown, stale sessions, cancel and service lifecycle.
6. Document the contracts and run `bash scripts/dev/host_ci.sh`.

## Validation and recovery

Expected: Host configure/build/CTest/Python checks exit 0 without Voice/AI third-party libraries. If build or test fails, fix only files in this task and rerun the failed gate; do not install global dependencies. All workers must join normally. No hardware resources are reserved.

## Result

Implemented the maintained reference decision, C++17 foundation libraries/service abstractions,
and five new CTest executables. `bash scripts/dev/host_ci.sh` exited 0 on WSL Ubuntu 22.04:
CTest 7/7 and Python unittest 6/6 passed, including explicit shutdown and stale-session
cases. CTest also passed 20 repeated runs per test for lifecycle race checking.
First run found a CMake Threads target scope error; a later run found an audio
stop/read race in the Mock status. Both were corrected before this result. No board,
real audio device, model, reference source or system package was touched.

Next: close upstream/model licenses and dependency versions, then implement the
VOICE-05 file-based ASR backend behind `IAsrBackend`. TTS and RKLLM remain separate
subsequent backend tasks; transport choice and real ALSA remain open.
