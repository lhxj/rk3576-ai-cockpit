# MPU6050 RTOS / RPMsg ExecPlan

2026-10-05（审查从10月4日晚开始）。本次里程碑为第一轮资源/接线审查，
不是完整业务实现或板端验收。最终目标保留 `MPU6050_RTOS_RPMSG_INTEGRATION_PASS`。

## 基线与证据

主仓存在5组未跟踪审查材料，未移动/暂存/覆盖。独立 worktree：
`/home/ywx/rk3576-work/worktrees/rk3576-mpu6050/project`，分支
`agent/mpu6050-rtos-rpmsg`，起点 `6e0aa7c83dd51de88e9f767dde09aadbba9336ea`。
远端fetch确认系统tip相同；`git merge-base --is-ancestor`对
`34076b02a85c145bc61f5746ed32c0f399262366`、Application
`8585c66d27fa65ef11a6531b95656acfa3dc9e8b`、AMP
`ec56833276d31df1e1ce8d36741a552042e2b6ca`均退出0。
34076b0含302.24秒SI_HEALTH_V1共存；6e0aa7c只更新AGENTS。没有重复合并历史feature。

已读根AGENTS、STATUS、SYSTEM、ROADMAP、PLANS、P010，以及AMP tip/JSON、
资源合同、共存/资源/构建/审批/冷恢复记录、部署安装器、C入口、M0构建脚本、
echo/health Linux驱动、公共codec、Core state及adapter、Qt Vehicle页。
原最小AMP10项产物hash全部匹配，SI_HEALTH_V1七文件单独hash登记；
见[保留清单](../bringup/mpu6050/preserved-assets.json)。

实时普通用户SSH只读：默认#8/root p3/boot p2、无stage、RPMsg设备0；
无项目进程。M0物理停机不能由空sysfs推断。第一地址超时，历史共存地址成功；
共用`board_lock`，没有sudo/MMIO/I2C事务、程序部署、采集或启动变更。

## 文件与权限范围

第一轮仅本计划、`docs/architecture/MPU6050_SENSOR_PIPELINE.md`、
`docs/bringup/mpu6050/`、STATUS/ROADMAP/P010；SDK与原始日志留外部/忽略目录。
后续Host范围：`rtos/sensor/`独立驱动逻辑与FakeI2C、`rtos/rpmsg/`sensor service、
`libs/protocol/`兼容增量codec、`apps/rpmsg_srv/`、Core sensor adapter及Qt既有页、
对应tests/CMake。BSP/HAL修改只进新派生副本与最小patch，不改冻结工作树。
Linux sensor KO使用独立目录与匹配Kbuild。

L0静态/Host已授权；L1普通用户只读由本轮实时审查请求涵盖。
接线由用户断电完成且须明确确认；L2/L3测试/上传/boot/DT/clock/reset/KO/冷启动
需具体构建产物审批包和明确批准。当前提示不构成L3部署授权。

## 最小实施顺序与阻塞

1. **当前：资源与接线审查。** 候选I2C9_M1，GPIO1_B5/B4，Pin19/23，
   VCCIO3设计3.3V。PCB最终确认V2R0/20260521与图纸一致；用户已测输入约5V、SDA/SCL各3.3V、AD0=0V（LDO未直接测）；Pin1/19/23空闲已确认、Pin20接串口调试器；继续确认资源/风扇预算，
   完成clock/reset/权限审查后发出`WIRING_READY_FOR_USER`，等待断电接线确认。
   目前仍`WIRING_NOT_READY`，不能把候选表当成接线命令。
2. **Host业务闭环。** 先落实[协议草案](../bringup/mpu6050/PROTOCOL_V1.md)、
   M0 boot epoch来源/唯一性与NS/endpoint数量。驱动100kHz、20Hz定时轮询；
   定向WHO_AM_I、配置读回、14-byte burst、有限timeout/retry。
   单订阅者/有限租约/latest样本/非阻塞发送，错误状态与计数独立。
   Linux typed SensorState -> sensor adapter -> Core canonical state -> Qt，
   本地monotonic年龄、remote epoch隔离、Qt queued更新5–10Hz。
3. **原生派生构建与审批包。** 独立MPU_SENSOR_V1 defconfig/SCons、ELF/map/
   stack/heap核验、FIT签名与controlDT验签，sensor KO匹配kernel/Module.symvers。
   DT只增加经审查的I2C9资源所有权；所有新hash有实际产物后生成，
   不拿SI_HEALTH hash冒充sensor。审批通过后再串行T0–T6。

具体未知项与停止门见[ownership](../bringup/mpu6050/WIRING_OWNERSHIP.md)。
没有安全M0访问/时钟方案时明确BLOCKED，不退化为Linux直读/UART/Mock业务。

## Host验收矩阵（后续实现必须覆盖）

- FakeI2C：正确/错误WHO_AM_I、NACK、超时、短读、reset/wake/配置读回错误。
- burst解析：14字节、高字节先行、二补码负数/边界、量程换算、温度与错误配置。
- 实际codec：golden bytes、大小端、截断/超长、错误版本/类型/长度。
- FakeTransport：订阅/退订、20Hz限速、租约到期、有界背压、覆盖/发送失败统计。
- Linux：duplicate/gap、remote epoch变化与旧包拒绝、超时失效、恢复新样本。
- Core/Qt：canonical映射、STALE/OFFLINE/年龄、source、GUI线程交接、正常退出。
- `bash scripts/dev/host_ci.sh`原门槛保留；全适用ASan/UBSan与非Qt LSan；
  原生SCons/Kbuild独立于根CMake。Fake只证明Host，不算真实传感器证据。

## 板端计划与失败恢复

获批前不执行。T0核实时环境/锁/风扇/接线/hash；T1派生AMP+原health；
T2 M0 WHO_AM_I/配置/有限100样本；T3 Linux100样本字段比对；
T4退订/重订/断开/租约/旧值失效；T5 Core/Qt与人工轻转模块；
T6同一获批窗至少300秒全部既有媒体/视觉/VoiceRuntime+sensor共存。
双UART与Host/板日志均有上限；错误风暴一次即停，缺人工观察标
`USER_CONFIRMATION_PENDING`。固定模块/杜邦线，风扇不移动、不加控制。

记录采样间隔/抖动/I2C错误、publish/rx/gap/drop/租约/年龄、UI合并，
Camera/Vision FPS、MPP实例、record/RTP/audio错误、CPU/RSS/PSS/MemAvailable/
温度/线程/FD。静止合理性（模长约1g、gyro近零）另记安装方向与容差，不称校准。
有风扇后的温度不能直接归因对比旧无风扇结果。
退出按停止订阅→接收/应用join→M0无订阅/health响应→获批正常关机→用户冷恢复
默认入口→文件hash/设备释放核验。无额外重启/拔插/故障注入。

## 当前结果与交接

第一轮只有来源核验/实时只读盘点/候选审查与计划，业务源码未实现。
HOST_PASS、RTOS_SENSOR_PASS、RPMSG_SENSOR_PASS、UI_SENSOR_PASS均未取得；
最终目标未达到。本轮Host CI结果见[结果](../bringup/mpu6050/BOARD_RESULT.md)。
完成该审查里程碑后commit/push并建Draft PR（base=agent/system-integration），不merge。
等PCB/测量与resource审查闭合后才给接线就绪；构建完成后另给精确部署审批包。

2026-10-05续审：源文件确认自动PREV probe须整体延迟，held-clock覆盖probe/xfer/resume，
去掉选I2C9时无条件I2C7 mux副作用；timeout/错误返回必须修复。
TRM确认PD_BUS与外设映射，当前firewall权限仍无新实板证据。见ownership续审条目。

## 2026-10-05 I2C9 BSP 修复结果

已完成早期 probe、Linux-held gate、I2C7 误复用、tick/错误路径四项 Host 修复。实际源码回归及 native SCons 适配诊断配置通过，详见 [I2C9_BSP_FIX](../bringup/mpu6050/I2C9_BSP_FIX.md)。全量 Kconfig solver baseline 缺陷仍在；业务固件、实板 ownership/接线/部署门未通过。

## 2026-10-05 接线前 Host 交付

完成固定 C DT 上的最小 I2C9 Linux-held clock/pin 提案与静态冲突检查；0002 在 resource-ready 后仅解除 I2C9 两个 reset，0003 仅一次使能 INTMUX2BUS 专用 gate，不 reset/disable 共享 INTMUX。原生适配诊断构建和实际驱动 sanitizer 通过，host_ci31/31、59/59、5/5。见 [当前结果](../bringup/mpu6050/PREWIRE_RESOURCE_RESULT.md)。当前板仍默认 Debian；实物电压和候选 pin 确认已完成，不再追加初学者微小 LDO/风扇电流测量。

冻结 BL31 的实际 BUS_MCU 安全权限和 INTMUX reset 状态待厂商精确说明或独立获批无传感器定向验证。该验证只提出有限方案，未实现新 probe 固件/启动包，不执行板端操作。本里程碑不扩展 sensor 产品；正式接线和部署审批门保持，用户确认接线不等于部署授权。

## 2026-10-05 已授权权限诊断准备

用户最新要求进行下一步且不需要审批。本轮只构建I2C_RESOURCE_PROBE_V1独立包，基于实际SI_HEALTH源副本，120秒/一次M0白名单读取、不写clock/reset/INTMUX/I2C。配套KO只在Linux CCF owner就绪及真实health往返后发诊断一次；原SI/冻结不变。Host通过并由主控被动安装，未执行诊断。下一实际cold进入/双UART/有限诊断/默认冷恢复由主控统一，人工断电不可替代为warm reboot。见RESOURCE_PROBE_HOST_PACKAGE，不开展sensor业务。

## 2026-10-05接线前里程碑完成

主控attempt4按最新授权完成一次无传感器诊断：M0十地址白名单只读status0，派生Linux owner/clock预检与真实health通过，诊断KO正常退出，用户冷恢复默认25文件hash PASS。现为WIRING_READY_FOR_USER，信号仍未接；等待用户断电接线确认，停止额外诊断/重启。完整sensor四等级与最终集成NOT_RUN，不把权限读诊断标业务HOST_PASS。实际结果见[脱敏JSON](../bringup/mpu6050/RESOURCE_PROBE_BOARD_RESULT.json)。


## 2026-10-05 用户断电接线确认（最新）

用户明确回复“已断电接线完成”，USER_CONFIRMED；SCL19/SDA23按既定表接入，电源2/14及风扇4/6、串口GND20保持。没有因此访问MPU或取得WHO/配置/样本证据。driver/sampler Host子里程碑见DRIVER_HOST_RESULT；下一步service/codec/epoch/订阅Host，不把未接control面的native FIT部署上板。


### 2026-10-05 Sensor service Host子里程碑（最新）

公共BE codec、独立0x3005 sensor endpoint、单订阅/租约/latest背压与READY worker、同health transport双owner有界退出完成；实际fixture sanitizer/native SCons/FIT验签及host_ci31/77/5通过。详见[SERVICE_HOST_RESULT](../bringup/mpu6050/SERVICE_HOST_RESULT.md)。用户断电接线USER_CONFIRMED；配套Linux sensor KO/rpmsg_srv/Core/Qt仍待实现，NOT_DEPLOYABLE，完整业务四等级与最终集成NOT_RUN，无本轮板端操作。下一步完成Linux桥及canonical/UI Host集成，不退回旧冻结tip或重复权限诊断。


### 2026-10-05 Linux/Core/Qt Host闭环（最新）

实际KO有界接口/ready ioctl与CCF持有、rpmsg_srv QUERY/epoch/订阅/freshness/正常退出、Core typed canonical校验与Qt六轴/芯片温度/状态/age/source完成，FakeI2C→实际driver/service→C++worker→Core→queuedQt闭环通过。host_ci34/79/5及适用sanitizer PASS：HOST_PASS，实板三个sensor等级与最终集成NOT_RUN。配套KO-v4精确vermagic/导出符号通过，MODVERSIONS=n/CRC=N/A；还未制作正式可部署sensor包。[结果/限制](../bringup/mpu6050/LINUX_CORE_QT_HOST_RESULT.md)。用户接线已确认，主控一次L1连接超时、未登录，当前启动身份UNKNOWN/UNREACHABLE，不沿用历史default快照。下步正式包须补一次WHO/关键读回与有限M0raw观测；本轮没有子代理板端操作。


### 2026-10-05 正式T1–T4产物（当前）

用户上电后主控L1确认当前默认6.1.99/rootp3boot2/RPMsg空、25文件beforehash通过。主控在独立用户目录实际AArch64编译v2（未运行应用/测试/传感器），M0 v7新增一次WHO/关键配置真实读回与100成功raw UART诊断，产品数据仍RPMsg；原生SCons/FIT验签和Host观测回归通过。exact10文件包v3、只被动安装器、finite两100样本/退订/health/卸载runner与冷恢复说明已可审阅，待主控review再操作，无子代理板端访问。见FORMAL_T1_T4.md/FORMAL_SENSOR_BUILD.json。driver/service/Linux/Core/Qt完整HOST_PASS保持，真实sensor三个等级/Qt人工/五分钟共存与最终等级NOT_RUN。


### 2026-10-05 真实T1–T4与冷恢复完成（当前）

主控真实WHO/config/100raw及两100 Linux样本通过，独立原始snapshot100/100字段一致；RTOS_SENSOR_PASS/RPMSG_SENSOR_PASS，T4正常匹配退订与卸载通过，默认25hash/bytes/links恢复通过。trace配置长行BSP截断由Host严格边界解析恢复100完整raw，缺失尾字段未重建、final日志NUL原样保留；实际订阅ID3作用域session内，未记录数字session限制。静止安装/陀螺X偏置、Qt人工、ES8323启动错误复核与五分钟共存仍未验证。用户任务本轮在T1–T4里程碑停止，不准备/执行新窗口，见SENSOR_T1_T4_RESULT.json。

## 2026-10-05 T5/T6准备（当前续步）

用户继续且无需额外审批，主控仍审核具体产物并唯一板操作。用户已重新固定模块；定义VALID+10..25秒静止功能容差，人工方向确认独立。native-v8短日志/四ARM ELF/独立package-v2已构建未运行，保留旧v3全部十文件。T6至少300秒实际全部链并行，严格进展/错误/资源门；默认codec startup已知-6不掩盖，同窗新增停止。Host CI34/97/5。执行和默认25+旧十保护恢复见FORMAL_T5_T6.md；当前不提升UI/最终PASS。

## 2026-10-06 独立errno诊断包装里程碑（L0续步）

既有Host诊断kernel v2实际完成并根审：release6.1.99-rk3576-audioprobe-d1，errno-only负日志，无clock/事务/重试改变。当前默认系统恢复实读PASS；音频冷启动原因UNKNOWN，RTP pacing未进入实板验证。本轮仅补exact stock Debian gzip/newc initrd配对及完整部署/撤回计划；实板NOT_RUN，子代理不部署/访问板。

复用p026_pair_initrd.py实际机制，不mkinitramfs：stockSHA425d2a68…5397、448成员、唯一cdc_eem KO；保留非module bytes/metadata/hooks/firmware/29links，模块路径/metadata改实际newstage，同newrelease检验。Host实现scripts/dev/package_audio_probe_initrd.py，60s/压缩32MiB/解压64MiB/4096成员；focused fixture测试格式/额外segment/路径/重复/special/保留字段/新KO内容。输出fresh task artifacts，仅小metadata/source/tests入Git。

本轮actual initrd6603260B/SHA53810e61…636658、448成员根独立审查PASS；stock非module保持，模块使用新native同release实际bytes。root只读确认boot仅12,574,720B，不足Image+initrd；旧v1计划停止。ROOT-p3派生v2曾遗漏内部partition变量，被root review拒绝并保留，不可执行。最终v3保留boot guard2与原FIT p2路径，新增rootfs guard3仅Linux Image/initrd/sensorDT从fresh /home/cat/cockpit/audio-probe-diagnostic-d1/boot读取；stage3696B/e70/SHA b1265098…26f8d根header/body/九项sourcehash审查PASS。完整模块实体安装/usr/lib/modules/newrelease；fresh拒覆盖、hash/readback、撤回精确命令见AUDIO_PROBE_DIAGNOSTIC_DEPLOY.md/JSON。新collector独立派生原SHA95f55…，只必要path/size/180s/600s变化；production PS5 Host AST/fakeport/顺序负例PASS，未Windows发布/未开串口。实板未部署、p3读取NOT_RUN、causeUNKNOWN；不改变M0/FIT/DT、原U-Boot、KO/业务或任何时钟。原正常shutdown后用户冷动作，再一次人工LOAD→INSPECT→SOURCE，失败停止无retry/reprobe。

最终Host CI35CTest/243Python/5withdrawal实际PASS、生产collector PS5 AST/fakeport另PASS；root独立v3九文件/CRC/body/partition/hash审查PASS。本轮source/manifest/明确cat传输与sudo-root安装和未知成员拒绝撤回命令已根最终审核；根独立18项audio-probe Python与Windows PS5生产fakeport PASS，板端和Windows发布均未执行。
