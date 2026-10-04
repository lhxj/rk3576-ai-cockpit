> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# Name Service 与 Linux service binding

派生 M0 `applications/amp_echo.c` 在 link-up 后创建 endpoint `0x3004`，调用 `rpmsg_ns_announce(..., "rk3576-m0-echo", RL_NS_CREATE)`。Linux `scripts/board/amp_echo_linux/rk3576_amp_echo_test.c` 的 `amp_echo_id_table[]` 使用同名 `.name=AMP_ECHO_SERVICE`，模块别名经 Host `modinfo` 实测为 `rpmsg:rk3576-m0-echo`。Linux driver `probe()` 发 `HELLO`，callback 收 `HELLO_ACK` 后发 `PING`，收 `PONG`。**SOURCE_VERIFIED + HOST_TESTED**，没有板端传输证据。

预期在 Linux RPMsg NS 创建设备后，`/sys/bus/rpmsg/devices/` 出现名称含 `rk3576-m0-echo` 的设备并由 `rk3576_amp_echo_test` 驱动 bind；具体实例前缀和 endpoint 地址由内核分配，不能预写成固定 sysfs 路径。当前该目录为空。`RPMSG_CHAR/CTRL/TTY` 当前未启用；本方案使用专用 driver，无需先改变这些配置。模块只可在以后确认 exact kernel source、boot 与 RPMsg gate 后考虑上板；本轮 KO 非 BOARD_MODULE_READY。
