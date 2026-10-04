> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# RK3576 BUS M0 CON16/CON17：访问语义门禁

**裁决：`READ_GATE_BLOCKED`。** 本轮仅做 Host 静态取证；未连接开发板，未读取 MMIO。固定基线：HAL `277de3fd4b0e640654ee73bb3308be2ef01e3aad`，LubanCat U-Boot `8f53f800da2c25d0c6ba414fb45902a01675703a`。公开 TF-A 取证版本为 `58bd918dce1e2831fbec252c0b2139d519a00d55`，**未证明等同于当前板 `bl31-v1.14`**。

## 寄存器归属与 TRM 范围

[RK3576 TRM v1.2 part1 §1.1、§8.6.2 表 8-6](https://rockchip.fr/Rockchip%20RK3576%20TRM%20V1.2%20Part1.pdf) 将 `0x26004000–0x26004fff` 列为 **SYS_SGRF**；BUS MCU code/shared remap 字段分别是 `SYS_SGRF_SOC_CON16[31:10]` 与 `SYS_SGRF_SOC_CON17[31:10]`。固定 HAL [`soc.h:650-652`](https://github.com/starwey604/rk3576-hal/blob/277de3fd4b0e640654ee73bb3308be2ef01e3aad/lib/CMSIS/Device/RK3576/Include/soc.h#L650-L652) 的 M0 alias `0x46004060/64` 对应系统物理地址 **`0x26004060/64`**。**SOURCE_VERIFIED**。

TRM 此表说明的是 **M0 窗口的 memory attribute**（code Normal WT、shared Normal WBWA），并非 CON16/17 自身的寄存器读属性。已核查 part1 的 SYS_SGRF 出现位置及相关 block 说明，未找到这两个寄存器的 `RO/RW/WO`、read-clear、read-latch、读取副作用或统一 SYS_SGRF 读语义。TRM 的普通 `SYS_GRF` 寄存器说明属于 `0x2600a000` 的另一个 block，不能移用。**UNVERIFIED**；“未找到”不等于“确定没有读取副作用”。

## 源码证据

| 来源、执行级 | 精确路径 / 函数 | 看到的行为 | 证据等级 |
| --- | --- | --- | --- |
| 固定 Rockchip HAL；BUS M0 | [`hal_cache.c:60-65,123-131,146-180`](https://github.com/starwey604/rk3576-hal/blob/277de3fd4b0e640654ee73bb3308be2ef01e3aad/lib/hal/src/hal_cache.c#L60-L65) 的 `HAL_CpuAddrToCacheAddr()`、`HAL_CACHE_GetCodeAddrBase()`、`HAL_CACHE_GetSRAMAddrBase()` | 弱 getter 对 M0 alias 作 `volatile uint32_t` 读取；没有对 CON16/17 写后读回比较。注释第 29–35 行提示 MCU 处于 unsafe state 时应由前级固件传入映射值。 | 函数存在 **SOURCE_VERIFIED**；无副作用 **UNVERIFIED** |
| 固定 HAL / RTOS 默认配置；BUS M0 | [`project/rk3576-mcu/src/hal_conf.h:16-20`](https://github.com/starwey604/rk3576-hal/blob/277de3fd4b0e640654ee73bb3308be2ef01e3aad/project/rk3576-mcu/src/hal_conf.h#L16-L20)、[RTOS `hal_conf.h:19-24`](https://github.com/starwey604/rk3576-rtos/blob/7c397f41751feb29b0b388dfda3d2c2225f1f87c/bsp/rockchip/rk3576-mcu/hal_conf.h#L19-L24) | 默认 `HAL_CACHE_DECODED_ADDR_BASE=0x47800000`；`DECODED_ADDR` 使用常量路径，只有该宏为零时才调用上述 getter。故不能把 getter 当作默认固件的运行时读回记录。 | **SOURCE_VERIFIED** |
| 固定 LubanCat U-Boot；A 核 bootloader | [`rk3576.c:61-70,255-275,293-341`](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/arch/arm/mach-rockchip/rk3576/rk3576.c#L293-L341) | `fit_standalone_release()` 通过 SMC 请求 BUS M0 code mapping；`arch_cpu_init()` 写 SYS_SGRF **CON14/15**。`readl()` 访问的是另一 block `FW_SYS_SGRF_BASE=0x26005000`；未找到针对 CON16/17 的 `readl()` 或 verify。 | **SOURCE_VERIFIED**；实际板 U-Boot 构建配置 **UNVERIFIED** |
| 上游 TF-A RK3576；BL31/EL3 | [`secure.c:28-38`](https://github.com/TrustedFirmware-A/trusted-firmware-a/blob/58bd918dce1e2831fbec252c0b2139d519a00d55/plat/rockchip/rk3576/drivers/secure/secure.c#L28-L38)、[`secure.h:24`](https://github.com/TrustedFirmware-A/trusted-firmware-a/blob/58bd918dce1e2831fbec252c0b2139d519a00d55/plat/rockchip/rk3576/drivers/secure/secure.h#L24) | EL3 写 `SYS_SGRF_SOC_CON(0/1)`，没有针对 `(16/17)` 的读取、写入或读回校验；公开 `sip` handler 也未提供这两个寄存器的只读接口。 | 对此公开源码 **SOURCE_VERIFIED**；当前板 BL31 **UNVERIFIED** |
| 上游 TF-A RK3576；BL31/EL3 | [`firewall.c:98-112,149-159,490-512,675-691`](https://github.com/TrustedFirmware-A/trusted-firmware-a/blob/58bd918dce1e2831fbec252c0b2139d519a00d55/plat/rockchip/rk3576/drivers/secure/firewall.c#L98-L112) | `sys_sgrf` 分入 secure slave group 1；该源码的 group 1 配置拒绝 non-secure 访问，`fw_init()` 应用该配置。若当前板使用同等策略，Linux EL1 直接读可能被 firewall 拒绝。 | 公开实现 **SOURCE_VERIFIED**；对当前板的推断 **SOURCE_INFERRED** |

**Read-back/verify 结论：** 未找到任何官方/厂商路径对 **CON16 或 CON17 本身**执行 `write → read → compare`，也未找到当前默认 M0 固件实际调用 HAL getter 的证据。U-Boot 对别的 `FW_SYS_SGRF` 寄存器作读改写、BL31/EL3 对 `SOC_CON(0/1)` 作写入，均不能充当目标寄存器的读回证据。当前板 vendor BL31 源码/镜像未匹配，不能宣称其中不存在相关路径。

## 两个独立结论

| 问题 | 结论 | 理由 |
| --- | --- | --- |
| `REGISTER_READ_SEMANTICS` | **`INSUFFICIENT_EVIDENCE`** | 没有 CON16/17 明确 read attribute 或已运行的厂商读回校验。HAL 弱 getter 仅说明接口意图，默认路径不调用。未发现明确 read-clear，也不能排除。 |
| `LINUX_MMIO_ACCESSIBILITY` | **`INSUFFICIENT_EVIDENCE`** | 公开 TF-A 将 SYS_SGRF 设为 non-secure 不可访问；当前板 `bl31-v1.14` 的实际 firewall 策略及 Linux EL1 合法只读入口未证。BL31/EL3 或 M0 可访问不等于 Linux EL1 可访问。 |

**最终：`READ_GATE_BLOCKED`，AMP 保持 `C. HOST_BUILD_PASS`。** 不给予下一轮 `devmem` 执行许可。解除门禁需要：(1) Rockchip/板厂明确 CON16/17 读取无副作用的寄存器属性或等价确认；(2) 与当前板 BL31/安全策略匹配的 Linux EL1 只读可达性证据，或明确受支持、低风险的只读固件接口。取得这些材料后重新评估；本轮不读取任何板端寄存器。
