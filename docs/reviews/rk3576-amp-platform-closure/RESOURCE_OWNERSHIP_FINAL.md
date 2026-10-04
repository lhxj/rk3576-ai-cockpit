> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 最小 echo 的资源所有权裁决

`UNRESOLVED` 的 P0 条目阻止 D；表中目标值不是当前板已经转移的所有权。派生 defconfig 的 I2C3/6/7/8 全部关闭，不分配 MPU6050。

| Resource | Current owner | Proposed owner | Gate / evidence |
| --- | --- | --- | --- |
| BUS M0 | FIRMWARE，实际 release 状态未证 | RTOS 独占 | UNRESOLVED：实际 U-Boot/BL31 配置与 SMC |
| RTOS DDR code/data/heap/stack | 当前 LINUX 可分配 | RTOS 独占 reserved-memory | **P0 UNRESOLVED**：0x47800000 未预留且与 CPU3 参考冲突 |
| RPMsg shared DDR/rings/buffers | 当前 LINUX 可分配 | SHARED_CONTROLLED | **P0 UNRESOLVED**：CON17、PA、cache、DMA 属性 |
| MBOX0 | 当前 Linux DT disabled | SHARED_CONTROLLED，M0 TX/Linux RX | **P0 UNRESOLVED**：最终 DTS/IRQ |
| MBOX4 | 当前 Linux DT disabled | SHARED_CONTROLLED，Linux TX/M0 RX | **P0 UNRESOLVED**：最终 DTS/IRQ 175 路由 |
| UART5 / GPIO3_D4/D5 | 当前 Linux UART disabled | RTOS console | UNRESOLVED：v2 管脚、overlay/pinctrl 互斥 |
| TIMER11 / M0 SysTick | 当前 Linux timer owner 待核 | RTOS tick/system timer | **P0 UNRESOLVED**：HAL `SYS_TIMER=TIMER11`，Linux DTS timer11 节点/IRQ 未最终比对 |
| CRU clock/reset | FIRMWARE + Linux | FIRMWARE release，RTOS 仅已审 HAL init | **P0 UNRESOLVED**：U-Boot 写 `TOP_CRU_GATE_CON19/SOFTRST_CON19`；M0 `HAL_Init()`、UART/TIMER/MBOX 与 Linux 共同门控未闭合 |
| Power domain | FIRMWARE/Linux | SHARED_CONTROLLED 或 FIRMWARE | UNRESOLVED：M0 suspend/power policy |
| I2C3 | LINUX，RTC `hym8563@51` | 无 | LINUX；RTOS off，不抢占 |
| I2C6/7/8 | Linux DTS disabled | 无 | 当前无归属转移；RTOS off；未来另审 |
| 其他 GPIO/PWM/SPI | LINUX 或未分配 | 无 | echo 未启 demo，不转移 |

U-Boot 公开 release hook 写 `TOP_CRU_GATE_CON19=0x5c000000`、`TOP_CRU_SOFTRST_CON19=0x38000000`；M0 `board/evb/board.c:HAL_Init()`、`HAL_TIMER_SysTimerInit(TIMER11)` 与 SysTick 中断配置是源码证据。是否实际影响 Linux 使用的 shared clock/reset 仍需当前板设备树、CRU bitfield 和 boot 镜像确认，不能以“Host build PASS”代替资源隔离。
