# 关键运行场景与端到端链路

本文把功能规格转换成可测试的端到端场景。每个场景都应最终映射到自动测试、Host Mock 或板端验证证据。

## S1：系统启动与状态汇总

```text
Linux boot
 -> services start
 -> vehicle_core builds system state
 -> rpmsg_srv waits for RTOS HELLO
 -> media/audio/infer/voice report readiness
 -> cockpit_ui renders states
```

验收重点：任何非关键模块离线时 UI 仍能启动；状态不得硬编码。

## S2：Camera0实时预览

```text
Touch Camera page
 -> UI requests Front preview
 -> vehicle_core routes request
 -> media_srv opens logical Front camera
 -> V4L2 DQBUF
 -> frame queue
 -> UI renderer
```

验收：稳定运行、有界队列、可退出、FPS/丢帧可统计。

## S3：Camera1离线降级

```text
Rear requested
 -> CameraManager discovers Rear Offline
 -> RESULT = unavailable
 -> UI shows Rear Offline
 -> Front capture remains alive
```

## S4：抓拍

```text
UI/Voice SNAPSHOT
 -> vehicle_core validates
 -> media_srv captures/saves
 -> RESULT(path)
 -> UI/TTS feedback
```

ACK 仅表示受理，文件实际落盘后才返回成功 RESULT。

## S5：录像

```text
START_RECORDING
 -> media_srv encoder/muxer
 -> periodic health/storage checks
 -> STOP_RECORDING
 -> finalize file
 -> RESULT
```

异常：低磁盘、摄像头掉线、编码器错误均需安全结束。

## S6：RTSP

```text
Camera frame
 -> encoder
 -> RTSP server
 -> RTL8822CE
 -> LAN client
```

Wi-Fi掉线时本地预览与录像不得自动失败。

## S7：视觉AI

```text
media_srv samples latest frame
 -> preprocessing
 -> RKNN
 -> structured inference result
 -> UI overlay / vehicle_core
```

推理慢时丢旧帧，不堆积30fps历史帧。

## S8：语音控制摄像头

```text
Mic PCM
 -> VAD/ASR
 -> intent
 -> vehicle_core whitelist validation
 -> SWITCH_CAMERA
 -> media_srv
 -> RESULT
 -> TTS
```

禁止 ASR/LLM 文本直接进入 shell。

## S9：Linux/RTOS握手

```text
Linux rpmsg_srv
 <-> transport
 <-> RT-Thread rpmsg_task
HELLO -> capability/version -> heartbeat
```

未完成握手时 UI 显示 RTOS Offline，Linux功能继续。

## S10：MPU6050状态上报

```text
MPU6050
 -> RT-Thread sensor_task
 -> SENSOR_REPORT
 -> rpmsg_srv
 -> vehicle_core
 -> Vehicle/Sensor UI
```

实际I2C控制器和pin由后续板级验证确定。

## S11：模拟车身控制

```text
Touch / voice
 -> SET_SIMULATED_LED
 -> vehicle_core
 -> RPMsg CONTROL_CMD
 -> RT-Thread control_task
 -> simulated state
 -> RESULT / VEHICLE_STATE
 -> UI
```

页面必须显示 `SIMULATED`。

## S12：大模型自由问答

```text
ASR text
 -> fixed-command matcher misses
 -> RKLLM
 -> natural language response
 -> TTS
```

若模型提出动作，只能作为候选 intent 再经 vehicle_core 校验。

## S13：部分故障

必须验证至少：

- Rear Offline；
- RTOS Offline；
- RKNN unavailable；
- Voice unavailable；
- Wi-Fi disconnected；
- low storage。

目标是“诚实降级”，不是所有模块都必须在线才能进入主界面。

## S14：单一Qt shell页面导航与服务调用

```text
Touch Home tile
 -> cockpit_ui page router / QStackedWidget
 -> target page becomes visible
 -> asynchronous service request
 -> media_srv / audio_srv / vehicle_core / infer_srv
 -> state or frame delivery
 -> cockpit_ui renders result
```

验收重点：切页不启动IMX6ULL旧GUI ELF；GUI线程不阻塞V4L2、音频、推理、
RPMsg、文件扫描或网络操作。服务离线时页面保持可操作并显示真实状态。

音乐与视频页面只发出播放控制并消费状态；传感器页只消费
`RT-Thread -> RPMsg -> vehicle_core` 数据；Camera页只消费 `media_srv` 提供的
帧接口；AI页只消费 `infer_srv` 的结构化结果。
