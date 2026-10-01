# Gate 3 — cache / barrier

**结论：M0 shared DDR = UNVERIFIED，阻塞上板。** Cortex-M0 的 ARMv6-M ISA 不自带通用 D-cache 指令，但 RK3576 HAL 为 M0 编译了 SoC cache 控制器：`rtconfig.h:RT_USING_CACHE`、`hal_conf.h:HAL_DCACHE_MODULE_ENABLED`、`board.c:rt_hw_cpu_cache_init()`；不能从核心架构推断共享内存无缓存。

| 对象 | 代码路径 | 判定 |
| --- | --- | --- |
| M0 vring / descriptor / avail / used | `rpmsg_env_rt-thread.c:env_mb/rmb/wmb` 调 `MEM_BARRIER()`；GNU ARM 在 `rpmsg_compiler.h` 展开为带 `"memory"` clobber 的 `dsb`。 | barrier **有代码证据**；缓存属性 **UNVERIFIED**。 |
| M0 payload buffer | `rpmsg_platform.c:platform_cache_all_flush_invalidate()` 是空实现；`platform_notify()` 中维护调用被注释；`platform_patova()` 仅减 `0x20000000`。 | **UNVERIFIED**，若 cached 必须显式维护或证明硬件不缓存。 |
| Linux vring | Rockchip `rockchip_rpmsg_mbox.c:rk_rpmsg_find_vq()` 用 `ioremap()`、`memset_io()`、`vring_new_virtqueue()`，清 `RPMSG_CACHED_VRING`。 | 参考源码为 **MAPPED_UNCACHED**；实际板对应 kernel 版本未比对。 |
| Linux payload | 参考 DTS `shared-dma-pool; no-map` 并由 transport `of_reserved_mem_device_init()` 关联；具体 virtio buffer 分配映射需核对实际 kernel。 | **UNVERIFIED**，不能从 DTS 推断所有 buffer coherent。 |
| U-Boot / M0 总线属性 | 与板 boot cmdline 对应的 LubanCat U-Boot 源 `rk3576.c` 写 `SYS_SGRF_SOC_CON14=0x20000000`、`CON15=0x48200000`，注释称 BUS M0 uncache 窗。 | **条件证据**；未证明当前二进制执行此路径、CON17 指向何处、窗口作用地址视图。 |

待闭合：锁定实际 U-Boot/BL31 源与配置、读清 CON14/15/17 的寄存器语义、确定 `0x27d...` 对应 PA；逐条确认 M0 `RT_USING_CACHE` 的启用与 cache line 维护、Linux vring IO 映射和 DMA payload 属性。若需要维护，应在 descriptor、ring 和 payload 所有权转移前后按 cache line 对齐操作，保留 `dsb` 与 compiler barrier；不能仅在 mailbox 前做一次全局空 flush。来源：[Rockchip TRM 表 8-6](https://rockchip.fr/Rockchip%20RK3576%20TRM%20V1.2%20Part1.pdf)、候选 HAL `hal_cache.c`、[Linux transport](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/drivers/rpmsg/rockchip_rpmsg_mbox.c)。
