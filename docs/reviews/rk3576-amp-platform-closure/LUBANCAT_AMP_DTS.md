> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# LubanCat AMP DTS：未生成最终 DTB，Gate BLOCKED

当前运行兼容串为 `embedfire,rk3576-lubancat-3-v2`、`rockchip,rk3576`；`/boot/dtb/rk3576-lubancat-3-v2.dtb` 已只读 hash `76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90`。当前 DT 没 AMP/RPMsg reserved-memory/transport，MBOX0/4 和 UART5 disabled。Rockchip `rk3576-amp.dtsi` 是 CPU3 link3/MBOX3/UART5 m2 参考，不能 include 到 LubanCat v2 或只换 link-id；它还在 RTOS code 0x47800000 放 vring0。

计划的 M0 专用 `rk3576-lubancat-3-v2-amp.dtsi` 必须从唯一 contract 生成/检查：RTOS code/data 独占 reserved-memory，vring 与 DMA pool/no-map，`rockchip,amp` 的 BUS_M0 clock/IRQ/pinctrl 逐项最小化，RPMsg `link-id=0x04`、Linux RX MBOX0/TX MBOX4 ch0；保留当前 camera/audio/Wi-Fi/display 等板节点。CON17、cache、U-Boot clock/reset 与 UART5 物理互斥未闭合，**不能填写最终 PA、编译部署 DTB**。因此 “LubanCat AMP DTS Host build PASS” = **否**，不是 `dtc` 工具缺失。
