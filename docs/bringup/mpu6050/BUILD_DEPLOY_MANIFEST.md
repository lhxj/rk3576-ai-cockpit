# MPU_SENSOR_V1 构建/部署清单草案

2026-10-05。**APPROVAL_PACKET_NOT_READY / NOT_BUILT / NOT_DEPLOYED**。
本文件列待审批变更，尚不请求执行L2/L3。实际固件/KO构建和资源审查完成后才输出
`APPROVAL_REQUIRED_BOARD_CHANGE`精确包；当前没有目标hash，不能拿本提示词代替审批。

## 保留基础与派生入口

[10项冻结产物和7项SI_HEALTH文件](preserved-assets.json)独立登记；10项重新hash
均与冻结manifest一致。源码、原最小AMP与SI_HEALTH_V1产物未改写/删除/覆盖。
RTOS 4.1.1/BUS M0：import8541f7ad0c584469cc6dd725b6e489d1e9d29e4a，
派生3a39b0f4f5eb25631d05b4ffd88746ce9b1b0cdb，再0010-M0至0015/v5；
HAL import277de3fd4b0e640654ee73bb3308be2ef01e3aad，派生
bc99978c1a030ad79610e89a5780dcd0ee3bb1f2。Linux
521833e2d28decbd6473d5717f1f96cc4108e208+0003 uncached pool补丁，
release `6.1.99-rk3576-m0echo-p026`。

Host新副本建议在任务worktree忽略`artifacts/local/mpu-sensor-v1/`内创建，
固定archive基线、按顺序补丁、HAL symlink定向核验；不修改原SDK工作树。
独立MPU_SENSOR_V1 defconfig通过SCons官方`--useconfig`生成rtconfig，
`RTT_ROOT=<fresh-source> RTT_EXEC_PATH=<pinned-arm-none-eabi>`，
在`bsp/rockchip/rk3576-mcu`运行`scons --useconfig=.config`再`scons -j4`。
应登记完整实际argv、GCC版本、patch系列hash、配置输入/输出hash与build log。
这些是拟用原生入口，不是本轮已执行命令。

ELF/readelf/map/nm必须确认load0x47800000、M0 entry0x141、代码区域0x80000及
原vring/pool保持；每个新线程stack/优先级、heap/静态/全局缓冲预算另列实际数值。
原BIN125704B不代表新映像仍能放下；拟stack/queue值要由map和最坏路径计算，
后续板端高水位验证。FIT使用现有受信签名链/公钥controlDT核验，不复制私钥。
不改U-Boot/CON16/17/共享内存/mailbox/link4/vring及默认恢复入口。

sensor KO在独立Kbuild目录构建：`make -C <paired-source> O=<fresh-prepared-build>
M=<sensor-ko-dir> ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules`。
必须使用配套.config/Module.symvers/kernel.release，核AArch64/vermagic/CRC与依赖；
不能默认#8加载paired KO，也不能force load。若审查发现必须改kernel则重新构建
完整Image/modules/initrd配套组与审批，不混用stock模块。

## 待批准的文件与资源变更

目标路径为**草案**，首次安装须读回不存在并exclusive-create，不覆盖已有项。

| 文件/对象 | 当前/hash | 拟安装位置 | 目标/hash |
|---|---|---|---|
| MPU_SENSOR_V1 ELF/BIN/map/defconfig/config/patch manifest | 新派生，未生成 | Host忽略目录 | NOT_BUILT / null |
| sensor signed FIT | 新路径应不存在，部署前核验 | /boot/amp-p029/mpu-sensor-v1/amp-signed.itb | NOT_BUILT / null |
| sensor ownership DTB（保留原CAM0 overlay） | 新路径应不存在 | /boot/amp-p029/mpu-sensor-v1/stage-sensor.dtb | NOT_BUILT / null |
| C入口cmd/scr（保留文件加载/签名/检查） | 新路径应不存在 | /boot/amp-p029/mpu-sensor-v1/stage-sensor.cmd/.scr | NOT_BUILT / null |
| sensor binding KO | 新路径应不存在 | /boot/amp-p029/mpu-sensor-v1/rk3576_sensor_v1.ko | NOT_BUILT / null |
| 已有health KO/新配套health（按最终endpoint契约选择） | SI_HEALTH hash见保留清单 | 新目录独立副本或只读引用，未定 | NOT_BUILT / null |
| preflight/manifest/SHA256SUMS/安装器 | 新源码，未生成 | 同新目录及用户staging | NOT_BUILT / null |
| Linux应用构建/二进制 | 第一轮未改产品 | /home/cat/cockpit/mpu-sensor-v1/（候选） | NOT_BUILT / null |
| paired Image/initrd/modules | 原配套身份已核 | /boot/amp-p029/Image、initrd（拟只读复用） | 是否可复用待KO/DT审查；不默认可用 |

Linux DT：i2c9 disabled保持，无子设备；mcu-amp增加CLK_I2C9/PCLK_I2C9、
独立xin24m parent/rate与i2c9m1 pinctrl，保留UART5与既有clocks。
RTOS：延迟注册I2C9/HAL、clock保持模式、受控reset和sensor_task；禁止PREV阶段访问，
去掉仅启用I2C9时无条件I2C7 mux，19/23 function10由Linux pinctrl单一配置。
初始化返回错误/timeout单位修正与就绪门必须通过Host验证，详见ownership续审。
不能仅改Linux status而遗漏clock late-disable，不能停用CAM0/codec/RTC/PMIC所在总线。
security/power-domain若需新变更，必须先提供具体寄存器/权限来源及影响，另列hash/命令。
目前不计划盲目改SGRF，不新增GPIO/PWM风扇控制。

## 拟审批测试窗口（最终包需按构建结果再定案）

单次新派生AMP冷启动与单次sensor KO加载、T0–T5有限100组、T6至少300秒共存。
拟整个板端程序窗口≤1200秒，外层timeout1240秒；每阶段内部截止，采样20Hz、
发布≤20Hz、单租约≤5000ms（最终范围由协议测试固定），无无限retry/queue。
每路UART≤2MiB、普通日志总≤16MiB、仅有限raw表100行；日志达到上限即停止。
准确命令/时长/日志路径/图形session/程序PID/资源停止步骤在最终审批包中给出。

停止条件：WHO_AM_I/读回不符、访问/clock/reset异常、I2C/RPMsg错误风暴、
health timeout、endpoint错绑、重复owner、record/audio overflow、关键媒体回归、
持续资源增长、无法正常退出或超时。停止增加负载，留证据，不重启M0/强制加载KO/
重复探测/危险拔插；按获批冷恢复流程处理。

## 冷启动与撤回草案

安装前捕获实时默认与冻结文件hash，包括默认Image/initrd/DT/uEnv/boot.cmd/scr、
冻结C目录、SI_HEALTH目录；新包只加独立目录，不改默认启动配置。
双UART有界保存；获批正常关机、用户完整冷断电上电，在现有U-Boot显式加载新
SCRIPT，核fileaddr/filesize/hash/controlDT/签名后一次source；不saveenv。
没有精确SCR字节大小与目标hash时不提供可执行启动命令。

退出：停止订阅/续租→停Linux sensor接收/应用并join→确认M0无订阅（发送停止/lease
到期）且health可响应→保存指标→获批正常关机→用户冷断电再走默认入口。
核默认#8/root p3/boot p2、无marker、RPMsg0、默认/冻结全部hash与前一致，设备释放。
独立新目录被动保留便于取证；不自动删除/改链接/重刷；不把强制断电称正常退出。
窗口外任何上传/KO/boot/重启操作都需相应批准。

## 2026-10-05 适配诊断构建（不能部署）

[I2C9_ADAPTER_BUILD.json](I2C9_ADAPTER_BUILD.json) 记录派生补丁、配置、ELF/map/bin hash 与内存预算；[I2C9_BSP_FIX.md](I2C9_BSP_FIX.md) 记录 Host 与原生构建结果。它只有保留 deferred API 的资源适配诊断配置，不是 MPU_SENSOR_V1 业务固件，未生成 FIT/DT/KO，也没有安装目标 hash。原有 `APPROVAL_PACKET_NOT_READY` 保持，不输出 `APPROVAL_REQUIRED_BOARD_CHANGE` 的可执行包。

## 2026-10-05 接线前派生配置包（不能部署）

[PREWIRE_RESOURCE_RESULT.json](PREWIRE_RESOURCE_RESULT.json) 记录顺序0001/0002/0003、DT源码/固定kernel输入hash、编译后DT逐项对比、fresh ELF/map/bin/config hash和内存预算。DT编译完成仅为资源提案；诊断固件仍不含 MPU_SENSOR_V1 业务或 FIT，未构建新 kernel/KO/启动脚本，缺少可执行部署审批包。本轮无上传/安装/重启；原冻结与默认产物保持。实际 BL31 权限与共享 INTMUX 初始状态的有限无传感器验证提案见结果文档，只准备，不执行。
