> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# RPMsg link-id：M0 目标 0x04，Linux 当前未配置

固定 HAL `middleware/rpmsg-lite/lib/include/platform/RK3576/rpmsg_platform.h` 定义 `RL_PLATFORM_SET_LINK_ID(master,remote)=((master<<4)&0xf0)|(remote&0xf)`，`RL_GET_M_CPU_ID` 和 `RL_GET_R_CPU_ID` 分别取高/低 nibble；`RL_GET_VQ_ID` 用 `link_id<<1|queue_id`。派生 `applications/amp_echo.c` 明确 `RL_PLATFORM_SET_LINK_ID(0U,4U)`，即 master 0、remote 4，M0 侧调用 `rpmsg_lite_remote_init()`。

[Rockchip 参考 AMP DTS](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/arch/arm64/boot/dts/rockchip/rk3576-amp.dtsi) 明文注释 `CPU3: link-id 0x03; MCU: link-id 0x04`，却将启用节点设为 **0x03**、TX MBOX3。这说明该已启用节点用于 CPU3，不可用于本 BUS_M0。Linux [Rockchip transport](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/drivers/rpmsg/rockchip_rpmsg_mbox.c) 读取 `rockchip,link-id`，不能由 M0 固件单方覆盖。当前运行 DT 无 RPMsg 节点。

| Link | 源码含义 | 当前证据等级 |
| --- | --- | --- |
| `0x03` | master 0 ↔ remote 3；参考 DTS 明注 CPU3 | SOURCE_VERIFIED；非本目标 |
| `0x04` | master 0 ↔ remote 4；参考注释 MCU，派生 M0 echo 采用 | SOURCE_VERIFIED；Linux M0 节点尚无 |

最终 M0 专用 Linux DTS 必须给 `rockchip,link-id=<0x04>` 并匹配 TX/RX MBOX；在最终 DTS Host 编译、运行板配置以及 CON17 闭合前，**双侧一致性 BLOCKED**。不能把参考 DTS 的 0x03 直接改值而保留 MBOX3。
