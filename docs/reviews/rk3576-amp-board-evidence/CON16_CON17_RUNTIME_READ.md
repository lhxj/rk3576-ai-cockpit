# CON16/CON17 runtime read：前置源码核验未通过

**状态：PREFLIGHT_BLOCKED_READ_SEMANTICS。未访问板端 MMIO；以下命令均未执行。**

## 源码定义

- [RK3576 TRM v1.2 part1 §1.1](https://rockchip.fr/Rockchip%20RK3576%20TRM%20V1.2%20Part1.pdf) 的 address mapping 给 `SYS_SGRF` 系统物理基址 `0x26004000`、大小 4 KiB。§8.6.2 表 8-6（PDF 第 762 页）给 BUS_MCU code window `SYS_SGRF_SOC_CON16[31:10]`、shared window `SYS_SGRF_SOC_CON17[31:10]`，分别形成 1 KiB 对齐的基址；shared 地址公式为 `Linux PA = M0 address − 0x20000000 + ((CON17 >> 10) << 10)`。remap 在 MCU soft reset 后才生效。**SOURCE_VERIFIED**。
- 固定 HAL [`soc.h:651-652`](https://github.com/starwey604/rk3576-hal/blob/277de3fd4b0e640654ee73bb3308be2ef01e3aad/lib/CMSIS/Device/RK3576/Include/soc.h) 的 BUS_MCU `CODE_ADDR_MAP_START=(0x26004000 + 0x20000000 + 0x60)`、`SRAM_ADDR_MAP_START=(0x26004000 + 0x20000000 + 0x64)` 是 **M0 alias**。TRM 表 8-6 的 peripheral window 将 M0 `0x40000000–0x5fffffff` 映至系统 `0x20000000–0x3fffffff`，所以减去 `0x20000000` 后，系统物理地址正是 CON16 `0x26004060`、CON17 `0x26004064`；[公开 U-Boot RK3576 源](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/arch/arm/mach-rockchip/rk3576/rk3576.c) 亦给 `SYS_SGRF_BASE=0x26004000`。地址与位字段无矛盾。**SOURCE_VERIFIED**。
- 固定 HAL `lib/hal/src/hal_cache.c:HAL_CACHE_GetCodeAddrBase()/HAL_CACHE_GetSRAMAddrBase()` 使用 `volatile` 读取映射寄存器，说明该 HAL 设计预期读这些值。但公开 TRM part1 的 1412 页全文扫描中，`SYS_SGRF_SOC_CON16/17` 只出现在 PDF 第 762 页的 remap 表；没有逐寄存器 `Attr`、read-clear 或读取副作用说明。**没有找到已知 read-clear 语义，也无法权威确认不存在读取副作用。** HAL 读函数是辅助证据，不能替代寄存器访问属性规范。**READ_SEMANTICS_UNVERIFIED**。

用户要求“读取副作用无法确认即停止”；因此不进入板端预检或读取。这个停止条件独立于特权读取审批，不能通过猜测解除。

## 待审批/待资料命令，未执行

| 顺序 | 命令 | stdout | stderr | exit code |
| --- | --- | --- | --- | --- |
| 1 | `devmem 0x26004060 32` | 未执行 | 未执行 | 未执行 |
| 2 | `devmem 0x26004064 32` | 未执行 | 未执行 | 未执行 |

两条命令按语法均为 32-bit **纯读**，不写寄存器；仍有 MMIO access denied、bus exception、系统挂起或未公开读副作用风险。需要 RK3576 SYS_SGRF CON16/17 的权威寄存器访问属性或等价厂商确认，并按 `AGENTS.md` L3/用户的特权读取门获得明确授权后，才可重新评估。未尝试 `devmem`、`/dev/mem`、debugfs 或 U-Boot console。

## 读数、解码与映射

| Purpose | M0 address | Linux PA | 状态 |
| --- | ---: | ---: | --- |
| code window base / CON16 | `0x00000000` | 未取得 CON16 值 | BLOCKED |
| shared base / vring0 | `0x27d00000` | `((CON17 >> 10) << 10) + 0x07d00000`，数值未知 | BLOCKED |
| vring1 | `0x27d08000` | `((CON17 >> 10) << 10) + 0x07d08000`，数值未知 | BLOCKED |
| buffer pool | 未证明具体 M0 地址 | 未能计算 | BLOCKED |

无原始寄存器读数，故没有 bitfield **运行时解码**或可供脚本核验的数值计算；也无法判定 `0x47800000` 与实际 M0 shared 区间是否重叠。CPU3 参考 DTS vring0 与候选 FIT load 同为 `0x47800000` 的**参考设计冲突**仍成立，但这不是本轮运行时 CON17 证据。

| 结果字段 | 值 |
| --- | --- |
| `CON16_RUNTIME_EVIDENCE` | `BLOCKED` |
| `CON17_RUNTIME_EVIDENCE` | `BLOCKED` |
| `M0_LINUX_ADDRESS_MAPPING` | `UNRESOLVED` |
| `0x47800000_CONFLICT` | `UNRESOLVED`（相对于当前 CON17 runtime mapping） |
| 整体 AMP 等级 | `C. HOST_BUILD_PASS` |

即使未来读到寄存器，也只能证明读取时刻的状态；写入者、boot 阶段、M0 release 后是否重写、实际 U-Boot `CONFIG_AMP` 和 BL31 SMC 能力仍需独立证明。
