# 功能追踪矩阵

> `当前状态` 只做架构级初始化，实时事实仍以 `docs/STATUS.md` 为准。
> `目标负责模块` 表示未来所有权，不表示模块已经实现。F01/F02等历史硬件
> 通过项也不证明 `cockpit_ui` 已运行。

2026-10-01 IMX6ULL RAR 已完成静态参考审查：`SHADOW_BUILD`、源码
`INCOMPLETE`、`UI_REFERENCE_ONLY`、迁移`REIMPLEMENT`、
`LICENSE_UNVERIFIED`。参考工程中的菜单、音乐、视频、传感器入口不是当前
RK3576功能实现，不能据此提升下表状态。

LLM_Voice_Flow 已审查为 `REFERENCE_ONLY`；新建的 Voice/AI Host 接口与Mock
仅验证协议和生命周期，F14-F20 的真实语音/模型功能状态保持不变。

| ID | 功能 | 目标负责模块 | 关键硬件/平台 | 验证方式 | 当前状态 |
|---|---|---|---|---|---|
| F01 | HDMI显示 | cockpit_ui / Linux graphics | 4.3" HDMI | 实屏显示 | BOARD_TESTED_HISTORICAL |
| F02 | USB触控 | cockpit_ui / input | Waveshare HID | 实际触控 | BOARD_TESTED_HISTORICAL |
| F03 | Camera0采流 | media_srv | OV8858 CAM0 | V4L2 1632x1224@30 NV12 | BOARD_TESTED_HISTORICAL |
| F04 | Camera0 Qt预览 | cockpit_ui + media_srv | CAM0 | Qt实时显示 | NOT_IMPLEMENTED |
| F05 | Camera1单独采流 | media_srv | OV8858 CAM1 | 已知好线缆采流 | BOARD_TESTED_HISTORICAL |
| F06 | 双摄并发 | media_srv | CAM0+CAM1 | 同时稳定stream | BLOCKED |
| F07 | 前后摄稳定映射 | media_srv | media graph | reboot后身份一致 | UNVERIFIED |
| F08 | 抓拍 | media_srv | camera/storage | UI命令→文件→RESULT | UNVERIFIED |
| F09 | 录像 | media_srv | MPP/storage | record/playback | UNVERIFIED |
| F10 | RTSP | media_srv | MPP+Wi-Fi | LAN client观看 | UNVERIFIED |
| F11 | 本地媒体播放 | media_srv/audio_srv/cockpit_ui | display/audio | 本地文件播放 | NOT_IMPLEMENTED |
| F12 | 板载麦录音 | audio_srv | codec/mic | ALSA record | BOARD_TESTED_HISTORICAL |
| F13 | 耳机播放 | audio_srv | 3.5mm | ALSA playback | BOARD_TESTED_HISTORICAL |
| F14 | Voice PCM pipeline | audio_srv/voice_srv | ALSA | 连续音频流 | UNVERIFIED |
| F15 | ASR | voice_srv | CPU/NPU dependency | board transcription | UNVERIFIED |
| F16 | VAD/wake | voice_srv | mic | board runtime | UNVERIFIED |
| F17 | TTS | voice_srv/audio_srv | model/audio | board speech output | UNVERIFIED |
| F18 | RKLLM | infer_srv | RK3576 runtime | board inference | UNVERIFIED |
| F19 | 结构化语音命令 | voice_srv/vehicle_core | software | command E2E | UNVERIFIED |
| F20 | RKNN视觉模型 | infer_srv | NPU | board inference | UNVERIFIED |
| F21 | AI overlay | infer_srv/cockpit_ui | NPU+display | preview overlay | NOT_IMPLEMENTED |
| F22 | Wi-Fi | system/monitor | RTL8822CE | network/SSH | BOARD_TESTED_HISTORICAL |
| F23 | SSH密钥开发闭环 | dev tooling | Wi-Fi | ssh lubancat | BOARD_TESTED_HISTORICAL |
| F24 | RT-Thread启动 | RTOS/AMP | RK3576 | serial/runtime evidence | UNVERIFIED |
| F25 | RPMsg echo | rpmsg_srv/RTOS | mailbox/shared memory | round-trip test | UNVERIFIED |
| F26 | HELLO/heartbeat | rpmsg_srv/RTOS | RPMsg | link state | UNVERIFIED |
| F27 | MPU6050采样 | RTOS sensor_task | MPU6050/I2C | board sensor values | UNVERIFIED |
| F28 | SENSOR_REPORT | RTOS/rpmsg_srv | RPMsg | RTOS→Linux→UI | UNVERIFIED |
| F29 | 模拟LED/蜂鸣器控制 | vehicle_core/RTOS | software simulation | command/result | UNVERIFIED |
| F30 | Monitor页面 | monitor/cockpit_ui | system APIs | live metrics | NOT_IMPLEMENTED |
| F31 | CPU/RAM/温度/磁盘 | monitor | Linux | runtime values | UNVERIFIED |
| F32 | Camera FPS/drop stats | media_srv | V4L2 | measured counters | UNVERIFIED |
| F33 | RTOS Offline降级 | vehicle_core/ui | AMP | disconnect scenario | UNVERIFIED |
| F34 | Rear Offline降级 | media/ui | camera | startup without Rear | UNVERIFIED |
| F35 | Low storage处理 | media_srv | eMMC | fault injection | UNVERIFIED |
| F36 | Host CMake scaffold | build/tests | WSL | CTest/Python | HOST_TESTED |
| F37 | cockpit_ui单shell与页面路由 | cockpit_ui | Qt/graphics | Host导航测试+板端全屏触控 | NOT_IMPLEMENTED |
| F38 | Vehicle / Sensor页面 | cockpit_ui/vehicle_core | RTOS/RPMsg | SENSOR_REPORT到UI | NOT_IMPLEMENTED |
| F39 | Settings页面 | cockpit_ui | config/state API | 配置显示与错误处理 | NOT_IMPLEMENTED |
| F40 | vehicle_core控制面foundation | vehicle_core | Host C++/Mock | lifecycle/state/timeout/idempotency tests | HOST_TESTED_FOUNDATION |

## 状态含义

- `HOST_TESTED`：只在开发主机验证。
- `BOARD_TESTED_HISTORICAL`：已有历史实板证据，但不表示当前运行时始终在线。
- `NOT_IMPLEMENTED`：已有目标定义，但主项目尚无对应业务实现。
- `UNVERIFIED`：目标功能尚无足够验证证据。
- `BLOCKED`：存在已知外部阻塞，解除后再测。

功能进入 DONE 时应同时有：实现、测试、必要实板证据和文档更新。
