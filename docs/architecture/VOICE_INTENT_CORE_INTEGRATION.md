# Voice Intent → Vehicle Core Host integration

This branch merges Intent Router `97dbc29` and Vehicle Core foundation `8445677`
with ordinary Git history. Validation uses synthetic ASR events and Mock service
adapters only. It does not connect VAD runtime, hardware services, UI, or a board.

## Ownership and handoff

```text
VoiceSessionController::deliver_event(ASR_FINAL)
  → VoiceIntentDispatcher::enqueue_event (copy FINAL, try_push, return)
  → bounded queue (default 32)
  → owned, joinable dispatcher worker
  → DeterministicIntentRouter::match / dispatch
  → typed CandidateAction
  → IVehicleCommandSink / VehicleCommandSinkAdapter
  → VehicleCommand(source=VOICE)
  → VehicleCore → MockMediaAdapter / MockRtosAdapter
  → ACK, terminal RESULT, canonical VehicleState
```

`deliver_event` invokes its callback while holding the Controller mutex. The
callback only copies a FINAL into the queue; the worker performs matching and
Core submission after that callback releases the mutex. PARTIAL and ERROR are
ignored by this dispatcher. `start()` owns one worker, `stop()` closes the queue
and joins it, and destruction calls `stop()`. Overflow returns
`INTENT_DISPATCH_QUEUE_OVERFLOW`; each queued text/status detail is capped at
512 bytes and the queue cannot grow without bound. This is
an integration component, not a live VAD callback hookup.

## Strict action mapping

| CandidateAction | Typed parameter | VehicleCommand | Core parameter | Result |
|---|---|---|---|---|
| `OPEN_CAMERA` | none | — | — | `UNSUPPORTED_ACTION`; Core has no preview-start command |
| `SELECT_CAMERA` | `CameraId::Front` | `CAMERA_SELECT` | fixed `camera=front` | Mock Media |
| `SELECT_CAMERA` | `CameraId::Rear` | `CAMERA_SELECT` | fixed `camera=rear` | Mock Media; rear availability still checked |
| `START_RECORDING` | none | `RECORDING_START` | none | Mock Media |
| `STOP_RECORDING` | none | `RECORDING_STOP` | none | Mock Media |
| `SET_BUZZER` | bool | `SIM_BUZZER_SET` | fixed `enabled=true/false` | Mock RTOS, simulated |
| `SET_LED` | bool | `SIM_LED_SET` | fixed `enabled=true/false` | Mock RTOS, simulated |

`关闭摄像头` is `NO_MATCH` at the router: no preview-stop action exists. The
Core foundation represents validated command parameters as bounded strings;
only the adapter manufactures the literal strings shown above from typed
values. ASR text is never copied into a Core parameter. Invalid variant,
unknown action, or `LLM_CANDIDATE` is rejected. There is no arbitrary string
execution entry point in this integration.

## Identity, cancellation and deadline

Each accepted candidate carries the current `SessionToken` (`session_id`,
`generation`, `request_id`, `boot_epoch`), `asr_sequence` and finite deadline.
The router validates that the Controller still owns the token and that the
deadline has not passed. The sink requires its separately activated token to
match exactly, rejects cancellation and mismatched deadline/typed parameter,
and preserves identity on `VehicleCommand`. The Vehicle command wire encodes
`voice_generation` and `asr_sequence` explicitly. Core rejects a mismatched
`boot_epoch`; the adapter never rewrites it. The Controller serializes cancel
against candidate submission under its mutex.

The dispatcher keeps a bounded recent-FINAL cache (default 128) keyed by boot
epoch, session, generation, request ID and ASR sequence. A repeated FINAL does
not generate another request ID or submit again. Core's independent bounded
request-ID/fingerprint cache is the second guard; it returns the original
future on an exact replay without a second service invocation. Both caches are
in-memory windows, not durable exactly-once execution across restarts.

## ACK, RESULT and canonical state

ACK means validation succeeded and the bounded Core queue accepted the
command. It is not an operation success. For `开始录像`, Mock Media can hold
completion: ACK is emitted while canonical Recording is `STARTING`, not
`RECORDING`. On Mock success, terminal RESULT succeeds and state becomes
`RECORDING`. Mock failure leads to ERROR; a deadline timeout is terminal and
later Mock success cannot change the state or revision. Mock RTOS results set
`simulated=true`; they are no evidence of hardware control.

## Future boundary

Only a later task may connect live VAD/ASR callbacks or real Media/RTOS
adapters. Live integration must call `enqueue_event` from a valid FINAL
callback, explicitly activate/cancel each session in the sink, and preserve the
same bounded queue, identity and deadline contract. It must not route LLM
output straight to a service or hardware device.
