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
   VCCIO3设计3.3V。PCB最终确认V2R0/20260521与图纸一致；再确认模块LDO/上拉/AD0电压、风扇供电预算，
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
