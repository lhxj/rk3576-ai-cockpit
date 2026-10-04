> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# Gate 2 — MBOX 通路

参考 Linux DTS 的 `rpmsg@47800000` 实为 **CPU3 link `0x03`**，`mboxes=<&mailbox0 0 &mailbox3 0>`，按 `mbox-names=rpmsg-rx,rpmsg-tx` 解析。其注释写 MCU link `0x04`，却未提供 MCU DTS 节点。不能将 mailbox3 用于 M0。

| 方向 | Linux controller / channel / IRQ | BUS M0 controller / channel / IRQ | 证据与状态 |
| --- | --- | --- | --- |
| M0 → Linux | 预期 mailbox0 ch0，Linux GIC SPI `0x7d`=125 | `MBOX0` ch0 TX | 参考 DTS mailbox0；HAL `soc.h:MBOX0`；`rpmsg_platform.c:platform_notify()` queue 0 走 master id 0。候选 M0 DTS **未建立**。 |
| Linux → M0 | 预期 mailbox4 ch0 TX，Linux GIC SPI `0x81`=129（controller 中断） | `MBOX4` ch0 RX，`MBOX_BB4_IRQn=175` | HAL `soc.h` 定义 175；本轮派生 port 增加 `mbox-clr4`。参考 DTS 实际写 mailbox3，**不一致**。 |

当前 LubanCat DTB：mailbox0 至 mailbox13 节点均存在，但 mailbox0/mailbox4 `status="disabled"`，无 RPMsg 节点。`MBOX_CNT=14`；原 port 仅显式初始化 `mbox_clr[0..3]`，link 4 会索引零初始化 client。本轮 Host 补 `mbox_clr[4]`，消除此确定的空映射；IRQ route、Linux MBOX4 与 BL31 路由尚需匹配板启动链。`platform_init_interrupt()` 对 link4 安装 `MBOX_BB4_IRQn`，注册 MBOX0/MBOX4 ch0；Linux driver 按 DTS 的 `rpmsg-rx/tx` 顺序取 channel。Linux/RPMsg 消息 `CMD=link-id`、`DATA=0x524D5347`；没有实板双向中断证据。

来源：派生 RTOS `.../porting/platform/RK3576/rpmsg_platform.c`；固定 HAL `lib/CMSIS/Device/RK3576/Include/soc.h`、`lib/hal/src/hal_mbox.c`；[Linux transport](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/drivers/rpmsg/rockchip_rpmsg_mbox.c)；当前板 DTB 只读反编译证据 `artifacts/local/amp-boot-files-20261001T120801Z-106653/`（本地忽略目录）。
