# Vring/payload coherency 裁决

**UNVERIFIED；目前不能在 A `UNCACHED_SHARED_MEMORY`、B `HARDWARE_COHERENT`、C `EXPLICIT_CACHE_MAINTENANCE` 中作选择。** 这是进入 D 的独立 blocker，不由 CON17 数值自动解决。

- [TRM v1.2 §8.6.2](https://rockchip.fr/Rockchip%20RK3576%20TRM%20V1.2%20Part1.pdf) 标记 BUS M0 shared window 为 `Normal WBWA`，不是 uncached 证明。候选 RTOS `RT_USING_CACHE`、HAL `HAL_DCACHE_MODULE_ENABLED` 启用 cache 相关模块。Cortex-M0 的 CPU 级 D-cache 特性不能代替 SoC bus/cache/window 属性判断。
- 固定/派生 RPMsg-Lite environment 的 `MEM_BARRIER()` 具 `dsb` 与 compiler barrier，能约束顺序；`rpmsg_platform.c:platform_cache_all_flush_invalidate()` 仍为空实现，无法证明 vring descriptor、avail/used 与 payload 所有权转换时的 clean/invalidate。见前轮 [MEMORY_COHERENCY_FINAL.md](../rk3576-amp-platform-closure/MEMORY_COHERENCY_FINAL.md)。
- [公开 U-Boot `rk3576.c:340-341`](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/arch/arm/mach-rockchip/rk3576/rk3576.c) 写 CON14/15，注释涉及 BUS MCU uncache window；当前板是否运行该二进制及实际寄存器值未证，且 CON14/15 不等于 CON17。Linux 参考 transport 的 vring `ioremap()` 与 DMA buffer/no-map 行为还需和当前板匹配的内核二进制、目标 DT 一起核查。

要选择 A，须证实实际 M0/shared window 与 Linux 对 vring/payload 的全部访问都不缓存；要选择 B，须证实端到端硬件 coherency；要选择 C，须落实并 Host 核查双侧针对 ring/payload 的 clean/invalidate 与屏障位置。当前均缺证据。**不启用 RPMsg link**。

## 2026-10-02：用户原始 TRM 补充核验

已直接读取用户桌面的 Part1/Part2；文件 SHA256、页码与原文核验见 [TRM_SOURCE_RECONCILIATION.md](TRM_SOURCE_RECONCILIATION.md)。Part1 p750 §8.1/表 8-1 明确列 BUS MCU 的 **16 KB unified I/D cache、32-byte line**；p765 §8.6.4–5 给 cache reset bypass、软件初始化和 maintenance 流程。不能以 Cortex-M0 核心本身的 cache 特性代替此子系统设计，也不能由 reset bypass 直接推出启动完成后仍 bypass。此证据为 **SOURCE_VERIFIED**，不是 cache 当前状态的实板读数。

Part1 p766 §8.6.8/表 8-14 指明 BUS MCU 的非缓存 peripheral space 由 **CON14/15** 定义；这使“建立明确 uncached/bypass 方案”具有硬件文档依据。其地址范围、生效状态及候选 Linux mapping 尚未闭合，故不升级为 `MAPPED_UNCACHED`。固定 HAL `HAL_DCACHE_Disable()` 会设 cache bypass，`HAL_DCACHE_Enable()` 初始化后清 bypass；该接口行为不是“目前已禁用”的证明，也不能随意把已有脏数据的动态 bypass 当成完整维护流程。

Part1 p772 的 CCI500 功能没有给 BUS MCU 与 A 核 DDR 的端到端一致性保证；p770 的 SHRM 描述针对 System SRAM，不能用于证明 DDR sharing 一定 coherent 或 non-coherent。**最终仍 UNVERIFIED**；下一步应完成 bypass/uncached 或 maintenance 的具体方案和双侧源码审查。
