> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# Host 验收记录

## 不变基线与派生 Git

- 固定 RTOS `7c397f41751feb29b0b388dfda3d2c2225f1f87c`、HAL `277de3fd4b0e640654ee73bb3308be2ef01e3aad`：`rev-parse HEAD` 已复核，两个 reference 工作树 clean。
- 派生 RTOS `/home/ywx/rk3576-work/worktrees/rk3576-amp-preboard/rtos`，分支 `agent/amp-preboard-closure`；同级 `hal` symlink 指向固定 HAL。主项目独立 worktree `.../project`，同名分支，基于 main `3b07df7ba05a535ba18dbc7d692ef961b9b748e5`。本轮不改固定参考，也不 vendor RTOS 源码到主项目。
- 派生 RTOS 单一 patch commit `1d0de06c394f89be35a4b6966e766b56f035c19d`；主项目保存 `patches/rk3576-amp-preboard/0001-Add-host-only-RK3576-M0-RPMsg-echo-candidate.patch`，SHA256 `4d02455ad53407414c9eaf832b9245b2c54e9da2ed909f25a4cf500ee5fba1fa`。RTOS 派生树 commit 后 clean。
- 仅 Host 使用 Arm GNU `13.2.1 20231009` `arm-none-eabi-gcc`、SCons 4.0.1、系统 `/usr/bin/mkimage`。未装新包、未执行候选 `../tools/mkimage` 或候选固件。

## clean build

执行：在 `bsp/rockchip/rk3576-mcu` 中以 `RTT_ROOT=.../rtos`、`RTT_EXEC_PATH=/home/ywx/.local/toolchains/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi/bin` 运行 `scons -c` 后 `scons -j4`。最终两个 exit **0**；完整原始输出 `/tmp/rk3576-preboard-final-clean.log`、`/tmp/rk3576-preboard-final-build.log`（Host 临时路径）。编译 `applications/amp_echo.c` 并链接 `rpmsg_lite_remote_init`、`rpmsg_ns_announce`、MBOX port；原默认 6 条 C warning 已通过排除无关 demo 清除，剩 1 条 ELF RWX warning，详见 BUILD_WARNINGS。

| 检查 | 最终结果 |
| --- | --- |
| `file` / `readelf -h` | ELF32 little-endian ARM, EABI5, soft-float；entry `0x141`（Thumb reset symbol `0x140`）。bin 是裸 data。 |
| `readelf -A` | **6S-M / ARMv6S-M / Thumb-1**。首次聚合 ELF 曾因 `drv_cache_gcc.S:.cpu cortex-m4` 显示 v7E-M；派生修为 cortex-m0 后 clean build 正确。 |
| `readelf -S` / `objdump -h` | `.vectors 0x0+0x19c`、`.text 0x1a0+0x20460`、`.data 0x20600+0x988`、`.bss 0x21080+0x34d4`、`.heap 0x24554+0x5b6ac`、`.stack 0x7fc00+0x400`，无 M0 虚拟地址 section overlap。 |
| `readelf -l` | 主要 RXW LOAD M0 `0x0–0x80000`、FileSiz `0x21058`；`0x20000000+0x2000` ATAGS 和 `0x27d00000+0x400000` RPMsg 为 NOLOAD。虚拟区间互不交叠；**物理区间 alias 未验证**。 |
| `nm` | `Reset_Handler 0x140`、`amp_echo_run 0x6b08`、`rpmsg_ns_announce 0x857c`、`rpmsg_lite_remote_init 0x8a0c`、`__StackTop 0x80000`、RPMsg base `0x27d00000`。 |
| `arm-none-eabi-size` / `wc -c` | text 132,804；data 2,448；bss 合计 4,591,488（包含 NOLOAD shared/heap），bin **135,256 B**。 |
| hash | ELF `14a73de360a6acddefe344519f9497ee3296f57e553c5a785537d65c4f2c75f9`；map `7cd626158474d284f1092645d376ca0a0270e84f839560791ed8368e2736f8e0`；bin `641c5813266a57443a40fcb5c545e60ffe56a135cd8fb07d65e7dd9bfaef484c`。仅代表该次构建，未建立逐字可复现保证。 |
| Host FIT | 系统 mkimage 结构生成成功；`amp.img` SHA256 `6fb95fd6544c6582666faa4fe08321bfa623497e1d0ac957622ca5243af9d4d2`；signature value unavailable，不能部署。 |

## Host 参数测试

`python3 scripts/test_amp_layout.py` 读取固定 Linux 参考 DTS/头、派生 M0 linker/ITS/port/echo 和候选 Linux test driver，**exit 1 / BLOCKED (6 failures)**：Linux link3 vs M0 link4、mailbox3 vs mailbox4、FIT 不在参考 MCU reserve、FIT 与 RPMsg vring 重叠、CON17 vring remap 无证、CON17 DMA buffer remap 无证。ring `0x8000`、alignment `0x1000`、buffer count64/payload496、service name 一致；`platform_patova()` 推出 M0 DMA buffer 地址 `0x27a00000`，其 Linux PA 对应关系仍未证。脚本 fail-closed；将来改 DTS/ITS/linker/service 任一值若不一致会报非零，必须以实际拟部署 DTS/内核头替换参考 fixture 才能成为 CI 准入测试。

参考 fixture 固定来自 Rockchip kernel commit `77168c8d5ab82399f65a80e9f807b50ba37cf483` 的 `rk3576-amp.dtsi` 和 `include/linux/rpmsg/rockchip_rpmsg.h`，SHA256 分别 `9c2c8cd4bd97b26007d771cab08e7407674258114e0a46f08ef99c552c1ca904`、`ca960dc798f87abc5339eb50f3671e08cc978f12d928d2f098fa62649618eb22`。它们是对照材料，不代表运行 DT/内核源。

`scripts/board/amp_echo_linux` 仅候选 Linux 模块源码，未取得当前 kernel exact build tree，因此没有声称模块 Host 编译 PASS。所有 ELF/FIT/代码均未运行于开发板。
