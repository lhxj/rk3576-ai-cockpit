> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# Linux 专用 RPMsg echo peer：Host 模块构建通过，非上板模块

`scripts/board/amp_echo_linux/rk3576_amp_echo_test.c` 只含 `probe()`、callback、`remove()`，service `rk3576-m0-echo`；probe 发 HELLO，收到 HELLO_ACK 发 PING，收到 PONG 结束。实际板 headers 的 `rpmsg_send()` 接 `void *`，故本轮将两个发送常量数组改成非 const 静态数组，消除 `-Werror=discarded-qualifiers`。`RPMSG_CHAR/CTRL/TTY` 不必启用。

`scripts/board/amp_copy_kernel_headers_readonly.sh` 经板锁、只读 SSH tar stream 获得板上 headers；只在 Host 副本把 ARM64 预编译 `fixdep/modpost` 换为 x86 Host 工具，未改板端。`scripts/amp/build_linux_echo_against_board_headers.sh` 于全新 `artifacts/local/amp-linux-echo-final/` Kbuild exit 0，输出 AArch64 relocatable KO，`vermagic=6.1.99-rk3576 SMP mod_unload aarch64`，alias `rpmsg:rk3576-m0-echo`；SHA256 见 manifest。Kbuild 警告仅 Host GCC 11.4 与镜像 GCC 10.3.1 不同，源码编译无 warning。

**HOST_TESTED；BOARD_MODULE_READY = 否。** exact source commit、板端模块 load/NS binding、signed FIT/boot 先决条件未证，未 scp/insmod KO。
