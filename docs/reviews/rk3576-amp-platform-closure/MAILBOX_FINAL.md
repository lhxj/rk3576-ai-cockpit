# Mailbox Gate 裁决：目标方向有据，最终板级链未闭合

| Side | Direction | Controller | Channel | IRQ | Source / status |
| --- | --- | --- | ---: | ---: | --- |
| M0 | TX → Linux | MBOX0 | 0 | M0 TX 非接收 IRQ | 派生 `rpmsg_platform.c:platform_notify()`、HAL `soc.h`；SOURCE_VERIFIED |
| Linux | RX ← M0 | `mailbox0` | 0 | GIC SPI 125 | 当前板 DTB `mailbox@2ae50000: interrupts=<0 0x7d 4>`，disabled；角色目标待 DTS，BLOCKED |
| Linux | TX → M0 | **目标 `mailbox4`** | 0 | GIC SPI 129 | 当前板 DTB `mailbox@2ae54000: interrupts=<0 0x81 4>`，disabled；M0 专用 consumer 尚无，BLOCKED |
| M0 | RX ← Linux | MBOX4 | 0 | `MBOX_BB4_IRQn=175` | 固定 HAL `soc.h`、派生 `mbox-clr4` 与 `platform_init_interrupt()`；SOURCE_VERIFIED |

Rockchip CPU3 参考节点是 `mboxes=<&mailbox0 0 &mailbox3 0>`、`mbox-names="rpmsg-rx","rpmsg-tx"`，因此 Linux TX 实际为 MBOX3，不能用于 M0 link4。M0 port 的回调类型和 `mbox-clr4` 已在前轮派生修复并 Host 编入；注册 `HAL_MBOX_Init/RegisterClient`，M0 接收 IRQ 175。Linux transport 通过 DT mailbox consumer 获取 TX/RX；当前 Linux DT 未启 MBOX0/4，无法做双向互证。controller 的 A/B 方向、GIC IRQ、安全路由与当前板实值仍待最终 DTS 和 boot 证据。
