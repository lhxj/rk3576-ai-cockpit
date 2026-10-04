> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# Memory layout candidate：条件不足，暂不生成最终地址

本表记录已有**候选/参考**地址和未知量，不是部署布局。当前 CON17 值、实际写入者、Linux reserved-memory 可用范围、U-Boot/BL31 入口与 coherency 尚未闭合，因此没有证据允许选新的物理地址。半开区间 `[base, base+size)`。

| Region | Linux PA | M0 view | Size | 证据 / 状态 |
| --- | ---: | ---: | ---: | --- |
| 候选 RTOS text/rodata/data/bss/heap/stack 全区 | `0x47800000` | `0x00000000` | `0x80000` | 固定 linker `DDR ORIGIN=0`, FIT `load`；仅候选，当前 DT 未 reserve |
| 候选 M0 effective vector/entry | 对应 code base + `0` | `0x00000000` vector / ELF `Reset_Handler` | — | 候选 U-Boot `fit_standalone_release()` 请求映射 FIT load；板端 CON16/SMC 未证 |
| 候选 shared DDR 全区 | `CON17_base + 0x07d00000`，**未知** | `0x27d00000` | `0x400000` | 固定 linker `LINUX_RPMSG`，TRM 转换；CON17 当前值未知 |
| 候选 vring0 | `CON17_base + 0x07d00000`，**未知** | `0x27d00000` | `0x8000` | RPMsg-Lite stride；不能直接当 Linux PA |
| 候选 vring1 | `CON17_base + 0x07d08000`，**未知** | `0x27d08000` | `0x8000` | RPMsg-Lite stride |
| 候选 buffer pool | **未知** | **待双方 allocator/layout 核对** | 64 buffers/方向、512 B slot 是配置量 | 不由 CON17 单独确定 |
| **CPU3 参考** vring0 | `0x47800000` | M0 对应值未知 | `0x8000` | Rockchip CPU3/link3 DTS；若与候选 RTOS 合用则物理重叠 |
| **CPU3 参考** vring1 | `0x47808000` | M0 对应值未知 | `0x8000` | 非 M0 最终地址 |
| **CPU3 参考** RPMsg DMA | `0x47a00000` | M0 对应值未知 | `0x200000` | 非 M0 最终地址 |

`0x47800000` 冲突已定性：FIT load 是**实际 payload 物理落点**；CPU3 参考 DTS 的 vring0 是 Linux PA，原样合并同一个物理位置。它不是 M0 alias 造成的假冲突；当前板未启 AMP，未发生运行时重叠。候选 M0 code base 也不是最终获准 load：当前 Linux DT 没为其保留内存，且当前 loader 未证可用。

后续只有在当前 CON16/17 或等价证据、实际启动链、RAM/reserved-memory map 与 DMA/cache 机制齐备后，才可计算新的 RTOS load、M0 effective entry、Linux vring0/1 与 buffer PA；再统一修改 contract、linker、ITS、LubanCat AMP DTS 并运行无重叠检查。本轮**没有修改**这些文件。
