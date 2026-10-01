# 最小 echo 的资源所有权（当前 / 目标）

`UNRESOLVED` 表示上板前关键证据不足，不代表允许两个系统同时驱动。当前 RTOS 派生 defconfig 关闭 I2C/SPI/PWM/common demo；不分配 MPU6050 引脚。

| RESOURCE | LINUX OWNER | RTOS OWNER | SHARED | UNRESOLVED | EVIDENCE |
| --- | --- | --- | --- | --- | --- |
| BUS M0 CPU | Linux 无 remoteproc；U-Boot/BL31 控制 release | 目标独占 | 否 | 实板 U-Boot AMP 配置、reset 顺序 | cmdline；U-Boot `fit_standalone_release()` |
| DDR code `0x47800000+0x80000` | 现 DT 未 reserve，Linux 可能分配 | 目标独占 | 否 | **地址冲突/预留** | ITS、linker、当前 DTB |
| DDR RPMsg/vring/buffers | 现 DT 未 reserve | 目标共享 | 是 | **alias/PA/cache/size** | linker、参考 DTS、TRM |
| MCU SRAM | 无划分 | 固件未使用 | 否 | 物理地址/权限 | linker 未定义 SRAM |
| MBOX0 | DT disabled | 候选 TX | 是 | Linux RX enable / IRQ route | 当前 DTB；port |
| MBOX4 | DT disabled | 候选 RX IRQ175 | 是 | Linux TX enable / IRQ route | HAL `soc.h`；派生 port |
| UART5 GPIO3_D4/D5 | DT disabled，Linux console 为 ttyFIQ0/tty1 | 目标 M0 console | 否 | v2 40Pin、电压、overlay 互斥现场核 | 当前 DTB/uEnv；候选 iomux；厂商管脚表 |
| I2C3 | Linux `okay`，RTC `hym8563@51`，并与相机资源相关 | 无 | 否 | 禁止抢占 | 当前 DTB |
| I2C6 | DT disabled | 本 echo 无；未来候选 | 否 | pin/clock/外设核对 | 当前 DTB；生成配置全部 I2C off |
| I2C7 | DT disabled，40Pin overlay 目前注释 | 本 echo 无；未来候选 | 否 | pin/clock/外设核对 | 当前 DTB/uEnv；生成配置 I2C7 off |
| I2C8 | DT disabled | 本 echo 无；未来候选 | 否 | pin/clock/外设核对 | 当前 DTB；生成配置 I2C8 off |
| GPIO bank3 D4/D5 | Linux UART5 disabled | M0 UART5 m0 | 否 | 现有其他 overlay 对 pin 的动态影响 | 当前 DTB/uEnv；iomux |
| GPIO bank1 D5、SPI0/4、PWM0 | Linux 占用逐 pin 未清 | 派生 echo **不操作** | 否 | 后续业务再分配 | 派生 iomux 移除无条件 GPIO1_D5，defconfig 关闭 SPI/PWM |
| clock / reset | Linux/U-Boot/BL31 参与 CRU | M0 HAL 初始化 UART/timer/MBOX | 可能共同管理 | **时序/门控/互斥** | board.c、HAL CRU、U-Boot release |
| power domain | Linux PM driver / U-Boot | M0 无独立 PM 任务 | 可能共享 | **M0/MBOX 所属域与 suspend 策略** | 当前 DTB、HAL PM；未实测 |
| timer11 / SysTick | Linux timer11 节点状态未单独证明 | `SYS_TIMER=TIMER11`、M0 SysTick | 不应共享 timer11 | **IRQ/clock ownership** | `hal_conf.h`；`board.c` |

最小试验前关键 UNKNOWN：DDR/alias、MBOX IRQ、clock/reset/power、timer11 Linux 冲突、UART5 v2 物理接线。I2C6/7/8 只是未来可评估候选，不是本轮所有权转移。
