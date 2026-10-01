# Manual touch validation

Date: 2026-10-01

## Existing foundation evidence

The user previously confirmed the Qt foundation binary on the 800x480 HDMI touch
panel: Home → Camera → Media → Vehicle/Sensor → AI → Monitor → Settings → Home,
with no obvious clipping, crowded buttons or unreadable text; edge controls and return
navigation responded to physical touch. This evidence applies to the foundation build,
not automatically to the new UI/Core integration binary.

## Integration binary status

`PENDING_USER_CONFIRMATION`.

The integration binary has passed target build, target CTest, bounded windowed profile
startup and bounded full-screen startup. The following physical checks still require
the user at the panel:

1. repeat the full page-navigation route and confirm no new status text causes clipping,
   overlap or layout movement;
2. `normal`: Start Recording reaches Recording only after pending, Stop returns Stopped;
3. `normal`: Rear reports unavailable and Front remains selected;
4. `normal`: LED/Buzzer terminal text says `SIMULATED`;
5. `rtos-offline`: Buzzer reports unavailable and remains off;
6. `media-timeout`: Start Recording reaches Starting then Timeout/Error without hanging;
7. exit naturally and confirm no residual process.

Until these checks are confirmed, `UI-CORE-08` is
`BOARD_BUILD_AND_X11_STARTUP_TESTED`, and the final project level remains below
`UI_VEHICLE_CORE_INTEGRATION_PASS`.
