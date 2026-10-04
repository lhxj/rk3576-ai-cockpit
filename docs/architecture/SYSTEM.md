# 系统架构基线

这是根据本会话已确定方案整理的设计，不是实现完成报告。

## 一、目标

基于LubanCat-3 v2 / RK3576，复用IMX6ULL Qt车机应用思路与LLM_Voice_Flow语音模块，
整合双OV8858、多媒体、AI、Wi-Fi和片内Linux + RT-Thread AMP/RPMsg。
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
| cockpit_ui | Qt页面、显示与触摸。消费状态/帧，不直接打开硬件。 |
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

## 四、数据与控制路径

**视频数据**：OV8858 → CSI/CIF/ISP → V4L2缓冲 → 预览/视觉/编码各支路。
RGA仅用于适配的图像处理；MPP调用硬件编解码；FFmpeg/OpenCV按需使用。
预览/AI允许丢旧帧以限制延迟；录像/编码定义独立过载策略。
先用明确生命周期的拷贝方案，后续验证DMA-BUF共享与同步，不预先声称零拷贝。

**语音数据**：板载咪头 → audio_srv PCM → voice_srv ASR → 规则或infer_srv LLM。
候选命令 → vehicle_core白名单校验 → 目标模块执行 → RESULT → TTS → audio_srv → 耳机。
命令“受理”与“执行成功”分开，不能在ACK时播报成功。

**跨域控制**：Linux rpmsg_srv ↔ Linux RPMsg / 平台transport ↔ 共享内存与通知
↔ RTOS RPMsg-Lite。业务结构仍描述目标；当前底层最小链已经实测：BUS M0 RT-Thread RPMsg-Lite remote与paired Linux mailbox/virtio master、link4、HELLO_ACK/PONG及冷恢复通过，使用显式U-Boot文件入口而非generic remoteproc。见[集成tip](../amp/AMP_RPMSG_INTEGRATION_TIP.md)。用户冻结该链，不开发RTOS业务或合入UI/Voice/Media。
RPMsg仅传小消息，不传原始视频与大块PCM。

**显示**：Qt适配当前GNOME图形会话；正式嵌入式EGLFS/KMS/Wayland方案待盘点。
不混跑多个抢DRM所有权的前台进程，不擅自停止桌面服务。

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
