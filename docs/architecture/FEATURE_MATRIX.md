# 功能追踪矩阵

> `当前状态` 只做架构级初始化，实时事实仍以 `docs/STATUS.md` 为准。
> `目标负责模块` 表示未来所有权，不表示模块已经实现。F01/F02等历史硬件
> 通过项也不证明 `cockpit_ui` 已运行。

2026-10-01 IMX6ULL RAR 已完成静态参考审查：`SHADOW_BUILD`、源码
`INCOMPLETE`、`UI_REFERENCE_ONLY`、迁移`REIMPLEMENT`、
`LICENSE_UNVERIFIED`。参考工程中的菜单、音乐、视频、传感器入口不是当前
RK3576功能实现，不能据此提升下表状态。

LLM_Voice_Flow 已审查为 `REFERENCE_ONLY`。后续自有实现的文件ASR、实时麦克风ASR、
VAD文件fixture和VoiceRuntime已有各自证据；每一项仍按实际边界标记，不能由Runtime
启停或synthetic FINAL推导出真人实时语音控制、wake、TTS、RKLLM或RKNN完成。

| ID | 功能 | 目标负责模块 | 关键硬件/平台 | 验证方式 | 当前状态 |
|---|---|---|---|---|---|
| F01 | HDMI显示 | cockpit_ui / Linux graphics | 4.3" HDMI | 实屏显示 | BOARD_TESTED_HISTORICAL |
| F02 | USB触控 | cockpit_ui / input | Waveshare HID | 实际触控 | BOARD_TESTED_HISTORICAL |
| F03 | Camera0采流 | media_srv | OV8858 CAM0 | V4L2 1632x1224@30 NV12 | BOARD_TESTED_PROJECT_PATH |
| F04 | Camera0 Qt预览 | cockpit_ui + media_srv | CAM0 | Qt实时显示 | BOARD_TOUCH_TESTED_CAM0_INTEGRATION |
| F05 | Camera1单独采流 | media_srv | OV8858 CAM1 | 已知好线缆采流 | BOARD_TESTED_HISTORICAL |
| F06 | 双摄并发 | media_srv | CAM0+CAM1 | 同时稳定stream | BLOCKED |
| F07 | 前后摄稳定映射 | media_srv | media graph | reboot后身份一致 | UNVERIFIED |
| F08 | 抓拍 | media_srv | camera/storage | UI命令→文件→RESULT | BOARD_TESTED_PPM |
| F09 | CAM0录像 | media_srv | MPP/storage | Annex-B文件、20轮、300秒并发 | BOARD_TESTED_CAM0_RECORDING |
| F10 | RTSP | media_srv | MPP+Wi-Fi | 单客户端H.264 RTP、重连、300秒并发 | BOARD_TESTED_CAM0_RTSP |
| F11 | 本地媒体播放 | media_srv/audio_srv/cockpit_ui | display/audio | 本地文件播放 | NOT_IMPLEMENTED |
| F12 | 板载麦录音 | audio_srv | codec/mic | ALSA record | BOARD_TESTED_HISTORICAL |
| F13 | 耳机播放 | audio_srv | 3.5mm | ALSA playback | BOARD_TESTED_HISTORICAL |
| F14 | Voice PCM pipeline | audio_srv/voice_srv | ALSA | 连续音频流 | BOARD_TESTED_BOUNDED_RUNTIME |
| F15 | ASR | voice_srv | CPU/NPU dependency | board transcription | BOARD_FILE_AND_LIVE_MIC_ASR_PASS |
| F16 | VAD/wake | voice_srv | mic | board runtime | PARTIAL：VAD_FILE_PIPELINE_PASS；WAKE_UNVERIFIED |
| F17 | TTS | voice_srv/audio_srv | model/audio | board speech output | NOT_IMPLEMENTED |
| F18 | RKLLM | infer_srv | RK3576 runtime | board inference | NOT_IMPLEMENTED |
| F19 | 结构化语音命令 | voice_srv/vehicle_core | software + CAM0/MPP | runtime synthetic ASR_FINAL→Core→real media | BOARD_TESTED_SYNTHETIC_ASR_FINAL_CAM0_MPP |
| F20 | RKNN视觉模型 | infer_srv | NPU | fixed image + CAM0 + 300秒 + media并发 | BOARD_TESTED_CAM0_RKNN |
| F21 | AI overlay | infer_srv/cockpit_ui | NPU+display | preview overlay | NOT_IMPLEMENTED |
| F22 | Wi-Fi | system/monitor | RTL8822CE | network/SSH | BOARD_TESTED_HISTORICAL |
| F23 | SSH密钥开发闭环 | dev tooling | Wi-Fi | ssh lubancat | BOARD_TESTED_HISTORICAL |
| F24 | RT-Thread启动 | RTOS/AMP | RK3576 BUS M0 | paired C串口/runtime；冷恢复 | BOARD_PASS_MINIMAL_FROZEN |
| F25 | RPMsg最小echo | Linux echo KO/RTOS | mailbox/shared memory | 一次性HELLO_ACK/PONG；非用户态业务 | BOARD_PASS_MINIMAL_FROZEN |
| F26 | 业务HELLO/heartbeat | rpmsg_srv/RTOS | RPMsg | 持续业务link state；最小一次握手不覆盖 | NOT_IMPLEMENTED |
| F27 | MPU6050采样 | RTOS sensor_task | MPU6050/I2C | board sensor values | NOT_IMPLEMENTED |
| F28 | SENSOR_REPORT | RTOS/rpmsg_srv | RPMsg | RTOS→Linux→UI | NOT_IMPLEMENTED |
| F29 | 模拟LED/蜂鸣器控制 | vehicle_core/RTOS | software simulation | UI→Core→Mock RESULT | HOST_TESTED_SIMULATED |
| F30 | Monitor页面 | monitor/cockpit_ui | system APIs | live metrics | PARTIAL_UI_SKELETON |
| F31 | CPU/RAM/温度/磁盘 | monitor | Linux | runtime values | UNVERIFIED |
| F32 | Camera FPS/drop stats | media_srv | V4L2 | measured counters | BOARD_TESTED_BOUNDED_300S |
| F33 | RTOS Offline降级 | vehicle_core/ui | AMP | Mock service offline scenario | HOST_TESTED_MOCK |
| F34 | Rear Offline降级 | media/ui | camera | canonical Rear unavailable + failed selection | HOST_TESTED_MOCK |
| F35 | Low storage处理 | media_srv | eMMC | fault injection | UNVERIFIED |
| F36 | Host CMake scaffold | build/tests | WSL | CTest/Python | HOST_TESTED |
| F37 | cockpit_ui单shell与页面路由 | cockpit_ui | Qt/graphics | Host导航测试+板端全屏触控 | BOARD_TOUCH_TESTED_UI_FOUNDATION |
| F38 | Vehicle / Sensor页面 | cockpit_ui/vehicle_core | RTOS/RPMsg | UI→Core Mock控制；真实SENSOR_REPORT另验 | HOST_TESTED_MOCK_INTEGRATION |
| F39 | Settings页面 | cockpit_ui | config/state API | 配置显示与错误处理 | PARTIAL_UI_SKELETON |
| F40 | vehicle_core控制面foundation | vehicle_core | Host C++/Mock | lifecycle/state/timeout/idempotency tests | HOST_TESTED_FOUNDATION |
| F41 | cockpit_ui统一状态模型与Backend | cockpit_ui | Host C++17 | Mock backend + canonical state mapping CTest | HOST_TESTED_MOCK_INTEGRATION |
| F42 | Camera/Media/AI页面骨架 | cockpit_ui | Qt Widgets | Qt构建+页面导航；业务另行验收 | BOARD_BUILD_TESTED_UI_SKELETON |
| F43 | cockpit_ui ↔ vehicle_core控制闭环 | cockpit_ui/vehicle_core | In-process client + Mock adapters | ACK/RESULT/revision/timeout/late-result/Qt offscreen | HOST_TESTED_MOCK_INTEGRATION |
| F44 | integration build板端触控闭环 | cockpit_ui/vehicle_core | RK3576 Qt/X11 | 800x480触控+Mock profiles | BOARD_TOUCH_TESTED_MOCK_INTEGRATION |
| F45 | CAM0真实控制闭环 | cockpit_ui/vehicle_core/media_srv | OV8858 CAM0 | ACK/RESULT/canonical state + project backend | BOARD_TESTED_CORE_T5 |
| F46 | CAM0真实UI数据链 | media_srv/cockpit_ui | V4L2 + Qt/X11 | owned frame→bounded mailbox→worker conversion→Qt | BOARD_TOUCH_TESTED_CAM0_INTEGRATION |
| F47 | RKNN结果到AI页面 | infer_srv/cockpit_ui | NPU + Qt/X11 | current-epoch result→queued UI state；真实X11启动 | BOARD_RUNTIME_TESTED_RKNN_UI |

## 状态含义

- `HOST_TESTED`：只在开发主机验证。
- `HOST_TESTED_BOARD_T0_BLOCKED`：Host协议、生命周期和sanitizer通过，但尚未进入
  实板构建/客户端验证；当前阻塞原因必须在bring-up记录中列明。
- `MOCK_TESTED`：Host只验证Mock/契约行为，不代表真实服务或硬件。
- `HOST_TESTED_MOCK_INTEGRATION`：Host闭合UI/Core/Mock adapter控制链，不代表真实业务服务。
- `HOST_TESTED_SIMULATED`：Host验证显式软件模拟RESULT，不代表GPIO或RTOS硬件。
- `PARTIAL_UI_SKELETON`：页面源代码存在，真实数据链与业务验收未完成。
- `PARTIAL_QT_BUILD_SKIPPED`：Qt源代码存在，但当前环境缺少Qt开发包，目标未编译。
- `BOARD_BUILD_TESTED_UI_SKELETON`：页面已在板端编译并通过启动测试，真实业务仍未接入。
- `BOARD_STARTUP_TESTED`：目标程序进入实际图形会话并退出正常；不等于视觉或触摸验收。
- `BOARD_TOUCH_TESTED_UI_FOUNDATION`：用户在foundation二进制上确认布局与触摸导航；不自动覆盖后续integration二进制。
- `BOARD_BUILD_AND_X11_STARTUP_TESTED`：AArch64构建/测试和实际X11启动通过；尚无该二进制的人工触摸证据。
- `BOARD_TOUCH_TESTED_MOCK_INTEGRATION`：用户在实体触摸屏验证Qt→Vehicle Core→Mock adapter的正常、拒绝和超时可视路径；不代表真实服务或硬件通过。
- `BOARD_TESTED_HISTORICAL`：已有历史实板证据，但不表示当前运行时始终在线。
- `BOARD_TESTED_PROJECT_PATH`：当前项目backend在记录的节点/格式完成受控实板采集。
- `BOARD_TOUCH_TESTED_CAM0_INTEGRATION`：当前真实CAM0 Qt二进制完成X11运行、
  预览计数及用户实体屏视觉/触摸验收；不包含Recording、RTSP或CAM1。
- `BOARD_TESTED_PPM`：当前项目从真实CAM0 owned frame生成并检查了PPM；不代表录像/编码。
- `BOARD_TESTED_SHORT_RUN`：真实计数器在短时运行中验证；不代表长期稳定性。
- `BOARD_TESTED_BOUNDED_300S`：真实链完成五分钟有界运行和资源采样；不代表长期稳定性。
- `BOARD_TESTED_CORE_T5`：真实Media adapter与CAM0完成ACK/RESULT/canonical闭环；不自动证明Qt触摸。
- `BOARD_TESTED_SYNTHETIC_ASR_FINAL_CAM0`：合成FINAL文本经确定性路由、Core和真实MediaService控制CAM0通过；不代表实时麦克风、VAD或语音准确率通过。
- `BOARD_TESTED_BOUNDED_RUNTIME`：真实ALSA/VAD/ASR对象在有界运行中完成启停和资源释放；
  没有真人FINAL时不等于VAD live或实时语音控制通过。
- `BOARD_FILE_AND_LIVE_MIC_ASR_PASS`：分别有文件输入和固定时长真人麦克风ASR历史证据；
  不表示VAD自动结束后的实时命令闭环通过。
- `BOARD_TESTED_SYNTHETIC_ASR_FINAL_CAM0_MPP`：同一VoiceRuntime application中的明确
  synthetic FINAL经Core控制真实CAM0开关和MPP录像；不属于实时语音控制证据。
- `BOARD_TESTED_CAM0_RECORDING`：真实CAM0通过项目MediaService和MPP生成并关闭
  Annex-B H.264，完成20轮启停与五分钟Preview并发；不代表MP4、RTSP、CAM1、
  长期录像或实时语音控制通过。
- `BOARD_TESTED_CAM0_RTSP`：真实CAM0、MPP、RTSP/RTP完成客户端识别/解码、
  重连、五分钟Preview及Recording并发和20轮启停；范围限单客户端UDP unicast，
  不代表CAM1、音频、Internet、多客户端或长期streaming。
- `BOARD_TESTED_CAM0_RKNN`：官方匹配模型通过固定图与真实CAM0 NPU推理，完成五分钟
  稳定性及Preview/Recording/RTSP并发；不代表检测、跟踪、CAM1、RGA或第二模型。
- `BOARD_RUNTIME_TESTED_RKNN_UI`：真实RKNN runtime通过现有Qt/X11 shell更新AI页状态模型；
  没有新的人工触摸/视觉验收或overlay证据。
- `NOT_IMPLEMENTED`：已有目标定义，但主项目尚无对应业务实现。
- `UNVERIFIED`：目标功能尚无足够验证证据。
- `BLOCKED`：存在已知外部阻塞，解除后再测。

功能进入 DONE 时应同时有：实现、测试、必要实板证据和文档更新。

2026-10-04：F24/F25来自冻结AMP tip实板历史，不代表默认启动已运行M0、长期echo或整机同载通过。
完整系统共存另见[系统集成记录](../bringup/system-integration/BOARD_COEXISTENCE_RESULT.md)。
