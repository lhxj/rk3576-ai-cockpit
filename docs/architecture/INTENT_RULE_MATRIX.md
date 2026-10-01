# Vehicle intent rule matrix v1

The table describes candidate generation only. It does not prove execution. Canonical plus every alias is tested by the Host rule-table test. `CameraId` and enabled flags are typed; LED and buzzer target the **simulated** control semantics of the separate `vehicle_core` branch.

| Rule ID | Canonical / aliases | Intent | CandidateAction | Typed parameter | Future VehicleCommand | Result |
|---|---|---|---|---|---|---|
| CAMERA_OPEN_001 | 打开摄像头；开启摄像头；打开相机；开启相机 | CAMERA_OPEN | OPEN_CAMERA | none | UNSUPPORTED_ACTION（没有预览启动命令） | MATCH candidate only |
| CAMERA_FRONT_001 | 切换前摄；切到前摄；切换前摄像头；切到前摄像头 | CAMERA_FRONT | SELECT_CAMERA | CameraId::Front | CAMERA_SELECT front | MATCH |
| CAMERA_REAR_001 | 切换后摄；切到后摄；切换后摄像头；切到后摄像头 | CAMERA_REAR | SELECT_CAMERA | CameraId::Rear | CAMERA_SELECT rear | MATCH |
| RECORDING_START_001 | 开始录像；开始录制；开始视频录制 | RECORDING_START | START_RECORDING | none | RECORDING_START | MATCH |
| RECORDING_STOP_001 | 停止录像；结束录像；停止录制；结束录制 | RECORDING_STOP | STOP_RECORDING | none | RECORDING_STOP | MATCH |
| BUZZER_ON_001 | 打开蜂鸣器；开启蜂鸣器 | BUZZER_ON | SET_BUZZER | true | SIM_BUZZER_SET enabled | MATCH |
| BUZZER_OFF_001 | 关闭蜂鸣器；关掉蜂鸣器 | BUZZER_OFF | SET_BUZZER | false | SIM_BUZZER_SET disabled | MATCH |
| LED_ON_001 | 打开LED；开启LED；打开灯 | LED_ON | SET_LED | true | SIM_LED_SET enabled | MATCH |
| LED_OFF_001 | 关闭LED；关掉LED；关灯 | LED_OFF | SET_LED | false | SIM_LED_SET disabled | MATCH |

`关闭摄像头` and its close aliases are `NO_MATCH`: no corresponding preview-stop command exists in the current `vehicle_core` contract. A mixed open/close utterance is rejected as ambiguous. No rule permits restart, shutdown, shell, file deletion, firmware, configuration or memory operations.
