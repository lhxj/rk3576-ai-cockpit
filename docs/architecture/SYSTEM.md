# 系统架构基线

这是根据本会话已确定方案整理的设计，不是实现完成报告。

## 一、目标

基于LubanCat-3 v2 / RK3576，参考IMX6ULL Qt车机的页面组织和交互需求，
结合经独立审查后可采用的语音模块思路，整合双OV8858、多媒体、AI、Wi-Fi和
片内Linux + RT-Thread AMP/RPMsg。RK3576应用重新实现，不以旧ARM32工程为代码基础。
不使用外部MCU、Pi Camera Module 3、LVGL；不做千兆Ethernet PHY驱动开发。

## 二、硬件与运行域

| 硬件 | 归属/连接 | 当前开发处理 |
|---|---|---|
| OV8858×2 | Linux，MIPI CSI / CIF / ISP | 单CAM0先开发；第二套线缆到货后验证双摄 |
| HDMI屏+USB触摸 | Linux显示与输入栈 | 用户已确认可用；盘点实际图形会话再选Qt后端 |
| 板载咪头+耳机 | Linux ALSA | 一个音频管理入口，先保持既有可用路由 |
| RTL8822CE | Linux PCIe/Wi-Fi | 主网络；本地功能不依赖互联网 |
| MPU6050 | 计划交给RT-Thread | 未接入；先审查I²C控制器/IRQ/clock/pinctrl归属 |
| LED/按钮/蜂鸣器 | 业务模拟 | 必须标注SIMULATED，不声称硬件实时控制 |
| RTOS Remote Core | RK3576片内AMP | 核号、启动方式、内存与通知机制以实际SDK为准 |

## 三、模块

| 模块 | 所有权与职责 |
|---|---|
| cockpit_ui | 单一Qt shell、页面路由、显示与触摸。消费状态/帧，不直接打开硬件。 |
| vehicle_core | 请求校验、状态机、业务路由、结果汇总。不搬运原始帧。 |
| media_srv | 摄像头、Frame生命周期、预览/抓拍/录像/编码/RTSP。 |
| audio_srv | PCM录放音、设备选择、音量/播放仲裁，避免多模块抢ALSA。 |
| voice_srv | 唤醒/VAD/ASR、意图、会话与TTS，调用infer_srv使用LLM。 |
| infer_srv | RKNN视觉与RKLLM语言后端、模型预算/任务调度、限并发。 |
| rpmsg_srv | Linux用户态与内核接口适配；协议、握手、心跳、ACK/RESULT。 |
| monitor模块 | 系统指标、RTOS状态、后期perf/ftrace/eBPF；初期可先做库。 |
| RT-Thread tasks | rpmsg、sensor、control、monitor、heartbeat；真实与模拟分开。 |

这些是逻辑模块边界，不要求第一天强拆成大量进程。进程合并/拆分需要ADR，
不得因此打破所有权。根CMake的host烟测也不是vehicle_core正式实现。

P006 已建立 Host-only `vehicle_core` foundation：结构化命令、ACK/RESULT、领域
Mock adapter、canonical state/revision、service registry、deadline/late-result、
有限幂等缓存及 Voice CandidateAction 桥。该结果仅为 `VEHICLE_CORE_HOST_PASS`，
详见 `VEHICLE_CORE.md`；真实服务和 UI IPC 尚未接入。

### cockpit_ui 页面与参考边界

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

页面运行在一个主Qt shell中，优先用 `QStackedWidget` 或项目统一页面路由。
IMX6ULL参考归档的角色固定为 `UI_REFERENCE_ONLY`，迁移策略为 `REIMPLEMENT`；
不得复制ARM32 ELF/object、Qt生成文件、旧Makefile、FSL sysroot、AP3216C sysfs
或QProcess子应用架构。旧图标和媒体保持 `LICENSE_UNVERIFIED`，不进入主仓。

Qt只负责 display、interaction 和 state presentation。GUI线程不得采集V4L2帧、
运行推理、阻塞音频/RPMsg，或执行长时间文件与网络操作。页面使用下列服务路径：

```text
Camera preview: media_srv -> frame delivery -> cockpit_ui
Media control:   cockpit_ui -> service IPC -> media_srv / audio_srv
Sensor state:    RT-Thread -> RPMsg -> vehicle_core -> cockpit_ui
AI result:       infer_srv -> vehicle_core / IPC -> cockpit_ui
```

## 四、数据与控制路径

**视频数据**：OV8858 → CSI/CIF/ISP → V4L2缓冲 → 预览/视觉/编码各支路。
RGA仅用于适配的图像处理；MPP调用硬件编解码；FFmpeg/OpenCV按需使用。
预览/AI允许丢旧帧以限制延迟；录像/编码定义独立过载策略。
先用明确生命周期的拷贝方案，后续验证DMA-BUF共享与同步，不预先声称零拷贝。

**语音数据**：板载咪头 → audio_srv PCM → voice_srv ASR → 规则或infer_srv LLM。
候选命令 → vehicle_core白名单校验 → 目标模块执行 → RESULT → TTS → audio_srv → 耳机。
命令“受理”与“执行成功”分开，不能在ACK时播报成功。

**跨域控制**：Linux rpmsg_srv ↔ Linux RPMsg / 平台transport ↔ 共享内存与通知
↔ RTOS RPMsg-Lite。结构只描述目标；不预设generic remoteproc启动。
RPMsg仅传小消息，不传原始视频与大块PCM。

**显示**：Qt适配当前GNOME图形会话；正式嵌入式EGLFS/KMS/Wayland方案待盘点。
不混跑多个抢DRM所有权的前台进程，不擅自停止桌面服务。旧工程通过QProcess
启动多个GUI程序只作为已拒绝的参考实现，不进入目标数据流。

## 五、范围保留与阶段降级

完整目标仍包含双摄、录像/媒体播放/无线RTSP、视觉AI、离线语音、AMP/RPMsg、
真实MPU6050与模拟控制、Qt、BSP配置及后期Buildroot定制/性能诊断。
等待线缆不影响单摄、Qt、语音或AMP调查。

天气是联网可选项，离线时显示缓存时间或不可用；无GNSS时仅做导航模拟。
项目是台架演示系统，不承诺真实车辆安全控制或车规认证。

## 六、资源边界

4GB RAM同时承载OS、桌面、图像buffer、模型与未来RTOS保留区。
模型大小、上下文长度、并发数量必须通过预算及实测决定。
Kernel/DTB、ISP库/IQ、MPP/RGA、NPU驱动/Runtime和模型保持兼容版本记录。
仅修改NV12输出尺寸未必降低Sensor MIPI速率，不能代替物理链路修复。
