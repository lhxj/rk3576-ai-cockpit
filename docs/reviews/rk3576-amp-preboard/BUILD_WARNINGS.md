> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 七条原始构建 warning 分类

原固定源码默认构建完整 log：`/tmp/rk3576-amp-audit/build-default-restored.log`；派生 clean build log：`/tmp/rk3576-preboard-build2.log`。两个日志均仅 Host。旧默认 SConscript 无条件编译所有 `applications/*.c` 与 `kiss_fft/*.c`；派生 echo 将应用列表限定 `main.c`、`amp_echo.c`，旧 demo 不编入固件，**不是通过静音选项隐藏 warning**。

| # | 原 warning | 分类 | 本轮处理 / 结果 |
| --- | --- | --- | --- |
| 1 | `applications/ddr_test.c:17` 丢 `volatile` | MUST_FIX_BEFORE_BOARD | DDR demo 不属 echo，排除；clean build 消失。若以后启用 demo，需源代码修复。 |
| 2 | `forlinx_timer_test.c:96` `rt_hw_interrupt_install` 隐式声明 | MUST_FIX_BEFORE_BOARD | Forlinx demo 排除；消失。 |
| 3 | `forlinx_timer_test.c:97` `rt_hw_interrupt_umask` 隐式声明 | MUST_FIX_BEFORE_BOARD | 同上。 |
| 4 | `kiss_fft.c:551` `rt_hw_interrupt_install` 隐式声明 | MUST_FIX_BEFORE_BOARD | FFT demo 排除；消失。 |
| 5 | `kiss_fft.c:552` `rt_hw_interrupt_umask` 隐式声明 | MUST_FIX_BEFORE_BOARD | 同上。 |
| 6 | `kiss_fft.c:589` void 函数带返回值 | MUST_FIX_BEFORE_BOARD | FFT demo 排除；消失。 |
| 7 | `ld: rtthread.elf has a LOAD segment with RWX permissions` | BENIGN（本裸机镜像） | linker 单一 `DDR (rxw)` 分段混合 text/data/heap；ELF 只作裸 bin 源，M0 不按 ELF 权限装载。`readelf -l` 确认没有区域溢出或 section overlap；警告仍保留，不能当作内存布局 gate 通过。 |

RPMsg 旧例程单独启用时还有两条**不属于上述七条**的回调函数指针类型与 `INIT_APP_EXPORT` 返回类型 warning；本轮派生源码修复了非 MOS 分支的两处签名、MOS 分支 NS callback。当前 echo clean build 没有 incompatible pointer / implicit declaration / address truncation / alignment / region overflow warning。另从 SCons 真实编译命令抽出相同 include/宏，分别对 `amp_echo.c` 与旧 `rpmsg_test.c` 的 Linux 分支做 `-Wall -Wextra -Werror=incompatible-pointer-types -Werror=implicit-function-declaration -fsyntax-only`：**两者 exit 0、零 warning**。这不代表未启用的其他 demo 已经零 warning。

另有一项**非 warning 的架构属性问题**：`common/drivers/drv_cache_gcc.S` 原写 `.cpu cortex-m4`，其 `.o` 属性为 ARMv7E-M，即使该 ISR `.text` 被 `--gc-sections` 丢弃，链接后的 ELF 属性仍显示 M4。派生树改为 `.cpu cortex-m0`；最终 clean ELF `readelf -A` 为 `6S-M / v6S-M / Thumb-1`。扫描了 354 个本轮构建对象，原唯一 v7E-M 对象就是此文件；其五条汇编指令均可由 Cortex-M0 编译。源码编译参数仍是 `-mcpu=cortex-m0 -mthumb`。
