> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# BUS M0 CON16/CON17：定义与当前板证据

## SOURCE_VERIFIED：寄存器及映射

[RK3576 TRM v1.2 part 1 §8.6.2 表 8-6](https://rockchip.fr/Rockchip%20RK3576%20TRM%20V1.2%20Part1.pdf) 的 BUS_MCU 两个 512 MiB 窗口：

| M0 地址窗口 | 系统物理基址字段 | 地址公式 | TRM 属性 |
| --- | --- | --- | --- |
| `0x00000000–0x1fffffff` code | `SYS_SGRF_SOC_CON16[31:10] << 10` | Linux PA = M0 code offset + code base | Normal WT |
| `0x20000000–0x3fffffff` shared | `SYS_SGRF_SOC_CON17[31:10] << 10` | Linux PA = M0 address − `0x20000000` + shared base | Normal WBWA |

TRM 指出 remap 在 MCU soft reset 后生效。**TRM 本身未提供 CON16/17 的 offset/detail；`+0x60/+0x64` 的来源是固定 HAL。** 固定 HAL `rk3576-hal/lib/CMSIS/Device/RK3576/Include/soc.h:651-652` 在 `HAL_BUS_MCU_CORE` 分支给 M0 寄存器视图 `CODE_ADDR_MAP_START=0x46004060`、`SRAM_ADDR_MAP_START=0x46004064`。结合 TRM 外设窗口地址译码，源码定义的系统物理地址为 `0x26004060`（CON16）、`0x26004064`（CON17）。`lib/hal/src/hal_cache.c:HAL_CACHE_GetCodeAddrBase()/HAL_CACHE_GetSRAMAddrBase()` **读取**两者，不能据此断言它们曾被写为某值。

[与 cmdline 短哈希对应的公开 U-Boot 源 `rk3576.c:255-275`](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/arch/arm/mach-rockchip/rk3576/rk3576.c) 在 `fit_standalone_release("bus_mcu", entry_point)` 调 `sip_smc_mcu_config(BUSMCU_0_ID, MCU_CODE_START_ADDR, entry_point)` 请求 BL31 设置 code 映射，随后解除 BUS M0 gate/reset。`arch_cpu_init():340-341` 写的是 **CON14/15**，数值 `0x20000000`、`0x48200000`，不是 CON16/17。此 U-Boot、固定 HAL 与候选 M0 BSP 已审路径均无已证 CON17 写入者。BL31 当前二进制未取得，不能排除其或更早固件设置。

## BOARD_OBSERVED_READONLY 与缺口

当前 `/proc/cmdline` 只报 `ddr-v1.09-2f85f4b2d4,bl31-v1.14,bl32-v1.05,uboot-8f53f800da-04/24/2026`。普通 `/boot` 文件、`/proc`、`/sys/firmware` 与有界 `dmesg` 未提供 CON16/17 当前值或写入时序。**CON16、CON17 当前值及 CON17 最终写入者 = UNVERIFIED。** 版本指纹不能替代寄存器读数或实际 BL31 镜像分析。

候选 M0 RPMsg base `0x27d00000` 在 M0 shared 窗口中，因此只有 `CON17_base` 已知才能算 `Linux_PA = 0x07d00000 + CON17_base`。不能把 `0x27d00000` 直接当 Linux PA。尚未在板上执行任何 MMIO 读、U-Boot console 或 MCU 启动；需要特殊访问时严格按 [APPROVAL_REQUIRED_READ.md](APPROVAL_REQUIRED_READ.md) 等待批准。

## 2026-10-02 后续证据（上述未读取状态为历史）

用户后续授权后，Linux CON16 直接读取触发 SIGBUS，当前 SiP 只读服务对 CON16/17 均返回 INVALID_ADDRESS，见 [CON16_CON17_RUNTIME_DIAGNOSTICS.md](CON16_CON17_RUNTIME_DIAGNOSTICS.md)。当前 BL31 镜像亦已只读取得，但不能由镜像中的地址表得出 CON17 运行值。两个值仍 BLOCKED。

用户提供两份原始 TRM 的全文/页面核对确认：基址与映射字段在 TRM，offset 在固定 HAL，读属性/复位值与 runtime mapping 仍缺。详细证据归属见 [TRM_SOURCE_RECONCILIATION.md](TRM_SOURCE_RECONCILIATION.md)。
