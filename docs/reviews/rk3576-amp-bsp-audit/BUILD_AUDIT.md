# Host 构建与产物审查

2026-10-01；x86_64 Ubuntu 22.04 WSL。原候选 Git 树未改；RTOS 独立 worktree 在 `/tmp/rk3576-amp-audit/rtos`，同级 `/tmp/rk3576-amp-audit/hal` 指向固定 HAL 仓库，以满足 Git 保存的 `common/hal -> ../../../../hal`。没有安装软件包，没有运行候选 `mkimage` 二进制或固件。

## 构建脚本安全审查与复现

`SConstruct` 的主动命令是调用指定 `arm-none-eabi-gcc` 预处理 linker；`rtconfig.py` post action 仅 `objcopy` 和 `size`；`mkimage.sh` 不被 SCons 调用。本次未执行 `mkimage.sh`。`SConscript` 通过 HAL link 收集源码，无下载、安装或板端命令。工具版本：`arm-none-eabi-gcc (Arm GNU Toolchain 13.2.rel1) 13.2.1 20231009`；SCons `4.0.1`；不能用 AArch64 Linux GCC 替换。

```sh
cd /tmp/rk3576-amp-audit/rtos/bsp/rockchip/rk3576-mcu
RTT_ROOT=/tmp/rk3576-amp-audit/rtos \
RTT_EXEC_PATH=/home/ywx/.local/toolchains/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi/bin \
scons -j4
```

默认构建 **exit 0**，完整命令/输出在 `/tmp/rk3576-amp-audit/build.log`；恢复默认配置后再次 exit 0，日志 `/tmp/rk3576-amp-audit/build-default-restored.log`。记录到的关键执行序列：`arm-none-eabi-gcc -P -C -E ... gcc_link.ld.S -o gcc_link.ld`；`CC/AS`；`LINK rtthread.elf`；`arm-none-eabi-objcopy -O binary rtthread.elf rtthread.bin`；`arm-none-eabi-size rtthread.elf`。`SConstruct` 显示 `os_start -1`，说明未解析到 SDK 的 OSA partition。主产物是 `rtthread.elf`、`rtthread.map`、`rtthread.bin`，无 `rttmcu.bin`、`amp.img`。

## `file` / `readelf` / `nm` / `size` / `objdump` 证据

| 检查 | 默认配置结果 |
|---|---|
| `file` | ELF32 little-endian ARM executable、EABI5、静态链接、debug_info；bin 为裸 data。 |
| `readelf -h` | ARM / EABI5 / soft-float，entry `0x141`（Thumb Reset_Handler `0x140`）。 |
| `readelf -S` / `objdump -h` | `.vectors` `0x0` 412 B；`.text` `0x1a0` 186,144 B；`.data` `0x2d8c0` 4,200 B；`.bss` `0x2ea00` 11,268 B；`.heap` `0x31604` 321,020 B；`.stack` `0x7fc00` 1,024 B；ATAGS `0x20000000` 8 KiB；RPMsg `0x27d00000` 4 MiB。 |
| `readelf -l` | RXW load `0x0`、file 190,968 B、mem 512 KiB；两段 `NOLOAD` 在 `0x20000000` 和 `0x27d00000`。linker 发出 RXW segment warning。 |
| `nm -n` | `__Vectors=0x0`、`Reset_Handler=0x140`、`SysTick_Handler=0x184`、`rt_hw_board_init=0x92d8`、`__StackTop=0x80000`。默认 ELF **没有** `rpmsg_lite_remote_init` / `rpmsg_ns_announce`，因测试服务未启用且 `--gc-sections`。 |
| `size` / `wc -c` | GNU `size`: text 186,756、data 4,208、bss 4,535,808；后者包含 4 MiB RPMsg + 8 KiB ATAGS + heap/stack，不能当成实际代码 BSS。bin 为 **190,968 B**。 |
| digest | 恢复默认后 `rtthread.bin` SHA-256 `aba92a38a5c31966d6559ca8ac6cd353c1a546a8174b65c3716c574b356350e9`。 |

默认构建 7 条 warning：`applications/ddr_test.c` 丢失 volatile；`forlinx_timer_test.c` 与 `kiss_fft.c` 的 IRQ API 隐式声明共 4 条；`kiss_fft.c` void 函数返回值；ELF RXW segment。警告文件属于候选 demo，但仍进入默认构建。无 error。编译成功只证明链接闭合，不证明固件可启动。

## RPMsg 示例的单独编译试验

仅在隔离 worktree 的 `rtconfig.h` 追加 `#define RT_USING_COMMON_TEST_LINUX_RPMSG_LITE`，再次 `scons -j4` **exit 0**，完整输出 `/tmp/rk3576-amp-audit/build-rpmsg-test.log`。`nm` 证实 `rpmsg_linux_test`、`rpmsg_lite_remote_init`、`rpmsg_ns_announce`、`platform_notify`、`HAL_MBOX_SendMsg` 进入实验 ELF。新增两条警告：`common/tests/rpmsg_test.c:462` 的 `rpmsg_ns_cb` 被错误声明为返回 callback 的函数，与 `rpmsg_ns_bind` 要求的 `void` callback 不符；`:478` 的 `INIT_APP_EXPORT` 注册 `void` 函数，而框架要求 `int (*)(void)`。实验未运行，更不能据此称服务可用。试验后 `git restore` worktree `rtconfig.h` 并重建默认配置；两个固定候选库始终 clean。

最小修复方向：保持固定源码为证据，在后续新 board worktree 中修正 HAL 目录映射、两处回调签名、默认 demo 选择，再明确 link-id/内存/MBOX/cache 与启动链；不能仅把 warning 静音。当前等级仍为 **C. HOST_BUILD_PASS**。
