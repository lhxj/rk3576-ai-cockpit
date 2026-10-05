# Linux sensor桥 / VehicleCore / Qt Host结果

2026-10-05。驱动逻辑、协议及应用集成已在Host实际源码闭环测试通过，`HOST_PASS`；RTOS_SENSOR_PASS/RPMSG_SENSOR_PASS/UI_SENSOR_PASS及MPU6050_RTOS_RPMSG_INTEGRATION_PASS仍NOT_RUN。用户断电接线USER_CONFIRMED。本轮子代理没有实板操作；主控仅一次持锁只读SSH连接超时，未登录或访问MPU，当前实时启动身份UNKNOWN/UNREACHABLE，不能沿用此前默认恢复快照作T0。

## Linux有界接口与所有权

新增`rk3576_sensor.ko`匹配`rk3576-sensor-v1`，创建`/dev/rk3576-sensor-v1`（0600、single open）。未open不积累wire、不自动HELLO；用户态只能在SENSOR_IOC_STATE返回abi1/owner_ready1/非removed后握手。probe核实际Linux地址`/i2c@2ae80000` disabled且无子设备、无I2C9 platformdevice；`mcu-amp`已绑定rockchip-amp、7clock、第二pinctrl组GPIO1_B5/B4 function10、I2C9两个CCF引用enabled、xin24m/24MHz与rate exclusive。不发Linux I2C事务；M0别名0x4ae80000不能用于Linux DT路径。

read为完整≤256字节record，短buffer返回EMSGSIZE、不消费；copy fault不消费；poll可读/HUP，write为单次rpmsg_trysend、不重试。callback只检查peer/长度并复制：SAMPLE独立latest一项，control/status ring8项，满队列明确drops；ioctl独立报告generation/Linux覆盖/控制drop/长度错误/send failure。用户态codec继续检查全部typed字段。

remove先从全局callback fence撤销对象，再锁I/O标removed、deregister、唤醒reader；已进入callback或open fd持kref。最后引用只queue到私有ordered workqueue，CCF/device释放在process context；module exit先unregister RPMsg driver、endpoint销毁同步callback，再drain/destroy释放队列。open fops.owner持module引用；不允许force unload。固定kernel `rpmsg_core.c:570..585`是driver remove先于endpoint destroy，故不能假定remove进入时callback已经结束；`virtio_rpmsg_bus.c:298..311`在destroy时摘endpoint并以cb_lock等待在途回调。factory覆盖open/remove/close/最后callback交错与非IRQ释放；这是确定性生命周期测试，不是实板IRQ压力测试。

配套fresh Kbuild-v4：固定kernel commit521833e2d28decbd6473d5717f1f96cc4108e208、prepared config58c9891a…、Module.symvers bebc886c…；vermagic精确`6.1.99-rk3576-m0echo-p026 SMP mod_unload aarch64`。所有undefined symbols在该Module.symvers导出；CONFIG_MODVERSIONS=n，因此CRC比较为N/A，不能称CRC已校验。KO144128B / SHA063175ef8c5a12855192f790b64664f2d14fe439415be44ab383479c8ccea69a。详见[精确清单](LINUX_SENSOR_HOST_BUILD.json)。命令：`python3 scripts/dev/build_sensor_ko.py --output artifacts/local/sensor-ko-host-v4`；SDK、KO不进Git。正式包仍须运行时pinctrl owner/no adapter、同配套kernel/DT与权限预检，不能强制加载其它kernel。

## 用户态与canonical/UI

实际SensorClient先当前generation的QUERY HELLO匹配request/session/Core上下文，再独立Linux getrandom nonce首次bind；已有epoch通过QUERY查询后保留，重连/新generation重新生成session与bootstrap nonce，不覆盖nonce为Core epoch。每2.5秒可发匹配QUERY验证epoch，只有对应pending HELLO STATUS可证明epoch变化；旧SAMPLE/STATUS不切epoch。匹配QUERY返回新epoch立即失效订阅/数据，epoch0使runtime重建fresh nonce后再bind。SUB20Hz/lease5000ms，每≤2000ms续租；ACK只受理，matching RESULT才ACTIVE。duplicate、out-of-order、publish gap、旧包/协议错误分别计数。

有效SAMPLE成功typed/config检查后才换算Accel g、Gyro °/s、MPU芯片温度°C，并记录Linux steady接收时间。500ms无新样本→STALE；控制/流6秒无有效消息或endpoint丢失→OFFLINE，旧值可保留且年龄继续更新；新有效样本才恢复VALID。M0成功读完成时间保留原毫秒，不与Linux时间相减。

SensorRuntime I/O worker独占fd，eventfd用于stop唤醒；异常/getrandom失败/HUP由顶层catch/RAII close处理，不逸出线程杀应用。840秒用户态窗口低于900秒transport共同窗口。退出停止renew、best-effort UNSUB、最多250ms等matching RESULT、close/join；unsubscribe_confirmed明确真假，失败依赖最多5秒租约兜底，不能误报退订通过。

`VehicleCore::report_sensor_state`为typed sensor adapter唯一canonical入口，保留旧BinaryState/MockRtos控制兼容；VALID入口检查runtime/source、epoch/generation/id/seq/online/config、有限且与raw一致的换算。Qt从canonical映射接收，既有MainWindow queued交接到GUI；VehiclePage100ms定时合并（UI merges计数）显示六轴、芯片温度、独立RTOS/RPMsg/MPU状态、seq/age/source/模块XYZ轴，未有值--，旧值显式STALE/OFFLINE。模拟LED/buzzer及其它Mock服务不升级RUNTIME。默认不启用sensor设备；源码CLI opt-in `--backend core --sensor-device /dev/rk3576-sensor-v1`，本轮未执行此板端命令或制作部署入口。

## 实测Host验证与边界

host_ci34/34 CTest、79/79 Python、5/5撤回PASS，日志`artifacts/local/sensor-linux-host/host-ci-final.log`。新增实际C服务↔C++client边界测试、worker五种异常/正常退出、FakeI2C调用生产mpu_init/mpu_sample→生产service→C++worker→VehicleCore→queuedQt真实显示/STALE/--/正常UNSUB+join；Core拒绝错误config/NaN/epoch0。实际KO source factory及固定候选DT身份/路径/pins对照通过；CI无仓外DT时该额外对照明确skip，已有独立本机实际DT验证仍保留，不能将skip说成新环境DT已验证。

实际client/runtime与Qt路径ASan/UBSan+Werror/no-pie通过；client/runtime和KO fixturedetect_leaks1，Qt offscreen因系统Qt插件外部生命周期使用detect_leaks0（不宣称Qt泄漏检测通过）。主控独立最终生产KO factory ASan/UBSan leak检查PASS；隔离CMake client/runtime两项detect_leaks1与Qt detect_leaks0 sanitizer全部PASS，logs在artifacts/local/mpu-root-review/asan-{runtime,qt}-test.log；其独立host_ci34/34 CTest、79/79 Python、5/5撤回全部PASS，日志artifacts/local/mpu-root-review/host-ci-final.log。最后deadline修正后，主控独立重建并复测最终source：artifacts/local/mpu-root-review/deadline-runtime-test.log 2/2 PASS（client/runtime ASan/UBSan、LSan=1），deadline-qt-test.log 1/1 PASS（Qt ASan/UBSan、LSan=0，不能声称Qt泄漏检测通过）。包含实际service的1000ms迟到HELLO负例，拒绝poll之前已过期的matching回复。

下一步仅具体正式运行包：AArch64用户态产物/确切组合hash/启动停止恢复计划及T2/T3有限观测。当前M0 UART尚缺一次实际WHO/关键配置读回及有限raw trace供M0↔Linux100样本抽核，需包阶段补有界观测，不以Linux收到包推导字段已实板核对。APPROVAL_PACKET_NOT_READY，ARM应用未构建；当前无正式sensor安装器/启动包，无实板WHO/配置/采样/NS/IRQ/Qt人工确认/五分钟共存证据，NOT_DEPLOYABLE。既有SERVICE_HOST_BUILD.json的原生FIT5dc2af…及ownership DT33dc67…保持源码/hash不变；诊断/health冻结产物不改。
