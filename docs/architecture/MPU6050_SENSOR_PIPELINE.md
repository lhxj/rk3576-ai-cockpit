# MPU6050 sensor pipeline

2026-10-05。设计与最小实施边界，**NOT_IMPLEMENTED**。
真实MPU6050 → RK3576 I2C9候选 → BUS M0 RT-Thread sensor_task →
独立rk3576-sensor-v1 RPMsg endpoint → Linux sensor KO → rpmsg_srv →
sensor adapter → VehicleCore canonical state → Qt Vehicle/Sensor。

## 已有接口与增量

`apps/rpmsg_srv`只有README，不存在正式用户态transport。冻结echo KO只在probe
发HELLO、ACK后发PING；回包打印内核日志，无char device/poll/read/write入口。
SI_HEALTH_V1为test-only，module参数提供健康计数，不能拿来承载正式sensor协议。
不假设/dev/rpmsg*、rpmsg_char或generic remoteproc；保留paired kernel启动链。

新增独立sensor绑定KO，匹配NS name；提供有界record read/poll与控制write，
固定最大包长、单用户owner、断开/endpoint remove唤醒、有限queue、非阻塞trysend。
callback只校验来源/包长度并复制进有界队列，不能读I2C、更新Qt或等待消费者。
慢消费者覆盖旧样本，control/status独立保留，覆盖、发送失败与序号gap分别计数。
移除时先停止work、唤醒reader、撤销endpoint引用，按生命周期释放；禁止force KO。

`VehicleState::sensor`目前只有BinaryState，RTOS域为Mock adapter；需增加typed
SensorState，包含link/MPU/data状态、raw/config、换算值、两个epoch、订阅和序号、
本地monotonic接收时间、年龄/错误及统计。经Core adapter的唯一入口更新，
不从Qt改canonical state。旧BinaryState继续兼容现有控制命令与mock测试。
Core boot epoch/revision与M0 remote_boot_epoch各自保留，不复用。

Qt `vehicle_page.cpp`现有6格为局部QLabel固定“-- / N/A”，标题固定MOCK；
需保存标签成员，增加芯片温度/状态/sequence/年龄与source；
`mapVehicleState`增量映射并经既有GUI queued handoff。无有效值显示--，断流保留
最后值必须标STALE/OFFLINE与年龄。GUI更新5–10Hz计数合并，服务采样/发布20Hz。
用模块自身XYZ，轴方向依实物丝印记录，不输出pitch/roll/yaw，不称环境温度。

## RTOS采样与服务

优先原RT-Thread I2C框架/HAL，补I2C9_M1 pinmux、配置注册与held-clock模式。
100kHz候选；20Hz定时等待，使用deadline节拍避免事务时间叠加漂移，
记录读取间隔/抖动。原驱动硬编码timeout为RT_TICK_PER_SECOND且再次转毫秒，
须明确tick/毫秒单位并支持该bus有界timeout；不能仅设bus.timeout却仍走旧常量。
每次有限重试、错误不伪造零值、不刷新旧样本时间；health与发送不被I2C等待拖住。

依据[InvenSense寄存器资料](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf)
与本轮用户明确寄存器要求进行后续逐项核验；官方旧URL当前重定向，完整手册下载
及版本hash尚未闭合，第一轮未声称已据完整手册实现驱动。
WHO_AM_I 0x75预期0x68；7位地址0x68/0x69由AD0决定。初始量程候选±2g、±250°/s。
从0x3B连续14字节读AccelXYZ/Temp/GyroXYZ，big-endian int16二补码。
reset/wake/时钟源/DLPF/分频配置应带有限等待和关键读回；内部ODR、轮询频率、
发布频率分别记录，实际量程以读回为准。候选换算：accel raw/16384 g、gyro raw/131
°/s、芯片温度raw/340+36.53°C，必须核手册及配置后使用，不将错误量程数据换算成有效。

传输实例仍只有一个、link4/mailbox/共享区/vring及load/entry冻结。
派生MPU_SENSOR_V1保持独立health endpoint与sensor endpoint，启动/退出/租约
有界；共享transport只有在所有owner退出后释放，不从health loop提前deinit。
health能力保留为可验证路径，sensor service为独立产品协议，NS/endpoint配置
须在Host验证地址冲突与资源上限后最终定案。

## 有效性与时钟

RTOS在线、RPMsg在线、MPU可用、数据有效分别建模。收到新remote epoch或endpoint
失联即撤销订阅/旧数据；重新HELLO和新订阅后新有效样本才能恢复ONLINE。
旧epoch/重复/倒序包不能刷新新鲜度。Linux本地monotonic用于到达年龄与超时；
M0 timestamp单位显式声明，未同步时不计算两时钟相减的单向延迟。
M0每启动唯一epoch的来源是协议前置设计门，不能用编译时间、固定0、Core epoch
或只用归零tick冒充。具体来源需现有BSP可用证据审查，第一轮未擅自扩展硬件随机数业务。

## 资源与审批

[接线/ownership](../bringup/mpu6050/WIRING_OWNERSHIP.md)、
[协议草案](../bringup/mpu6050/PROTOCOL_V1.md)、
[构建/部署草案](../bringup/mpu6050/BUILD_DEPLOY_MANIFEST.md)、
[第一轮结果](../bringup/mpu6050/BOARD_RESULT.md)。
clock/reset/权限未闭合不部署，不Linux直读或UART转发，不修改默认恢复路径。
所有阶段通过且300秒共存/退出/获批恢复通过才授予最终PASS。
