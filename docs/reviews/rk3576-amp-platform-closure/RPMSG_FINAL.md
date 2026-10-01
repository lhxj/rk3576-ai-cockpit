# RPMsg Gate 裁决：SOURCE_VERIFIED 局部链，端到端 BLOCKED

最小链：`applications/amp_echo.c:amp_echo_run()` → `rpmsg_lite_remote_init(RPMSG_LINUX_MEM_BASE,0x04)` → 等待 link-up（每 5 s heartbeat）→ `rpmsg_queue_create()` → 固定 endpoint `0x3004` → `rpmsg_ns_announce(...,"rk3576-m0-echo",RL_NS_CREATE)` → HELLO/PING 回 HELLO_ACK/PONG。Linux 专用测试 driver 的 `rpmsg_device_id.name` 相同。初始化失败会 deinit；运行中若 Linux 热重启/失联，固件只打印 link heartbeat，**完整热重连未实现**；首次受控测试应要求冷启动次序。

`RPMSG_LINUX_MEM_BASE` 来自 linker `__linux_share_rpmsg_start__=0x27d00000`，M0 是 remote；Linux `rockchip_rpmsg_mbox.c`/virtio 为 master。`VRING_SIZE=0x8000`，`VRING_ALIGN=0x1000`，两 ring 紧邻；`RL_BUFFER_COUNT=64`，payload=496 B、含 16 B header 的 slot=512 B，与参考 Linux 头宏数值匹配。**M0 地址与 Linux PA 之间的 CON17 base 未证；buffer pool PA/M0 双视图未证。** CPU3 参考 DTS 的 vring0 `0x47800000`、ring1 `0x47808000`、DMA `0x47a00000` 不能直接作为 M0 最终值。

`scripts/amp/check_platform_contract.py` 读取 contract、ITS/linker、M0 echo、HAL config、Linux driver 和给定 DTS；目前对 CPU3 参考 DTS 返回非零，指出 link3/MBOX3 和未知最终 PA。它是阻止错配的 Host gate，**失败是期望且不可绕过**。前轮 callback 类型修复保留；本轮 clean M0 build 无 incompatible pointer warning。
