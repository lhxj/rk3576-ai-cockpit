# Cockpit UI and Vehicle Core integration

This document defines the Host/board-userland integration between the Qt shell and
the Vehicle Core foundation. The current service adapters are Mock. A closed control
path here does not validate Camera, Recording, RTSP, playback, Voice, RTOS, RPMsg or
sensor hardware.

## Dependency and ownership

```text
Qt page signal
  -> MainWindow
  -> IUiBackend
  -> VehicleCoreUiBackend
  -> IVehicleCoreClient
  -> VehicleCore
  -> MockMedia / MockVoice / MockRtos / MockSystem
  -> ACK + asynchronous RESULT
  -> canonical VehicleState + revision
  -> VehicleCoreUiBackend
  -> UiState
  -> queued Qt GUI-thread update
```

Qt widgets only know `IUiBackend`, `UiRequest`, `UiResult` and `UiState`. They do not
include or call `VehicleCore`, `StateStore`, command routing or Mock adapter types.
Vehicle Core remains a Qt-independent library. `CoreIntegrationRuntime` is the single
composition point for adapters, core, in-process client and backend.

The current client is `InProcessVehicleCoreClient`. It was selected to validate
control semantics without introducing sockets, reconnects and process supervision at
the same time. A future `IpcVehicleCoreClient` can replace it without changing pages.

## Backend selection

The executable keeps both modes:

```sh
cockpit_ui --backend mock
cockpit_ui --backend core --profile normal
```

`mock` is the default for isolated visual work. `core` composes Vehicle Core with
Mock service adapters. Test-only core profiles are `normal`, `media-failure`,
`media-timeout` and `rtos-offline`; they are not product configuration.

## Action and command mapping

| UI action | Vehicle command | Parameters |
|---|---|---|
| Front / Rear | `CAMERA_SELECT` | `camera=front/rear` |
| Snapshot | `CAMERA_SNAPSHOT` | none |
| Start / Stop Recording | `RECORDING_START` / `RECORDING_STOP` | none |
| Start / Stop RTSP | `RTSP_START` / `RTSP_STOP` | none |
| Play / Pause / Stop | `MEDIA_PLAY` / `MEDIA_PAUSE` / `MEDIA_STOP` | optional media item only for Play |
| Previous / Next | `MEDIA_PREVIOUS` / `MEDIA_NEXT` | none |
| Voice Start / Cancel | `VOICE_SESSION_START` / `VOICE_SESSION_CANCEL` | backend session id |
| LED / Buzzer | `SIM_LED_SET` / `SIM_BUZZER_SET` | `enabled=true/false` |

`MainWindow` does not create protocol fields. `VehicleCoreUiBackend` uses an atomic,
monotonic request counter starting at one, obtains `boot_epoch` from
`IVehicleCoreClient`, owns the voice session id and applies one centralized 2000 ms
deadline policy using the injected Vehicle Core clock. Tests inject `FakeClock`.

## ACK, RESULT and pending state

`submit()` performs bounded, non-waiting submission. A valid Vehicle Core ACK becomes
an `Accepted` UI result with `terminal=false` and text `ACK accepted; awaiting RESULT`.
For Recording, RTSP and Voice, the backend adds a local pending overlay such as
`Starting`; it does not write that overlay into Vehicle Core or mark the operation
successful.

A joinable backend worker observes the shared RESULT futures without blocking the GUI
thread. The terminal result is mapped to `Ok`, `Unavailable`, `Rejected`, `Error` or
`Timeout`, then delivered through the result callback. Service truth remains the
canonical snapshot. A failed Recording command therefore ends in the core's error
state, and a late success after timeout is fenced by Vehicle Core and cannot update
the UI revision or Recording state. Repeated clicks on Recording, RTSP and Voice are
disabled while their local request is pending.

## Snapshot, state mapping and revision

Backend startup performs:

```text
get full snapshot
  -> map all supported fields
  -> subscribe to state changes
  -> get one more full snapshot to close the race
```

`RevisionedStateProjector` accepts the first full snapshot and then only revisions
strictly greater than `last_revision`. Equal and older events are ignored. Mapping is
a Qt-independent pure function and covers Wi-Fi, both cameras, selected camera,
Recording, RTSP, Media, Audio, Voice, Vision, LLM, RTOS, Sensor, simulated controls,
all five service-health entries, source and revision.

Source is preserved independently from condition. An online Mock adapter is displayed
with source `MOCK`; RTOS business state remains Offline until a real state update, even
if the Host Mock adapter is available. Vision and LLM remain Offline/Not Ready. A rear
camera command failure never changes selected camera away from Front.

## Qt thread and lifetime boundary

Vehicle Core state callbacks and backend RESULT callbacks may run on non-Qt threads.
`MainWindow` checks Qt affinity and uses
`QMetaObject::invokeMethod(..., Qt::QueuedConnection)` before touching widgets. It
never waits on a future, sleeps or polls the core.

There are no detached threads. The backend has a fixed pending capacity of 64 and one
joinable RESULT worker. A shared callback gate prevents a retained core subscription
from dereferencing a destroyed backend. Destruction clears Qt-facing callbacks, stops
and joins the backend worker, then `CoreIntegrationRuntime` stops and joins Vehicle
Core before destroying the client and adapters. The lifecycle is covered by 20
create/start/send/stop/destroy cycles.

## Future process boundary

This stage intentionally has no UDS, ZeroMQ, TCP or WebSocket. Process separation must
implement `IVehicleCoreClient`, preserve snapshot-first synchronization, revisions,
ACK/RESULT separation, boot epoch and deadlines, and add explicit disconnect state plus
a fresh full snapshot after reconnect. Exponential retry and daemon supervision belong
to that later task.
