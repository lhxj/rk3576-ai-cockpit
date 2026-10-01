# apps/vehicle_core

Host-only control-plane foundation. It validates structured commands, emits accepted ACKs,
routes to bounded Mock adapters, commits canonical state only from terminal outcomes, and
publishes revisioned snapshots. It never opens V4L2, ALSA, RPMsg, GPIO, model, or Qt APIs.

Current status: `VEHICLE_CORE_HOST_FOUNDATION`. Mock service health and Mock state sources are
not evidence that camera, recording, voice, RTOS, sensors, or any target-board service works.
See `docs/architecture/VEHICLE_CORE.md`.
