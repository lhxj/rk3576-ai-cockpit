> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# RPMsg mailbox/link 候选资源

本轮冻结 **RPMSG_RESOURCE_CANDIDATE**，不是板端生效配置。M0 派生 echo 调 `rpmsg_lite_remote_init(..., 0x04)`；候选 `rpmsg_platform.c` 的 notify 用 MBOX0 TX，MBOX4 channel 0 接收与 `MBOX_BB4_IRQn=175`。固定 HAL `soc.h` 与平台 port、前轮 [MAILBOX_FINAL.md](../rk3576-amp-platform-closure/MAILBOX_FINAL.md) 给源码证据。Linux 目标应把 `rpmsg-rx` 指向 `mailbox0` channel 0、GIC SPI125，把 `rpmsg-tx` 指向 `mailbox4` channel 0、GIC SPI129；这样 M0 TX 对 Linux RX，Linux TX 对 M0 RX。

| Side | 方向 | Controller / channel | IRQ | 证据等级 |
| --- | --- | --- | --- | --- |
| M0 | TX → Linux | MBOX0 / 0 | TX 无接收 IRQ | SOURCE_VERIFIED（派生 port） |
| Linux | RX ← M0 | mailbox0 / 0 | GIC SPI125 | 当前 DT controller 地址/IRQ BOARD_OBSERVED_READONLY；consumer 未部署 |
| Linux | TX → M0 | mailbox4 / 0 | GIC SPI129 | 目标匹配 SOURCE_VERIFIED；当前 controller disabled |
| M0 | RX ← Linux | MBOX4 / 0 | M0 IRQ175 | SOURCE_VERIFIED（HAL + port） |

Rockchip `rk3576-amp.dtsi` CPU3/link `0x03` 参考节点使用 MBOX3 TX，**不能移植成 M0/link `0x04` 的 Linux 侧配置**。当前板 DT 没有 AMP/RPMsg consumer，MBOX0/4 为 disabled，故中断路由、secure ownership 和 Linux transport 实际 probe 仍 UNVERIFIED。M0 shared base 候选 `0x27d00000` 是 M0 视图；没有 CON17 无法冻结 Linux vring0/1 或 buffer pool PA，不能启动 link。前轮 echo Name Service `rk3576-m0-echo` 与专用 Linux test driver ID 静态匹配，Host Kbuild 通过；本轮没有板端 service announce。
