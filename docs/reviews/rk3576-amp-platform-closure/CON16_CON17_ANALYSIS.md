> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# BUS_M0 remap：定义已证，板端值未证

[RK3576 TRM v1.2 part 1 §8.6.2 表 8-6](https://rockchip.fr/Rockchip%20RK3576%20TRM%20V1.2%20Part1.pdf) 将 M0 `0x00000000–0x1fffffff` code window 经 `SYS_SGRF_SOC_CON16[31:10]<<10` 映射；M0 `0x20000000–0x3fffffff` shared DDR window 经 `SYS_SGRF_SOC_CON17[31:10]<<10` 映射，公式 `Linux PA = M0 address - 0x20000000 + base`，remap 在 MCU soft reset 后生效。TRM 表中 shared window 属性为 Normal WBWA，不能由 Cortex-M0 架构推断“天然无 cache”。

固定 HAL `lib/CMSIS/Device/RK3576/Include/soc.h:CODE_ADDR_MAP_START/SRAM_ADDR_MAP_START` 将 M0 视图 `0x46004060/0x46004064` 指向系统 PA `0x26004060/0x26004064`；`lib/hal/src/hal_cache.c:HAL_CACHE_GetCodeAddrBase()/HAL_CACHE_GetSRAMAddrBase()` 读 CON16/17。`HAL_CpuAddrToCacheAddr()` 进行 shared 地址转换。固定 HAL 寄存器定义是 **SOURCE_VERIFIED**，不是当前板读数。

[匹配 U-Boot 指纹的公开源码](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/arch/arm/mach-rockchip/rk3576/rk3576.c) `fit_standalone_release("bus_mcu")` 将 FIT load 作为 `MCU_CODE_START_ADDR` 经 `sip_smc_mcu_config()` 请求 BL31 设置 code base，然后写 `TOP_CRU_GATE_CON19`/`SOFTRST_CON19` release M0。未在该路径找到 CON17 写入。`arch_cpu_init()` 写的是 CON14/15 的 cache peripheral/uncache window **0x20000000、0x48200000**，不能误当 CON16/17。候选 RTOS startup 及 HAL 没有已证的 CON17 设置时序。

当前板 cmdline 只自报 `uboot-8f53f800da-04/24/2026`、`bl31-v1.14`；既不能证明此公开源码逐字编入，也不能证明当前 BL31 的 SMC/CON17 行为。**CON16 当前值、CON17 当前值、CON17 写入者/时序均 UNVERIFIED。** `REGISTER_READ_REQUIRED.md` 列审批后的只读证据需求；本轮未触碰寄存器。
