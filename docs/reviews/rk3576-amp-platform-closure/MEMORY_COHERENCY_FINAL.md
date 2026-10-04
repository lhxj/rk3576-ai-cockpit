> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# Coherency Gate：UNVERIFIED，三种准入方案均未证成

## P025/P023候选更新：UNCACHED_SHARED_MEMORY

已选择未来cold reset+全局Rockchip MCU cache bypass（不启用cache）、Linux ring Device-nGnRE与独占payload Normal-NC pool、device/DMA barriers/DSB+compiler barrier。实际M0 ELF/cache关闭、pool fail-closed、两侧排序链已经SOURCE_VERIFIED/HOST_TESTED；运行有效属性/SiP/reset仍UNVERIFIED，gate未开放。细节见 [P025](P025_CLOSURE_RESULT.md)。下文记录最初cache-on候选的历史缺口，不代表最新Host候选仍启用cache。

TRM §8.6.2 将 BUS_M0 shared window 列为 **Normal WBWA**；这不等于已证硬件 coherent。Cortex-M0 自身缺少传统 D-cache 指令，也不能推出 SoC 总线、Rockchip cache IP 与 shared DDR 一致。候选 `rtconfig.h:RT_USING_CACHE`、`hal_conf.h:HAL_DCACHE_MODULE_ENABLED`，HAL `hal_cache.c` 具有 cache 控制；`board.c:rt_hw_cpu_cache_init()` 的具体 enable 行为和 boot 固件预状态须一起审查。U-Boot 公开 `rk3576.c:arch_cpu_init()` 写 `SYS_SGRF_SOC_CON14=0x20000000`、`CON15=0x48200000`，注释称 BUS MCU uncache window 包括 RPMsg shared memory；这只是 **SOURCE_VERIFIED** 的候选代码，不是板端值或内存属性实测。

M0 `rpmsg_env_rt-thread.c` 的 `env_mb/rmb/wmb` 经 `MEM_BARRIER()` 展开 `dsb` 与 compiler `memory` clobber，可约束排序；但 `rpmsg_platform.c:platform_cache_all_flush_invalidate()` 是空函数，notify 前维护调用亦有注释。payload 写后→descriptor/avail 发布、consumer 读 ring→payload、used ring 回写前后，没有可证明的有效 clean/invalidate。Linux 参考 transport 对 vring 使用 `ioremap()`，DMA buffer 来自 `shared-dma-pool; no-map`；当前板无该节点，且尚未证对应内核源码逐字一致。

**不选** `UNCACHED_SHARED_MEMORY`、`HARDWARE_COHERENT` 或 `EXPLICIT_CACHE_MAINTENANCE` 任一最终方案。进入 D 需要：CON14/15/17 实际值与 TRM bitfield、M0 code/shared 实际 cache 属性、Linux vring 和 DMA payload mapping、cache IP 使能状态、两侧所有权转移屏障/维护代码证据。空 hook 不可当作 no-op 正当性。详见 `CON16_CON17_ANALYSIS.md` 与前轮 `MEMORY_COHERENCY.md`。
