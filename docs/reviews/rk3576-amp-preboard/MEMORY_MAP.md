> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# Gate 1 — 唯一内存图（2026-10-01）

**BLOCKED。** 表中 Linux PA 是总线物理地址，M0 Address 是 BUS Cortex-M0 软件地址。区间一律半开。`rk3576-amp.dtsi` 是 Rockchip CPU3 示例，**不是**当前 LubanCat 运行 DT。当前实板 DTB 仅有 ramoops 预留 `0x40110000+0xe0000`，没有 AMP/RPMsg 预留。M0 固件不能据此上板。

| Region | Linux PA | M0 Address | Size | Owner | Purpose | Source |
| ------ | -------: | ---------: | ---: | ----- | ------- | ------ |
| FIT 实际装载 / M0 code window | `0x47800000`（候选） | `0x00000000` | `0x80000` 链接窗；bin `0x21058` | M0 | vector/text/data/BSS/heap/stack | `Image/amp.its:load`；`gcc_link.ld.S:DDR`；ELF `readelf -l`；板版本 U-Boot `fit_standalone_release()` |
| RTOS 可装载字节 | `0x47800000–0x47821058`（候选） | `0x0–0x21058` | `0x21058` = 135,256 B | M0 | vector/text/data | 本轮 `rtthread.bin`、`readelf -l` |
| RTOS `.data` | `0x47820600–0x47820f88`（候选） | `0x20600–0x20f88` | `0x988` | M0 | initialized data | `readelf -S` |
| RTOS `.bss` | `0x47821080–0x47824554`（候选） | `0x21080–0x24554` | `0x34d4` | M0 | zero init | `readelf -S` |
| RTOS heap | `0x47824554–0x4787fc00`（候选） | `0x24554–0x7fc00` | `0x5b6ac` | M0 | RT heap | `gcc_link.ld.S`；`readelf -S` |
| RTOS stack | `0x4787fc00–0x47880000`（候选） | `0x7fc00–0x80000` | `0x400` | M0 | main stack | `gcc_link.ld.S:MAIN_STACK_SIZE`；`nm:__StackTop` |
| Linux 参考 RPMsg reserved | `0x47800000–0x47a00000` | **未证明**；若 remap base `0x40000000` 则 `0x27800000` | `0x200000` | RPMsg | vring reserve | [Rockchip `rk3576-amp.dtsi`](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/arch/arm64/boot/dts/rockchip/rk3576-amp.dtsi) |
| Linux 参考 vring0 | `0x47800000–0x47808000` | 同上，未证明 | `0x8000` | Linux/M0 | virtqueue 0 | DTS `rpmsg reg`；Linux `rk_set_vring_phy_buf()` |
| Linux 参考 vring1 | `0x47808000–0x47810000` | 同上，未证明 | `0x8000` | Linux/M0 | virtqueue 1 | Linux `RPMSG_VRING_SIZE` |
| Linux 参考 RPMsg buffers / reserved DMA | `0x47a00000–0x47c00000` | 若 remap base `0x40000000` 则 `0x27a00000`；**未证明** | `0x200000` | Linux DMA/M0 | payload pool | DTS `rpmsg-dma`；`memory-region`；`platform_patova()` 减 `0x20000000` |
| Linux 参考 AMP shmem | `0x47c00000–0x48000000` | **未证明** | `0x400000` | shared，分区未定 | AMP shared DDR | DTS `amp-shmem` |
| M0 linker `LINUX_RPMSG` | **未证明**；公式为 `0x07d00000 + SRAM_REMAP_BASE` | `0x27d00000–0x28100000` | `0x400000` | Linux/M0 | RPMsg M0 window | `gcc_link.ld.S`；[TRM 8.6.2 表 8-6](https://rockchip.fr/Rockchip%20RK3576%20TRM%20V1.2%20Part1.pdf) |
| Linux 参考 MCU reserved | `0x48000000–0x48200000` | **未证明** | `0x200000` | M0 | 示例保留 | DTS `mcu@48000000` |
| M0 ATAGS | **未证明**；`SRAM_REMAP_BASE` | `0x20000000–0x20002000` | `0x2000` | shared | boot ATAGS | `gcc_link.ld.S`；TRM 表 8-6 |
| MCU 内置 SRAM | **UNRESOLVED** | **UNRESOLVED** | **UNRESOLVED** | 未分配 | 本固件未显式链接到 SRAM | linker 仅 `DDR/LINUX_ATAGS/LINUX_RPMSG` |
| 其他 DDR | 当前 Linux 管理，具体避让未定义 | N/A | 4 GiB 总内存中未划定 | Linux | 正常系统 | 当前 DTB `reserved-memory` 不含 AMP |

## `0x47800000` 语义与冲突

候选 `amp.its` 的 `load=<0x47800000>`；板版本 U-Boot 源码 `drivers/cpu/rockchip_amp.c:amp_cpus_on()` 从名称为 `amp` 的分区读取 FIT，`boot_get_loadable()` 把 payload 装到 FIT load；`arch/arm/mach-rockchip/rk3576/rk3576.c:fit_standalone_release()` 把 entry 作为 `MCU_CODE_START_ADDR` 传 BL31 SMC 并 release BUS_M0。ITS 没有 `entry`，该 loader 使用 load 作 entry。**这不是后续会搬走的临时地址。** Rockchip 示例把同一 PA 作为 vring0 起点，重叠 `0x47800000–0x47880000`；不能同时使用这两份原样文件。当前板因没有 AMP DT 与 `amp` 分区，尚不存在运行时重叠，亦没有 AMP 启动路径。

TRM 的 BUS_MCU 公式：M0 `0x0–0x1fffffff` 经 `SYS_SGRF_SOC_CON16` 映射；`0x20000000–0x3fffffff` 经 `SYS_SGRF_SOC_CON17[31:10]<<10` 映射，且 remap 要经 MCU 软复位生效。候选 HAL `soc.h:CODE_ADDR_MAP_START/SRAM_ADDR_MAP_START` 和 `hal_cache.c:HAL_CACHE_GetCodeAddrBase/GetSRAMAddrBase` 可读两寄存器。板版本 U-Boot release 路径只设置 **CODE** remap，没有可证明的 CON17 写入；板上 BL31 v1.14 的 CON17 初值也未取得。因此不能把 `0x27d00000` 擅自等同 `0x47d00000`。即使假设 base=`0x40000000`，它也对应 `0x47d00000`，与参考 Linux vring `0x47800000` 不同。Host 脚本另以假设值 `0x3fb00000` 试算：虽可让 M0 ring 映到参考 `0x47800000`，但当前 `platform_patova()` 固定减 `0x20000000` 会让 DMA buffer 错映到 `0x47500000` 而非参考 `0x47a00000`。这些是假设演算，均不是板端 remap 实测值。

**解除条件：** 取得与当前 U-Boot/BL31 构建一致的 CON17 设置源码或只读可验证的寄存器证据；设计新的不重叠 Linux reserved-memory / ITS / linker 三方区域，交叉检查每个物理区间与 4 GiB DDR 和现有保留区，再 Host 构建。此轮不指定新地址。
