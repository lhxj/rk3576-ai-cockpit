# 接口约束（设计草案，待P002/P003/P006细化）

## 1. 控制层

Linux用户态控制IPC优先复用ZeroMQ思路，实际依赖版本通过CMake显式检测。
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
