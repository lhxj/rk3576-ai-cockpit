> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# RPMsg 两侧参数对齐

**目前不闭合。** 本轮 M0 echo 取 remote，Linux Rockchip transport 为 virtio master；M0 `RL_PLATFORM_SET_LINK_ID(0,4)=0x04`。参考 Linux DTS 为 CPU3 `0x03`，且当前实板 DTB 无 RPMsg 节点。

| 参数 | 派生 M0 | Rockchip 6.1 参考 Linux | 当前 LubanCat | 判定 |
| --- | --- | --- | --- | --- |
| link-id | `0x04`，`amp_echo.c` | `0x03`，DTS 注释 MCU=`0x04` | 无节点 | **不一致** |
| vring0 | M0 `0x27d00000` | PA `0x47800000` | 无 | remap 未知；参考不匹配 |
| vring1 | M0 `0x27d08000` | PA `0x47808000` | 无 | 同上 |
| 每 ring 大小 / 对齐 | `0x8000` / `0x1000` | `0x8000` / `0x1000` | N/A | 参数本身一致 |
| descriptors / payload | 64 / 496 B，512 B slot | 64 / 496 B，512 B slot | N/A | 参数本身一致 |
| buffer pool | M0 `platform_patova()` 对 Linux PA 减 `0x20000000` | `rpmsg-dma@47a00000`, 2 MiB | 无 | alias / cache 未证明 |
| Mailbox | MBOX0 TX + MBOX4 RX ch0；IRQ175 | 参考 mailbox0 RX + mailbox3 TX；应为 MBOX4 TX 才能谈 M0 | mailbox0/4 disabled | **不一致** |
| NS | `rpmsg_ns_announce(..., RL_NS_CREATE)` | `RPMSG_NS=y`，virtio feature NS | `CONFIG_RPMSG_NS=y` | API/开关吻合；运行未证 |
| endpoint | `0x3004` | Linux动态 endpoint | 无驱动绑定 | Host 源码有候选 peer |
| service | `rk3576-m0-echo` | 现有 `rockchip_rpmsg_test` 仅匹配 `rpmsg-mcu0-test`/`rpmsg-ap3-ch0` | `CONFIG_RPMSG_ROCKCHIP_TEST=n`、CHAR/CTRL/TTY=n | 派生小型 Linux driver 匹配；未按实际 kernel 构建 |

调用链：`applications/main.c` → `amp_echo_run()` → `rpmsg_lite_remote_init()` → M0 virtqueue 绑定 linker `LINUX_RPMSG` → `rpmsg_lite_is_link_up()` 每秒轮询且每 5 秒 log → queue/endpoint → NS announce → 接受精确 `HELLO`/`PING` 字节并回 `HELLO_ACK`/`PONG` → `platform_notify()` 发 MBOX。Linux driver 通过 DTS 创建 virtio，两 ring 和 DMA pool 分别由 `rockchip_rpmsg_mbox.c` 与 `virtio_rpmsg_bus.c` 管理；NS 名称匹配 `rpmsg_device_id`。发送/中断/缓存均未实板运行。

原 Rockchip 例程 `common/tests/rpmsg_test.c` 的 `rpmsg_ns_cb` 被声明成**返回函数指针**，但 `rpmsg_ns.h` typedef 为返回 `void`；本轮派生树将两个条件分支都改成 `static void`，并将会 `INIT_APP_EXPORT` 的非 MOS Linux 测试函数改为 `int`。新 echo 不使用此旧测试服务，`rpmsg_queue_rx_cb` 类型与 `rpmsg_lite_create_ept()` 一致。参考 Linux driver 的具体源码见 [transport](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/drivers/rpmsg/rockchip_rpmsg_mbox.c)、[test](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/drivers/rpmsg/rockchip_rpmsg_test.c)、[参数头](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/include/linux/rpmsg/rockchip_rpmsg.h)。

`scripts/test_amp_layout.py` 对当前固定参考源返回 **exit 1 / BLOCKED (6 failures)**：link、mailbox、FIT reserve、FIT/vring overlap、CON17 vring remap 未证明、CON17 DMA buffer remap 未证明。脚本同时检查 `platform_patova()` 的偏移、M0 DMA 地址窗及 buffer pool 容量；接受将来实际 Linux DTS/头文件和经证明的 remap 值。任一关键参数变化导致不一致时 CI 非零退出。
