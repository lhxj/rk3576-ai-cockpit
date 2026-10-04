> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# RT-Thread BSP 与 HAL 静态审查

路径缩写：`R=/home/ywx/rk3576-work/reference/rk3576-amp/rk3576-rtos`，`H=/home/ywx/rk3576-work/reference/rk3576-amp/rk3576-hal`。以下路径均相对于该根；固定提交见 `SUMMARY.md`。这是代码证据，不是 LubanCat 实板结论。

## BSP 逐项结论

| 问题 | 结论与源码证据 |
|---|---|
| 核心 | `R/bsp/rockchip/rk3576-mcu/.config` 的 `CONFIG_ARCH_ARM_CORTEX_M0=y`；`rtconfig.py` `CPU='cortex-m0'`；`hal_conf.h` 定义 `HAL_MCU_CORE`、`HAL_BUS_MCU_CORE`；`H/lib/CMSIS/Device/RK3576/Include/soc.h` 设置 `__CM0_REV`。这是真正的 **BUS Cortex-M0** 构建。`rk3576-64/rtconfig.py` 的 `cortex-a53+crypto+fp16` 是不同 BSP。 |
| ISA/ABI | `rk3576-mcu/rtconfig.py` `-mcpu=cortex-m0 -mthumb`、`arm-none-eabi-`；HAL `start_m0.S` `.arch armv6-m`。ELF `Machine=ARM`、EABI5、soft-float、32-bit little-endian。 |
| entry/vector | `gcc_link.ld.S` `ENTRY(Reset_Handler)`、`.vectors > DDR`、`KEEP(*(.vectors))`；HAL `lib/CMSIS/Device/RK3576/Source/Templates/GCC/start_m0.S` 中 `__Vectors` 第 0 word `__StackTop`、第 1 word `Reset_Handler`，启动清 BSS、设 MSP、调用 `SystemInit` 和 `entry`。实际 ELF vector 在 `0x0`，第 0 word `0x00080000`，第 1 word `0x00000141`，ELF entry `0x141`（Thumb bit）。 |
| memory | `gcc_link.ld.S` 将 M0 视图 `DDR=0x0..0x7ffff`（512 KiB）、ATAGS `0x20000000..0x20001fff`、Linux RPMsg `0x27d00000..0x280fffff`（4 MiB）。`hal_conf.h` `HAL_CACHE_DECODED_ADDR_BASE=0x47800000`，`H/.../rk3576.h` 的 `MCU_OFFSET=0x20000000` 用于外设寄存器地址，**不能单独证明 DDR 别名映射**。`rk3576-64/build.sh` 将其 Linux RPMsg base 设为 `0x47d00000`，可作交叉线索；M0 共享区实际 SoC 物理地址仍待 boot/总线映射证实。`Image/amp.its` 固件 load `0x47800000`，与参考 Linux RPMsg vring 起点相同，不能原样配套。没有为代码使用片上 SRAM 的 linker region。 |
| stack/heap | `gcc_link.ld.S` `MAIN_STACK_SIZE=0x400`，heap 在 BSS 结束和 stack 起点间；`board/evb/board.c::rt_hw_board_init` 调 `rt_system_heap_init`。构建 map：BSS `0x2ea00..0x31604`（11,268 B），heap `0x31604..0x7fc00`（321,020 B），初始 MSP/stack `0x7fc00..0x80000`（1,024 B）。RT-Thread main thread stack `rtconfig.h` 2,048 B、FinSH 4,096 B，另由 RTOS heap 分配。 |
| console | `.config` `CONFIG_RT_CONSOLE_DEVICE_NAME="uart5"`、`CONFIG_RT_USING_UART5=y`；`board/evb/board.c::g_uart5_board` 115200，`rt_hw_board_init` 调 `rt_hw_usart_init`、`rt_console_set_device`。`board/evb/iomux.c::rt_hw_iomux_config` 选 `uart5_m0_iomux_config`；`board/common/iomux_base.c` 为 GPIO3_D4/D5。 |
| IRQ/NVIC | 启动 `SystemInit` → `HAL_INTMUX_Init`；`HAL_Init` → `HAL_NVIC_Init`；`board/evb/board.c` → `rt_hw_interrupt_init`、SysTick install/unmask；`H/.../soc.h` 定义 BUS MCU IRQ/INTMUX，`R/bsp/rockchip/common/drivers/interrupt.c` 为 RT-Thread IRQ 分派。MBOX port 另调用 `rt_hw_interrupt_install`、`HAL_MBOX_RegisterClient`。 |
| tick/timer | `hal_conf.h` `SYS_TIMER=TIMER11`，`HAL_Init` → `HAL_TIMER_SysTimerInit` 供 HAL delay/tick；RTOS 调度 tick 为 `board.c` 的 `HAL_SYSTICK_CLKSourceConfig(HAL_SYSTICK_CLKSRC_EXT)`、`HAL_SYSTICK_Config((SYSTICK_EXT_SRC/RT_TICK_PER_SECOND)-1)`，默认 `SYSTICK_EXT_SRC=PLL_INPUT_OSC_RATE`，`.config` `RT_TICK_PER_SECOND=100`。须核对启动后时钟频率。 |
| clocks/reset | `.config` `RT_USING_CRU=y`；`board.c` 的 `clk_inits` 与 `clks_unused` 仅 sentinel，`clk_init`/`clk_disable_unused` 无板级表；`H/lib/bsp/RK3576/hal_bsp.c::BSP_Init` 为空。HAL 有 CRU API，但 M0/UART/MBOX/timer clock/reset 的前置开启依赖 Linux AMP DTS/firmware boot chain，候选不能独立证明。 |
| cache/barrier | `.config` `RT_USING_CACHE=y`；`system_rk3576_mcu.c::SystemInit` 开启 SoC cache；`R/.../rpmsg_platform.c::platform_cache_all_flush_invalidate` 的 clean/invalidate 被注释；`rpmsg_compiler.h::MEM_BARRIER` 在 ARM GCC 为 `dsb`，environment `env_mb/rmb/wmb` 调它。屏障存在，跨核缓存一致性没有闭合。 |
| 产物 | `SConstruct` 预处理 `gcc_link.ld.S` 为 `gcc_link.ld`；`rtconfig.py` 指定 `rtthread.elf`、`rtthread.map`、objcopy 的 `rtthread.bin`。未生成 `rttmcu.bin`；这不是构建失败，需按启动链约定命名。 |
| mkimage | `mkimage.sh` 拷贝 `rtthread.bin` 为 `Image/rtt.bin`，调用 `../tools/mkimage -f Image/amp.its -E -p 0xe00 Image/amp.img`；`amp.its` 载入物名 `mcu`、load `0x47800000`、RSA-PSS `dev` 签名提示。脚本/随仓二进制未执行，`amp.img` 未生成；key、U-Boot FIT 支持和加载地址尚待验证。 |
| SDK依赖与 SCons | `SConstruct` 的 `RTT_ROOT` 默认指向 RTOS 根，`HalSConscript` 的 HAL 源经 `R/bsp/rockchip/common/hal` 符号链接解析到 `<SDK>/hal`。该链接由 Git mode `120000` 正确保存，目标 `../../../../hal`；本次目录结构下指向不存在的 `/home/ywx/rk3576-work/reference/rk3576-amp/hal`。隔离 worktree 提供同级 `hal` 映射后 `scons -j4` 独立 exit 0，无厂商顶层脚本。`SConstruct` 输出 `os_start -1`，未找到 SDK partition `OSA`，本配置 linker 未使用该值，仍须审查最终打包。 |
| 厂商残留 | `applications/forlinx_*.c` 仍由 `applications/SConscript` 的 `Glob('*.c')` 编译，`board/evb/board.c` 留有 I2C8 400 kHz 表项（当前 I2C8 未编译启用），`board.h` 写 `NCA9539_I2C_BUS_NAME="i2c7"`，`Image/amp.its` 与 EVB 参数固定。`PROVENANCE.md` 声称删去 Forlinx demo 与固定 commit 实际不符。无 `lubancat-v2` board 配置。 |

## HAL 模块闭合程度

`R/bsp/rockchip/common/HalSConscript` 用 `PROJECT='RK3576'` 从 `common/hal/lib/CMSIS/Device/RK3576/Source/Templates`、`hal/src`、`bsp/RK3576` 和 GCC startup 聚合源码。默认构建证实当前配置下可链接；不能推论 HAL 所有可选功能或板上外设都运行正常。`H/middleware/rpmsg-lite` 是另一份 RPMsg-Lite（与 `R/bsp/rockchip/common/drivers/rpmsg-lite/lib/rpmsg_lite/rpmsg_lite.c` hash 不同），当前 SCons 使用 **R 内嵌副本**，没有混链 HAL middleware。

| 项目 | 代码入口与本配置状态 |
|---|---|
| SoC 寄存器/IRQ | `H/lib/CMSIS/Device/RK3576/Include/{rk3576.h,soc.h}`；M0 `MBOX_BB0_IRQn=9`、INTMUX 扩展 IRQ、MCU 外设偏移。 |
| MBOX | `H/lib/hal/src/hal_mbox.c::{HAL_MBOX_Init,RegisterClient,SendMsg,IrqHandler,UnregisterClient}`；`hal_conf.h` 始终启用。 |
| CRU / reset | `H/lib/hal/src/cru/hal_cru_rk3576.c`、`hal_cru.c`；`H/lib/bsp/RK3576/hal_bsp.c::g_cruDev`；RTOS `RT_USING_CRU=y`。具体 clock owner 未定。 |
| GPIO / I2C / timer | `H/lib/hal/src/{hal_gpio.c,hal_i2c.c,hal_timer.c}`；`H/lib/bsp/RK3576/hal_bsp.c` 有 I2C 设备描述。**配置漂移**：`.config` 标记 I2C3/6/7/8，但本次 SCons 实际使用的 `rtconfig.h` 只定义 `RT_USING_I2C7`；`drv_i2c.c` 的其他总线分支被条件编译排除。EVB `board.c` 的 I2C8 400 kHz 表项因此未作用于实际启用的 I2C8 设备；I2C7 走默认配置。 |
| NVIC / pinctrl / UART | `H/lib/hal/src/{hal_nvic.c,hal_uart.c}`、`H/lib/hal/src/pinctrl/hal_pinctrl_rk3576.c`；RTOS `board/evb/iomux.c` 有实际 mux 写入。 |
| cache / barrier | `H/lib/hal/src/hal_cache.c` 处理 decoded 地址，DCACHE API 存在；RPMsg port 当前未调用 clean/invalidate。 |
| PM / power | `H/lib/hal/src` 有 PM/电源接口和设备层；RTOS `.config` 的 `RT_USING_PM`、DVFS、runtime PM 均关闭。无 LubanCat power-domain 策略证明。 |

`rk3576-64` 的 `fl_rpmsg_test.c` 和 A53 linker/FIT 只作为对照，不构成 M0 的启动或 Linux 联通证明。
