# 产品完整功能规格（Product Functional Specification）

> 项目：基于鲁班猫3 RK3576 的 AMP AI 车载多媒体座舱
>
> 本文回答“最终系统需要提供哪些功能、由谁完成、依赖什么硬件、失败时如何降级”。
> 本文描述目标功能，不等于实现完成状态。实时完成情况以 `docs/STATUS.md` 为准。

---

## 1. 产品定位

本项目是在 LubanCat-3 v2 / RK3576 上构建的台架型 AI 车载多媒体座舱原型，用于展示：

- Linux 图形、多媒体、网络与端侧 AI 能力；
- RK3576 片内 Linux + RT-Thread AMP 异构运行；
- Linux 与 RTOS 通过 RPMsg/RPMsg-Lite 进行小消息控制和状态交换；
- 双摄像头、语音、视觉推理、传感器、触控 UI 和系统监控的协同。

它不是车规量产系统，不承担真实车辆安全关键控制，不宣称符合 ISO 26262、AEC-Q 或其他车规认证。

---

## 2. 最终功能总览

系统最终应提供九类功能：

1. **座舱主界面与触控交互**
2. **双路摄像头与多媒体处理**
3. **录像、抓拍、媒体文件与 RTSP**
4. **视觉 AI 推理**
5. **离线语音助手与本地大模型**
6. **Linux + RT-Thread AMP / RPMsg**
7. **MPU6050 传感器与模拟车身控制**
8. **Wi-Fi、服务状态与系统监控**
9. **日志、故障恢复、配置与工程调试**

系统必须支持“部分模块离线但主界面仍可用”的降级运行，而不是某一路设备失败就导致整个程序退出。

---

# 3. 座舱 UI 与触控

## 3.1 主界面

Qt/C++ `cockpit_ui` 提供适配 4.3 英寸 HDMI 触摸屏的全屏座舱界面。

目标页面结构：

```text
cockpit_ui
├── Home
├── Camera
├── Media
│   ├── Music
│   └── Video
├── AI
├── Vehicle / Sensor
├── Monitor
└── Settings
```

AI页面可容纳视觉结果、语音会话和本地模型状态；首版可先实现少量页面，但
统一导航结构必须支持后续扩展。应用采用单一主Qt shell，页面切换优先使用
`QStackedWidget` 或项目选定的统一页面路由。

IMX6ULL参考工程仅为 `UI_REFERENCE_ONLY`：可参考主菜单、大触控入口、
音乐/视频控制项和传感器图表呈现。RK3576 `cockpit_ui` 的迁移策略是
`REIMPLEMENT`，不得复制旧ARM32 ELF/object、Qt生成文件、平台设备访问、
QProcess子应用架构或许可未核验资源。

## 3.2 首页应展示

至少显示：

- 当前时间；
- Wi-Fi 状态；
- Front Camera / Rear Camera 状态；
- Linux / RTOS 链路状态；
- Audio / Voice 状态；
- AI 模型状态；
- MPU6050 状态；
- 当前录像/推流状态；
- 故障或告警摘要。

状态必须来自运行时服务，不能因为文档中写了 PASS 就硬编码成 ONLINE。

## 3.3 触控交互

支持：

- 页面切换；
- 摄像头选择；
- 抓拍；
- 开始/停止录像；
- 开始/停止 RTSP；
- AI 推理开关；
- 语音功能开关；
- 模拟 LED/蜂鸣器等控制；
- 系统诊断页面刷新。

GUI 线程不能执行阻塞式摄像头采集、模型推理、音频等待、网络 I/O、长文件
操作或 RPMsg 等待。Qt只负责显示、交互和状态呈现；服务数据通过异步接口进入UI。

---

# 4. 双摄像头与视频

## 4.1 摄像头

目标硬件为两颗 OV8858：

- Front Camera：前向/主摄概念；
- Rear Camera：后向/辅助摄概念。

当前开发基线允许只有 Front 在线；Rear 未接好时 UI 必须显示 Offline，而不是阻止系统启动。

## 4.2 采集链

Linux 侧目标链路：

```text
OV8858
  -> MIPI CSI / DPHY
  -> CIF / ISP
  -> V4L2 MPlane
  -> media_srv
```

当前已验证开发格式以 `1632x1224 @ 30fps, NV12` 为基线，但最终软件不得把 `/dev/video11`、`/dev/video31` 或 `/dev/video-camera0` 永久等同于某一物理摄像头。

物理摄像头身份应通过 media graph、sensor 名称、I2C topology 或稳定映射规则确认。

## 4.3 media_srv 责任

`media_srv` 负责：

- 摄像头发现与打开；
- V4L2 format 配置；
- MMAP/DMABUF 等 buffer 生命周期；
- Capture Thread；
- bounded frame queue；
- 前/后摄状态管理；
- 预览帧分发；
- AI 帧抽样；
- 抓拍；
- 录像；
- 编码；
- RTSP 输出；
- 设备掉线/错误恢复。

不允许 Qt 界面自己打开 `/dev/video*`。

## 4.4 CameraManager

建议抽象：

```text
CameraManager
├── FrontCamera : Online / Offline / Error
└── RearCamera  : Online / Offline / Error
```

每个 Camera 实例至少暴露：

- logical_id；
- physical identity；
- state；
- format；
- sequence；
- timestamp；
- drop count；
- error reason。

## 4.5 预览

Qt Camera 页面应能：

- 显示单路预览；
- 前后摄切换；
- 显示设备状态、分辨率和 FPS；
- Rear Offline 时给出明确占位状态；
- 不因为第二路摄像头不存在而阻塞 Front Camera。

未来可扩展双窗口/PIP，但不是第一阶段必须项。

---

# 5. 抓拍、录像、媒体播放与 RTSP

## 5.1 抓拍

用户点击 Snapshot 或语音命令后：

```text
cockpit_ui / voice_srv
 -> vehicle_core
 -> media_srv
 -> 保存图片
 -> RESULT(path/status)
 -> UI/TTS反馈
```

抓拍必须基于完整有效帧，保存路径与失败原因可查询。

## 5.2 录像

支持：

- 开始录像；
- 停止录像；
- 显示录像状态；
- 文件名/时间信息；
- 存储不足处理；
- 摄像头掉线时安全结束文件。

优先利用 Rockchip MPP 硬件编码能力；是否使用 FFmpeg/GStreamer 封装由实际 SDK 和实现验证决定。

录像链和 UI 预览链应采用独立消费策略，不能因为 UI 丢帧导致录像数据错误。

## 5.3 媒体播放

最终 UI 提供本地媒体文件播放入口，例如录像回放或演示视频。

播放功能应与实时 Camera Capture 所有权分离，避免多个模块同时抢占硬件解码/显示资源。

## 5.4 RTSP

通过 RTL8822CE Wi-Fi 对局域网输出视频流。

目标：

```text
Camera
 -> media_srv
 -> MPP encoder
 -> RTSP server / framework
 -> Wi-Fi
 -> remote client
```

RTSP 是局域网功能，不要求互联网可用。

需要记录：

- stream URL；
- codec；
- resolution；
- bitrate；
- FPS；
- client count；
- reconnect/error state。

---

# 6. 视觉 AI

## 6.1 infer_srv

`infer_srv` 是 Linux 侧 AI 资源入口，负责：

- RKNN runtime 生命周期；
- 视觉模型加载；
- RKNN 输入预处理；
- 推理队列；
- 后处理；
- 结果发布；
- 模型状态与性能统计；
- 与 RKLLM 并发时的资源控制。

## 6.2 数据路径

```text
media_srv
 -> sampled frame
 -> RGA / CPU preprocessing
 -> infer_srv / RKNN
 -> structured result
 -> vehicle_core / cockpit_ui
```

AI 不应消费所有 30fps 帧；应按模型能力和 UI 需求抽样，并保持有界队列，优先丢弃过时帧而不是无限积压。

## 6.3 视觉功能形态

具体模型可根据最终参考资料确定，但架构至少支持：

- object detection / recognition 类结果；
- bounding boxes / labels / confidence；
- FPS / inference latency；
- enable/disable；
- model unavailable/error 状态。

未确定具体模型前，不能在产品描述中声称已完成某类驾驶安全算法。

---

# 7. 语音助手与 RKLLM

## 7.1 音频链

当前开发音频基线：

```text
板载麦克风
 -> ALSA capture
 -> audio_srv
 -> voice_srv
```

输出：

```text
voice_srv / TTS
 -> audio_srv
 -> ALSA playback
 -> 3.5 mm headphone
```

## 7.2 audio_srv

负责：

- ALSA capture/playback 统一访问；
- PCM format 协商；
- 音量与播放仲裁；
- xrun/错误统计；
- 录放资源互斥；
- 为 voice_srv 提供稳定 PCM 数据接口。

避免 ASR、TTS 和 UI 各自直接打开声卡造成竞争。

## 7.3 voice_srv

目标流程：

```text
Mic
 -> Wake / VAD
 -> ASR
 -> intent classification
 -> rule path or RKLLM
 -> candidate command
 -> vehicle_core validation
 -> execution
 -> RESULT
 -> TTS
```

语音模块可复用 LLM_Voice_Flow 的模块化设计，但必须实际核验其模型、依赖、启动顺序和许可证。

## 7.4 意图与安全边界

语音指令不能直接转换为 shell 命令。

`vehicle_core` 维护允许动作白名单，例如：

- OPEN_CAMERA
- SWITCH_CAMERA
- TAKE_SNAPSHOT
- START_RECORDING
- STOP_RECORDING
- START_RTSP
- STOP_RTSP
- QUERY_SENSOR
- SET_SIMULATED_LED
- PLAY_MEDIA

LLM 只能输出结构化候选动作，由 `vehicle_core` 校验参数、状态、deadline 和权限。

## 7.5 RKLLM

RKLLM 用于：

- 自由问答；
- 复杂自然语言理解；
- 将用户语言映射成结构化候选 intent。

固定车辆命令优先走确定性规则，避免所有命令都依赖大模型。

模型大小、上下文长度和并发能力必须根据 4GB RAM 实测决定。

---

# 8. AMP / RT-Thread / RPMsg

这是项目区别于普通 Linux Qt 车机 Demo 的核心系统能力。

## 8.1 目标运行域

```text
RK3576
├── Linux Domain
│   ├── Qt / UI
│   ├── media
│   ├── audio / voice
│   ├── RKNN / RKLLM
│   ├── network
│   └── rpmsg_srv
│
└── RT-Thread Domain
    ├── rpmsg_task
    ├── sensor_task
    ├── control_task
    ├── monitor_task
    └── heartbeat_task
```

具体 Remote Core、启动方法、内存布局和 mailbox 必须由对应 RK3576 SDK/BSP 证据决定，不能在架构文档中预先写死。

## 8.2 RPMsg用途

只传控制与状态小消息，例如：

- HELLO
- HEARTBEAT
- SENSOR_REPORT
- VEHICLE_STATE
- CONTROL_CMD
- ACK
- RESULT
- ERROR

禁止通过 RPMsg 传摄像头原始帧或大块 PCM。

## 8.3 启动握手

目标行为：

```text
Linux rpmsg_srv starts
    ↓
transport available
    ↓
RTOS HELLO
    ↓
version/capability negotiation
    ↓
heartbeat starts
    ↓
state = RTOS_ONLINE
```

超时则：

```text
RTOS_OFFLINE
```

Linux UI、Camera、Audio 等可继续运行，不应整体退出。

## 8.4 协议

消息至少包含：

- protocol version；
- message type；
- sequence/request id；
- payload length；
- payload；
- error/result status。

禁止直接发送未经定义 ABI 的 C struct 原始内存。

---

# 9. MPU6050 与车辆状态

## 9.1 MPU6050

计划由 RT-Thread 通过 I2C 管理。

目标数据：

- acceleration X/Y/Z；
- gyroscope X/Y/Z；
- optional temperature；
- sample sequence；
- RTOS timestamp / sequence domain。

路径：

```text
MPU6050
 -> RT-Thread sensor_task
 -> SENSOR_REPORT
 -> RPMsg
 -> Linux rpmsg_srv
 -> vehicle_core
 -> cockpit_ui
```

在实际接线和 BSP 资源确认前，不写死 I2C controller 或 GPIO/IRQ pin。

## 9.2 软件模拟控制

LED、Button、Buzzer 不要求当前存在真实外设。

它们用于验证：

```text
Qt / Voice
 -> vehicle_core
 -> CONTROL_CMD
 -> RT-Thread control_task
 -> simulated state change
 -> RESULT / VEHICLE_STATE
 -> Linux UI
```

UI 必须明确标记 `SIMULATED`，不能冒充 GPIO 实测。

---

# 10. vehicle_core

`vehicle_core` 是业务控制中心，但不是大数据转发中心。

职责：

- 维护系统状态机；
- 接收 UI/Voice 请求；
- 参数校验；
- 动作白名单；
- command routing；
- request_id 关联；
- timeout；
- ACK / RESULT 区分；
- subsystem online/offline；
- 将运行状态汇总给 UI。

不负责：

- 复制视频帧；
- 运行 V4L2 capture；
- 直接访问 ALSA；
- 直接运行 RKNN/RKLLM；
- 直接操作 RTOS 外设。

---

# 11. 网络功能

RTL8822CE Wi-Fi 用于：

- SSH/SCP 开发；
- RTSP；
- 可选天气服务；
- 可选在线资源或远程调试。

核心座舱功能应尽量离线运行。

没有 GNSS，因此：

- 不宣称真实导航定位；
- 导航页面如保留，只能是模拟/演示；
- 天气不可用时显示 offline/error，而不是阻塞主程序。

---

# 12. 系统监控与诊断

Monitor 页面最终至少展示：

- CPU 使用率；
- memory；
- temperatures；
- disk usage；
- Wi-Fi；
- Camera FPS/drop/error；
- Audio state/xrun；
- RKNN/RKLLM state；
- RTOS heartbeat；
- RPMsg statistics；
- recording/RTSP state。

后期可使用：

- perf；
- ftrace；
- eBPF；
- Rockchip/NPU/MPP 自带 profiling 工具。

这些是性能和诊断增强，不应成为 MVP 启动前置依赖。

---

# 13. 日志与故障恢复

每个服务应具备结构化日志，至少记录：

- start/stop；
- device identity；
- selected format；
- errors；
- reconnect/retry；
- resource usage warnings；
- request/result id。

典型降级策略：

| 故障 | 系统行为 |
|---|---|
| Rear camera missing | Front继续运行，Rear显示Offline |
| Camera capture failure | Camera页面报错，其他服务继续 |
| RTOS missing | AMP功能Offline，Linux多媒体继续 |
| MPU6050 missing | Sensor unavailable，不伪造数据 |
| RKNN model missing | AI页面Unavailable，Camera预览继续 |
| RKLLM unavailable | 固定命令仍走规则路径 |
| ASR/TTS unavailable | UI控制仍可使用 |
| Wi-Fi lost | 本地功能继续，RTSP/天气Offline |
| storage low | 阻止新录像并提示，不崩溃 |

---

# 14. 启动与关闭

建议 Linux 侧未来由 systemd 或明确 supervisor 管理。

目标启动顺序：

```text
basic OS
 -> hardware/device availability
 -> core services
 -> media/audio/rpmsg
 -> inference/voice
 -> vehicle_core
 -> cockpit_ui
```

实际上可根据依赖调整，但 UI 必须能够显示“服务正在启动/离线”，而不是等待所有服务后才出现。

退出时：

1. 停止接受新请求；
2. 停止语音/AI新任务；
3. 停止录像/RTSP并完成封装；
4. 停止 Capture Thread；
5. 释放 V4L2/ALSA/NPU 资源；
6. 终止 IPC；
7. Qt 正常退出。

---

# 15. 逻辑进程布局

最终逻辑模块不强制“一模块一进程”。第一阶段建议避免过度拆分。

一种可行演进：

```text
cockpit_ui                  process
vehicle_core                process
media_srv                   process
voice/audio/infer           initially co-located or selectively split
rpmsg_srv                   process or vehicle_core adapter initially
RT-Thread                   independent AMP domain
```

拆分依据是：

- 硬件资源所有权；
- 故障隔离；
- 大模型内存；
- IPC成本；
- 调试复杂度。

任何拆分/合并应记录 ADR。

---

# 16. 最终演示闭环

完整演示应能够按如下顺序工作：

```text
1. RK3576启动Linux与RT-Thread AMP
2. Qt座舱界面出现在4.3" HDMI屏并支持触控
3. UI显示Linux/RTOS/Wi-Fi/Audio/Camera实时状态
4. Front Camera实时预览
5. 切换Rear Camera（硬件到位后）
6. 抓拍与录像
7. 局域网RTSP观看
8. RKNN对相机画面进行视觉推理并叠加结果
9. 用户通过麦克风发出语音命令
10. ASR/规则/RKLLM生成候选动作
11. vehicle_core校验并执行，如“切换摄像头/开始录像”
12. MPU6050由RT-Thread采集，通过RPMsg上报
13. UI显示传感器与RTOS heartbeat
14. 软件模拟LED/蜂鸣器控制通过Linux→RPMsg→RTOS→RESULT闭环
15. TTS播报真实执行结果
16. Monitor页面显示CPU/RAM/FPS/AI/RPMsg状态
```

其中任一未验证功能都必须在演示或文档中明确标记，而不能被其他模块的完成状态掩盖。

---

# 17. MVP 与最终版本分层

## MVP-1：Linux基础座舱

- Qt触控界面；
- Camera0实时预览；
- 音频录放；
- Wi-Fi状态；
- vehicle_core；
- 基础监控。

## MVP-2：多媒体闭环

- 抓拍；
- 录像；
- RTSP；
- CameraManager；
- Camera1上线后双摄切换。

## MVP-3：AI/语音

- RKNN视觉；
- ASR/TTS；
- 规则意图；
- RKLLM辅助自然语言理解。

## MVP-4：AMP核心

- RT-Thread实际启动；
- RPMsg最小echo；
- HELLO/heartbeat；
- MPU6050；
- CONTROL_CMD/RESULT模拟控制闭环。

## Final Integration

- Qt + dual camera + multimedia + vision + voice + LLM + AMP + sensor；
- 异常恢复；
- 资源预算；
- 并发压力测试；
- 演示脚本和文档。

MVP编号仅表示功能分层，不强制开发时间严格串行；AMP调查应从第一天并行进行，因为它是高风险路径。

---

# 18. 不属于完成条件的事项

以下不能单独被视为“项目完成”：

- Host CMake编译通过；
- Mock测试通过；
- Qt页面能显示；
- README存在；
- Kernel里CONFIG_RPMSG=y；
- 能找到某个AMP DTS；
- 摄像头能用v4l2-ctl采流；
- 模型文件能下载；
- RTSP代码能编译。

最终结论必须由目标板实测覆盖相应功能链路。
