# MPU6050 driver / bounded sampler Host milestone

2026-10-05。`DRIVER_SAMPLER_HOST_TESTED / NATIVE_BUILD_VERIFIED / NOT_DEPLOYABLE`。用户已明确确认“已断电接线完成”；此确认来自用户，不是新增采样证据。完整sensor HOST_PASS和三个实板sensor等级仍未取得。子代理没有访问板、安装、启动或读取MPU。

## 实现与范围

`rtos/sensor/mpu6050.[ch]`只保留raw int16与配置元数据。地址允许0x68/0x69，WHO_AM_I预期始终0x68；每次init先失效旧ready，再检查参数/WHO、DEVICE_RESET、两次100ms RTOS等待、reset位与关键配置逐项读回。选择PLL X gyro clock、±2g/±250dps、DLPF3、SMPLRT_DIV49，内部寄存器ODR=1000/(49+1)=20Hz；读取频率20Hz，发布频率本里程碑0。禁止FIFO/DMP/AUX/INT，没有姿态输出。一次0x3B起14字节读取，显式大端二补码转换；不裸struct传输。配置ID0x00010331仅本固定配置标识。

实际RT-Thread adapter使用rt_i2c_transfer/i2c9与已审三patch的100kHz/20ms共享事务预算；不新增总线栈。应用不另加重试，每次尝试1个有界事务（框架retries未用于本driver），失败无无限重试。固定HAL的HAL_ERROR同时可能是NAK或其他状态，adapter忠实记UNAVAILABLE，Fake独立NACK测试不冒充实板能精确分辨NAK；后续若业务需要精确计数须最小增量保留HAL原因。

sensor_task无INIT导出、main/health未调用READY。只有未来独立control worker在Linux资源预检/握手完成后可调用READY，锁存首次非零独立remote nonce、冷启动内不能重写；此milestone没有transport调用者，因此本FIT不能部署做真实采样。READY初始化含200mssettle，禁止在health callback/IRQ同步调用。最多100次采样，连续3错误即stop，不再事务；2KiB静态线程栈，无busy loop。按照事务耗时补至50ms，超期等待至少1tick并计missed，记录start interval max；成功样本完成后取M0 monotonic ms，错误不刷新旧raw/seq/time且valid=0。当前interval是读取尝试间隔，不冒称有效样本或IRQ抖动；正式service应增加绝对deadline和有效间隔统计。M0时基为RT_TICK_PER_SECOND并用uint32无符号tick差扩展uint64，工作线程必须在tick回绕周期内取样；未与Linux同步。

## 手册证据

厂商作者InvenSense `RM-MPU-6000A-00`实际取得版本**4.0 / 2012-03-09 / 47页**，下载host为[授权分销资料托管SparkFun](https://cdn.sparkfun.com/datasheets/Sensors/Accelerometers/RM-MPU-6000A.pdf)，665861 bytes，SHA256 `ccaa6312b9d86a9da79e26e511101e1150dc85a48255600010a854369cf7c05d`。原PDF/提取文本仅ignored artifacts/local/mpu6050-reference，不进Git、不复制厂商driver。原官方[旧手册URL](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf)检索标rev4.2/2013-08-19，但实际下载重定向新站失败，不能称本次成功取得官方host rev4.2/hash。

逐项使用rev4.0的§3寄存器表、§4.2分频、§4.3 DLPF、§4.4/4.5量程、§4.7 FIFO_EN、§4.15/4.16 INT配置、§4.17–4.19 burst字段、§4.28 USER_CTRL、§4.30/4.31电源、§4.33 WHO。寄存器0x19/1A/1B/1C/23/37/38/3B..48/6A/6B/6C/75均与官方rev4.2检索表相符。rev4.1已知修订为SPI reset说明（本实现MPU6050 I2C且不操作SPI接口），rev4.2已知修订§6/7/8/10；尚未获取整份rev4.2逐页差异，后续正式部署前保留手册版本复核项，不冒称已完成全量版本diff。

## Host / 原生构建

实际生产driver Fake覆盖正常/错误WHO、AD0高地址WHO不变、read NACK/timeout/short、write NACK/timeout/short、reset未清、10项关键配置读回错误、非法重init后无旧总线访问、14字节INT16_MIN/MAX/负数、失败输出保留和恢复后新时间。实际生产task factory在100/1000Hz验证提前零访问、非零epoch一次门/禁止重写、100次20Hz/5秒、有限3错误stop及旧样本时间/seq保留。driver/task ASan+UBSan通过；原host_ci31/31 CTest、74/74 Python（新增2项）、5/5撤回通过，不删原门槛。fixture不算硬件证据。

原生命令：`python3 scripts/dev/build_mpu_sensor_driver.py --output artifacts/local/mpu6050-driver-native-v4`。精确SI_HEALTH persistent-host-v2源码身份+HAL输入hash检查、SDK副本与symlink隔离、显式RTT_ROOT/CC/GCC、三patch fuzz0，原生SCons useconfig/build与签名FIT/原controlDT验签通过。原main/amp_echo.c保持SHA一致，载入/入口/shared/link/mailbox未改变。API使用-u仅保留入口，未接启动控制。

ELF text128772/data2672/bss532096（含heap/stack及NOLOAD预分配区），bss_end/heap_start0x2370c，heap_end0x7fc00，heap378100 bytes，原main stack1024，另sensor静态栈2048已计BSS。entry0x141，LOAD区完整位于0..0x80000，linker原文件hash一致；共享NOLOAD保持原合同。FIT136704 bytes(0x21600)、image131448，符合冻结loader FIT≤0x90000/image≤0x80000，data offset/size/hash/signature nodes与实际payload匹配；实际assert见builder。运行期stack/heap高水位未测。精确产物/来源见[build manifest](DRIVER_HOST_BUILD.json)，无新KO/安装器/可执行启动包。

## 下一独立Host里程碑

在本driver上实现独立sensor service/实际公共44字节包络codec、订阅租约/背压/epoch fence及Linux桥，再集成Core/Qt。epoch拟由Linux getrandom生成独立nonce，M0本启动仅首次绑定且重复HELLO查询既定epoch；新客户端不能重写，Core epoch独立。READY必须在owner预检/epoch绑定后排入独立worker。

固定RK3576 RPMsg config为496B payload/64 buffer，RL_MAX_INSTANCE_NUM12是instance预算，不是endpoint上限。rpmsg_lite_create_ept动态分配endpoint/list节点并查重；现health0x3004保留，sensor拟0x3005，无源码冲突；NS使用53。新增1 endpoint/1有界control队列需在下个实际native配置记录sizeof与heap预算，并测试分配失败回滚。不用64buffer推断endpoint数量或无限队列。sensor NS/传输未实现，不声称endpoint运行已核。

主控独立复核：实际mpu6050.c与include生产sensor_task.c的fixtures，-Wall/-Wextra/-Werror、ASan/UBSan no-pie与detect_leaks=1，driver及task100/1000Hz全部PASS；二进制保留artifacts/local/mpu-root-review。
