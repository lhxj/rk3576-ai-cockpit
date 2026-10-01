# 接口约束（设计草案，待P002/P003/P006细化）

## 1. 控制层

Linux用户态控制IPC先由 `libs/protocol` 与 `libs/ipc` 定义传输无关契约。
ZeroMQ 与 Unix Domain Socket 的选择待进程拓扑和资源需求确定；不直接沿用
LLM_Voice_Flow 的 ZMQ 包装或其协议。
禁止自动下载/FetchContent不可信依赖。视频与PCM用独立有界通道。

命令最少包含：版本、request_id、动作名、参数、deadline、source。
返回分ACK（受理）和RESULT（实际完成/失败）；重试有幂等语义。
LLM输出仅是候选命令，禁止shell字符串、任意路径/寄存器操作直接执行。

## 2. 图像

帧描述至少包括camera_id、sequence、采集时间及时间域、width/height/fourcc、
每plane的stride/bytesused/offset、内存所有权句柄。
V4L2 API plane与颜色plane不同；NV12一个内存plane仍含Y与UV。
队列中禁止裸悬空指针。MVP在DQBUF后拷贝到自有内存再QBUF，
或以受控引用计数延迟QBUF，不能两个方案混用。

将来共享DMA-BUF时须确认导入、格式、stride、同步与fd生命周期；
跨进程fd不能只序列化整数，需恰当的文件描述符传递机制。
此处是接口约束，不代表DMA-BUF链已实现。

## 3. 音频

Capture数据先以用户已验证16kHz mono S16_LE为适配起点，硬件格式需协商。
TTS模型采样率不必相同，必须明确重采样位置；记录period/buffer/xrun。
请求取消时清除旧session排队的TTS/动作，播放时根据策略半双工或后期AEC。

## 4. RPMsg

消息目标：HELLO、HEARTBEAT、SENSOR_REPORT、VEHICLE_STATE、CONTROL_CMD、ACK、RESULT、ERROR。
明确版本、type、seq、payload_len、请求关联和时效。
显式字节序与长度校验，不直接发送C struct原始内存，避免padding/ABI差异。
payload上限取决于实际transport；不要随意写一个通用上限当平台事实。
未同步跨域时钟时不做不可靠的单向延迟相减；可以先测同一侧RTT。

## 5. 状态真实性

- UNVERIFIED：没有证据。
- USER_REPORTED：用户确认，需保留具体条件。
- HOST_TESTED / MOCK_TESTED：不是硬件测试。
- BOARD_TESTED：有命令/设备/配置/输出的板端测试。
- SIMULATED：显式模拟业务，不伪装传感器或GPIO。
- BLOCKED：缺硬件/SDK/授权/环境，列出解除条件。

开机不要因为docs/STATUS写过PASS就把实时设备显示为ONLINE。

## 6. cockpit_ui 页面与服务边界

`cockpit_ui` 是一个主Qt shell。页面切换使用 `QStackedWidget` 或统一页面路由；
页面之间不通过 `QProcess` 启动旧GUI ELF。UI发起控制请求并呈现状态，硬件和
长任务由服务拥有：

| UI功能 | 请求/数据来源 | 服务边界 |
|---|---|---|
| Camera预览 | `media_srv -> frame delivery -> cockpit_ui` | UI不打开V4L2节点；帧所有权和回收由明确接口管理 |
| Music/Video | `cockpit_ui -> service IPC -> media_srv / audio_srv` | UI不自行抢解码器或ALSA设备 |
| Vehicle/Sensor | `RT-Thread -> RPMsg -> vehicle_core -> cockpit_ui` | UI不读取旧AP3216C sysfs或直接阻塞RPMsg |
| AI | `infer_srv -> vehicle_core / IPC -> cockpit_ui` | UI不运行RKNN/RKLLM推理 |
| Monitor | monitor/state API -> cockpit_ui | UI只显示有时间戳和来源的状态，不把缺失值当ONLINE |

GUI线程仅处理事件、绘制、轻量状态转换和用户交互。V4L2采集、推理、音频等待、
RPMsg等待、长文件扫描和网络操作必须位于服务或worker，并通过有界异步接口把
结果送回GUI线程。服务回调进入Qt对象前必须遵守线程亲和性，退出时能够取消和
唤醒等待。

2026-10-01 IMX6ULL参考审查只证明旧交互轮廓，不能作为这些接口已实现的证据。

当前integration实现把`IUiBackend`作为Qt唯一控制入口，并以
`VehicleCoreUiBackend -> IVehicleCoreClient`连接同进程Vehicle Core。Backend统一生成
request id、boot epoch、session和deadline，ACK只形成local pending overlay；最终显示
消费canonical全量snapshot及严格递增revision。Core worker回调必须经Qt queued
invocation回到GUI线程。跨进程transport、重连和daemon生命周期仍属后续接口实现。
