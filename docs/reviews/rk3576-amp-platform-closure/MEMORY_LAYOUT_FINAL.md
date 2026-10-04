> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# Memory Gate 裁决：BLOCKED

## P025/P023新增证据

已形成并Host检验了无重叠**拟配置**图：codePA47800000、ringsPA47d00000/47d08000、poolPA47d10000。它要求未来cold reset及checked B16/B17 setter，不是当前CON读数。section/heap/stack见 [Host图](MEMORY_LAYOUT_HOST_PROPOSAL.md)，本轮重复9,672项验证PASS。恢复后的运行DT/boot备份已核相同hash；U-Boot复制前完整RTOS/shared内存保护仍需解决，不能由Linux no-map替代。详见 [P025](P025_CLOSURE_RESULT.md)。以下为历史布局，旧bin/地址不得混用。

0x47800000 的结论为 **A：Rockchip `rk3576-amp.dtsi` 是 CPU3/link3 参考布局，不适用于本 M0/link4 方案原样合并**；若原样合并，它与候选 FIT 实际 load **物理重叠**，不是 alias 或临时缓冲。证据：`rk3576-mcu/Image/amp.its:load`、[U-Boot `amp_cpus_on()` 的 `boot_get_loadable()`](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/drivers/cpu/rockchip_amp.c)、[参考 DTS 注释及节点](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/arch/arm64/boot/dts/rockchip/rk3576-amp.dtsi)。当前板 DT 没有 AMP/RPMsg 节点，运行时尚无该冲突。**冲突已解释，尚未设计并验证新的无冲突布局。**

以下是候选/参考对照，**不是最终可部署内存图**。半开区间；`?` 代表不能映射到当前板 PA。

| Region | Linux PA | M0 Address | Size | Owner | Attr | Purpose / evidence |
| --- | ---: | ---: | ---: | --- | --- | --- |
| 候选 RTOS 全 512 KiB code/data/heap/stack | `0x47800000` | `0x00000000` | `0x80000` | M0 目标独占 | code window，当前板未 reserve | linker `DDR` + ITS load；本轮 ELF file size `0x21058` |
| vectors/text/rodata | code region 内 | `0x0` 起 | 由本轮 ELF/map 记录 | M0 | 待证 | ELF LOAD、`.vectors/.text` |
| `.data/.bss` | code region 内 | `0x20600/0x21080` 起 | `0x988/0x34d4` | M0 | 待证 | 前轮 readelf；本轮重建见 Host 报告 |
| heap/stack | code region 内 | `0x24554/0x7fc00` 起 | `0x5b6ac/0x400` | M0 | 待证 | linker/map；入口 vector 独立于 FIT physical load |
| **CPU3 参考** vring0 | `0x47800000` | ? | `0x8000` | CPU3/Linux 参考 | no-map | 与 RTOS code 冲突 |
| **CPU3 参考** vring1 | `0x47808000` | ? | `0x8000` | CPU3/Linux 参考 | no-map | 与 RTOS code 冲突 |
| **CPU3 参考** RPMsg DMA | `0x47a00000` | ? | `0x200000` | CPU3/Linux 参考 | shared-dma-pool/no-map | 非 M0 最终配置 |
| 候选 M0 shared/ring0 | ? | `0x27d00000` | `0x400000` shared, ring `0x8000` | M0/Linux 目标共享 | **UNVERIFIED** | linker `LINUX_RPMSG`，CON17 base 未知 |
| 候选 M0 ring1 | ? | `0x27d08000` | `0x8000` | M0/Linux | **UNVERIFIED** | ring stride `VRING_SIZE` |
| 候选 buffer pool | ? | ? | 64×每向×512 B 最低需求 | M0/Linux | **UNVERIFIED** | RPMsg config；Linux DMA PA/CON17 未闭合 |
| MCU SRAM | 未分配 | 未分配 | 未分配 | 无 | N/A | linker 未在 SRAM 放节 |
| 当前 Linux DDR | 当前系统管理 | N/A | 4 GiB 总容量 | Linux | DT 默认 | 当前 DT 仅 ramoops AMP 无 reserve |

不能因为候选 code 起点看似可用，就沿用 `0x47800000`：当前 DT 不保留该段，Linux 可分配。最终 code、共享 DDR、vring、buffer PA 只有在当前 CON16/17、boot 与 DMA/cache 路径证实后才能填入 contract；必须 Host 检查半开物理区间和现有 reserved-memory 无 overlap。**最终 load/entry = UNRESOLVED。**
