# MPU6050 sensor pipeline

2026-10-05：Host实际源码闭环通过；实板传感器业务NOT_RUN。

真实MPU6050 → RK3576 I2C9_M1 → BUS M0 RT-Thread sensor_task →
rk3576-sensor-v1/0x3005 → Linux rk3576_sensor KO → apps/rpmsg_srv SensorRuntime →
VehicleCore typed SensorState canonical入口 → Qt Vehicle/Sensor。

M0复用已审查RT-Thread I2C/HAL：100kHz、配置ODR20Hz、读取目标20Hz、发布上限20Hz。
成功读完成才更新raw时间与sample_seq，失败失效且不刷新旧数据。独立sensor endpoint
与health共享唯一RPMsg instance；callback短临界copy、control worker初始化/发包、
最新样本有界交接，900秒统一owner窗口，双owner先退出才deinit。

Linux KO保留mcu-amp CCF owner两额外引用与rate exclusive，不触发Linux I2C事务。
单open、record read/poll、nonblocking write、ready/counters ioctl、sample latest/control8
分离。callback对象使用fenced kref，最后释放排process work；remove唤醒reader，
open fd持module与clock引用直到close，无force unload。

rpmsg_srv worker先QUERY匹配generation/request/context，再独立getrandom nonce bind，
remote epoch不可混Core epoch。订阅20Hz/5秒租约、≤2秒renew、单次trysend与有限失败。
重复/倒序/旧generation/旧epoch包不刷新新鲜度；本地monotonic接收时间用于500ms stale
与6秒offline。只匹配QUERY可确认新epoch，失效旧订阅后重新握手。退出UNSUB最多等250ms，
不成功明确lease兜底；fd/eventfd/worker close+join，无GUI线程I/O。

VehicleCore::report_sensor_state为sensor adapter canonical唯一入口，校验typed配置/状态
与raw换算。旧sensor/rtos BinaryState兼容现有Mock控制；真实sensor状态与模拟LED/buzzer
独立。Qt由后台callback经queued交接，六轴g/°每秒、MPU芯片温度、source/seq/age/模块XYZ；
100ms显示合并计数，没有值--，保留值必须显式STALE/OFFLINE。不输出pitch/roll/yaw。

实现/验证与精确产物见[Linux/Core/Qt Host结果](../bringup/mpu6050/LINUX_CORE_QT_HOST_RESULT.md)、
[service结果](../bringup/mpu6050/SERVICE_HOST_RESULT.md)、[wire协议](../bringup/mpu6050/PROTOCOL_V1.md)
与[接线ownership](../bringup/mpu6050/WIRING_OWNERSHIP.md)。原冻结echo/SI_HEALTH协议和恢复文件保留。

HOST_PASS只说明FakeI2C/FakeTransport及实际codec/Core/Qt Host通过；RTOS_SENSOR_PASS、
RPMSG_SENSOR_PASS、UI_SENSOR_PASS、最终集成PASS仍未运行。正式包尚缺AArch64应用、
有界WHO/readback/raw观测与具体部署恢复组合；不把Host固件/KO单独部署，不冒充硬件证据。
