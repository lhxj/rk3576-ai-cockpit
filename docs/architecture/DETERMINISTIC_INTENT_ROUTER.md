# Deterministic intent routing (text and candidate only)

## Scope and boundary

`DeterministicIntentRouter` consumes a complete typed `AsrEventType::FINAL` with `SessionToken`, ASR sequence and a finite Unix-ms deadline. PARTIAL, ASR error, malformed UTF-8, empty or over-512-byte text cannot produce a candidate. `match()` is synchronous and side-effect free; `dispatch()` makes at most one `IVehicleCommandSink::submit_candidate` call. Router unit tests use an in-memory recording sink. A separate integration target connects the same router to `VehicleCore`, `MediaService` and real CAM0 without changing the pure matching boundary. Neither Sherpa nor VAD normalization or decoding was changed.

The maintained C++ rule table has ten supported intents, ten canonical phrases and 23 aliases. Rule IDs and mappings are in [INTENT_RULE_MATRIX.md](INTENT_RULE_MATRIX.md). Constructor validation rejects duplicate rule IDs and collisions after normalization; Host tests traverse every canonical phrase and alias. There is no keyword scoring, regex generalization, edit distance, pinyin correction, generated synonyms or LLM rewrite.

## Normalization and decisions

`normalize_intent_text()` checks UTF-8, trims and compresses whitespace, folds ASCII case, converts fullwidth ASCII and the ideographic space, maps selected Chinese terminal punctuation, and strips terminal sentence punctuation. A separate, fixed command form removes only one listed courtesy prefix (`请帮我`, `麻烦你`, `帮我`, `麻烦`, `请`) and one listed suffix (`一下`, `吧`). Negation remains intact. Explicit `不`, `别`, `勿`, `取消`, `禁止` anywhere in the command form causes `REJECTED_NEGATED` before matching; this deliberately favors false negatives. `停止` and `关闭` are not stripped as negation. Unknown complete texts are `NO_MATCH` (ordinary, not an error).

Authorization uses normalized **whole-string** exact or alias comparison. Two or three contiguous copies of the same complete rule phrase produce one `REPEATED_EXACT` match and one candidate; four or more copies are `NO_MATCH`. Substring search only detects mixed rule phrases and returns `REJECTED_AMBIGUOUS`; it never authorizes an action. A solitary “关闭摄像头” maps to typed `CLOSE_CAMERA`; an utterance containing both open and close phrases is rejected as ambiguous. `EXACT`, `ALIAS`, `REPEATED_EXACT` are rule classes, not ASR probability estimates.

## Typed candidate, time and session

`CandidateAction` retains the existing `ActionType`, `ActionSource`, and complete `SessionToken` (request/session/generation/boot epoch). It now carries a finite deadline, ASR event sequence, and a variant parameter: no parameter, `CameraId::{Front,Rear}`, or `bool` for the simulated LED/buzzer requests. `VoiceSessionController::submit_action` rejects an action/parameter shape mismatch. There is no free-form command string, shell payload or arbitrary JSON.

`match()` asks the controller to validate the current token and stage; cancelled and replaced generations map to `REJECTED_CANCELLED` and `REJECTED_STALE`. An expired deadline is `REJECTED_STALE`. `dispatch()` rechecks the deadline and controller before submission, transitions Recognizing to Understanding if needed, then calls the existing sink boundary once. The controller holds its lock during sink delivery so cancellation cannot pass an old action after `cancel()` returns. Sink callback must remain quick and non-reentrant. A sink failure is returned unchanged; no retry, LLM fallback or “operation succeeded” message occurs.

The route itself does not keep an unbounded seen-set. Duplicate FINAL dispatch prevention remains an owner dispatcher / sink / future `vehicle_core` idempotency responsibility keyed by request, session, epoch and event sequence. An action submission is **not** ACK or RESULT; those refer to later vehicle service lifecycle.

## Integration boundary

Current ASR/VAD callbacks are invoked under the `VoiceSessionController::deliver_event` lock, and the VAD processor marks its session Completed after FINAL. `VoiceIntentDispatcher` copies FINAL into a bounded queue and routes it on its owned worker after the callback releases the mutex; calling this router or a sink reentrantly inside the callback would deadlock. The real-CAM0 validation injected synthetic FINAL events and did not connect live VAD callbacks.

The integrated `VehicleCommandSinkAdapter` maps typed candidates through a fixed whitelist. It retains Core's bounded string parameter representation only inside the adapter, using fixed camera IDs and boolean literals. `OPEN_CAMERA` maps to `CAMERA_PREVIEW_START`, and `CLOSE_CAMERA` maps to `CAMERA_PREVIEW_STOP`; both have no parameter. Core supplies distinct ACK and terminal RESULT. See [VOICE_INTENT_REAL_CAM0.md](VOICE_INTENT_REAL_CAM0.md). The older [VOICE_INTENT_CORE_INTEGRATION.md](VOICE_INTENT_CORE_INTEGRATION.md) preserves the earlier Mock-only branch evidence.

`MATCH → CandidateAction`; `NO_MATCH` may later enter a language query path, but no LLM is linked here. Safety rejection must never fall through to LLM-generated automatic hardware action. Product logging should record rule ID and decision with bounded retention; it need not retain arbitrary unmatched speech text. The text-only test CLI prints its input fixture for debugging and never opens hardware.
