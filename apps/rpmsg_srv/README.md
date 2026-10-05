# apps/rpmsg_srv

Host实现SensorClient/SensorRuntime与配套rk3576_sensor绑定KO，独立NS rk3576-sensor-v1；
不使用generic rpmsg_char或Linux I2C采集。协议/生命周期/测试边界见
[Host结果](../../docs/bringup/mpu6050/LINUX_CORE_QT_HOST_RESULT.md)。

用户态read/poll/write最大256字节、single open、ready/counters ioctl，source=RUNTIME只供
真实sensor backend状态，Fake传感器仅测试fixture。默认不启用sensor设备；启用路径需要
完整配套kernel/DT/固件与具体板端运行计划，当前NOT_DEPLOYABLE。
