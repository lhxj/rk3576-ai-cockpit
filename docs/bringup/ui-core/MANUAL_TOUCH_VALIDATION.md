# Manual touch validation

Date: 2026-10-01

## Existing foundation evidence

The user previously confirmed the Qt foundation binary on the 800x480 HDMI touch
panel: Home → Camera → Media → Vehicle/Sensor → AI → Monitor → Settings → Home,
with no obvious clipping, crowded buttons or unreadable text; edge controls and return
navigation responded to physical touch. This evidence applies to the foundation build,
not automatically to the new UI/Core integration binary.

## Integration binary status

`BOARD_TOUCH_TESTED_MOCK_INTEGRATION` (`USER_CONFIRMED`, 2026-10-01).

The user physically tested the integration binary on the 800x480 HDMI touch panel.
The following checks passed:

1. `normal`: full page navigation, touch targets and layout were reported as normal;
2. `normal`: Start/Stop Recording showed the expected terminal states;
3. `normal`: Rear reported unavailable and Front remained selected;
4. `normal`: LED/Buzzer remained explicitly marked `SIMULATED`;
5. `rtos-offline`: the control request visibly returned `UNAVAILABLE` rather than a
   simulated success;
6. `media-timeout`: Start Recording visibly terminated as `Error` rather than
   `Recording`;
7. each bounded profile was run alone and the prior process exited naturally before
   the next profile was launched.

This closes `UI-CORE-08` at the Mock integration boundary and supports
`UI_VEHICLE_CORE_INTEGRATION_PASS`. It does not validate a real media service,
RPMsg, RT-Thread, GPIO, camera, audio or AI backend.
