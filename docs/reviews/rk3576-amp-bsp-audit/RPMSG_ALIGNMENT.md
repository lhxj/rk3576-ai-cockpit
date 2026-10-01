# RPMsg-Lite / Linux 参数对齐

参照固定 RTOS/HAL commit 见 `SUMMARY.md`。Linux **参考源码**取 Rockchip kernel `77168c8d5ab82399f65a80e9f807b50ba37cf483` 的 [rk3576-amp.dtsi](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/arch/arm64/boot/dts/rockchip/rk3576-amp.dtsi)、[rockchip_rpmsg_mbox.c](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/drivers/rpmsg/rockchip_rpmsg_mbox.c)、[rockchip_amp.c](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/drivers/soc/rockchip/rockchip_amp.c)、[rockchip_rpmsg.h](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/include/linux/rpmsg/rockchip_rpmsg.h) 和 [rockchip_rpmsg_test.c](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/drivers/rpmsg/rockchip_rpmsg_test.c)。它们**不是**运行中 LubanCat 内核/DTB 的核验结果。

RTOS 重点路径：`R/bsp/rockchip/common/drivers/rpmsg-lite/`，`R/bsp/rockchip/common/tests/rpmsg_test.c`。`H/middleware/rpmsg-lite/` 有独立副本，但 `SConscript` 选 R 内嵌副本，两个 `rpmsg_lite.c` SHA-256 不同，不能混用未经兼容审查的对象。

## 历史实板内核配置与参考代码

下表配置抄自主项目 `docs/STATUS.md:77-86` 的历史实板记录，**不是**本轮对运行内核的重新读取。源码含义按上述固定 Rockchip 6.1 参考树解释。

| 配置 | 历史值 | 对 M0 方案的含义 |
|---|---|---|
| `CONFIG_MAILBOX` / `CONFIG_ROCKCHIP_MBOX` | `y` / `y` | mailbox 框架与 Rockchip 控制器可用；实际 controller/channel/IRQ 仍须与 M0 port 对表。 |
| `CONFIG_RPMSG` / `CONFIG_RPMSG_NS` | `y` / `y` | RPMsg core 与 Name Service 编入；需要 RTOS announce 正确服务名才产生目标 channel。 |
| `CONFIG_RPMSG_ROCKCHIP_MBOX` / `CONFIG_RPMSG_VIRTIO` | `y` / `y` | 与 `rockchip_rpmsg_mbox.c` 的 mailbox + virtio 双 vring 路径相符；不证明当前 DTB 节点已启用或参数相等。 |
| `CONFIG_REMOTEPROC` | `n` | 不能假设 generic remoteproc 会装载/恢复 M0；须单独核对 `rockchip_amp.c`、bootloader/FIT 责任。 |
| `CONFIG_RPMSG_TTY` / `CONFIG_RPMSG_CHAR` / `CONFIG_RPMSG_CTRL` | `n` / `n` / `n` | 无这些通用用户态接口；`rockchip_rpmsg_test.c` 的 `rpmsg-mcu0-test` 匹配仍取决于该 test driver 在真实内核中是否编入。 |

## 调用链与缺口

`common/tests/rpmsg_test.c` 在 `RT_USING_COMMON_TEST_LINUX_RPMSG_LITE` 开启时用 `INIT_APP_EXPORT(rpmsg_linux_test)`；函数调用 `rpmsg_lite_remote_init` → `rpmsg_lite_wait_for_link_up` → `rpmsg_ns_bind` / `rpmsg_queue_create` / `rpmsg_lite_create_ept` → `rpmsg_ns_announce` → `rpmsg_queue_recv` / `rpmsg_lite_send`。`lib/rpmsg_lite/rpmsg_lite.c` 创建两条 virtqueue，`lib/virtio/virtqueue.c` 读写共享 ring；`porting/platform/RK3576/rpmsg_platform.c` 的 `platform_notify` → `HAL_MBOX_SendMsg`，接收 IRQ → `HAL_MBOX_IrqHandler` → `rpmsg_remote_cb` → `env_isr` → `virtqueue_notification`。Linux `rockchip_rpmsg_mbox.c` 请求 `rpmsg-rx`/`rpmsg-tx` mailbox、ioremap vring、`vring_new_virtqueue`，再由 virtio RPMsg NS 建 channel。默认 `.config` **没有开启示例 task**，产品要求的五个 task 均尚不存在。

| 参数 | RTOS 固定代码 | Linux 参考源码 / 对齐判定 |
|---|---|---|
| 角色 | `rpmsg_test.c::rpmsg_linux_test` 调 `rpmsg_lite_remote_init`；M0 是 remote。 | `rockchip_rpmsg_mbox.c` 注册 virtio device 并初始化 vring，Linux 是 master。角色模型一致。 |
| link-id | `rpmsg_platform.h::RL_PLATFORM_SET_LINK_ID(M,R)=((M<<4)&0xf0)|(R&0xf)`；非 MOS 示例 `MASTER_ID=0`、`remote_id=3`，故 `0x03`。 | DTS `rockchip,link-id=<0x03>`，注释明确 `CPU3:0x03; MCU:0x04`。当前**样例数值相同但核心身份错误**；M0 应按经验证的 MCU 值设计，不能直接沿用 CPU3。 |
| vring base / region | `gcc_link.ld.S` M0 视图 `0x27d00000`，`rpmsg_test.c` 从 `__linux_share_rpmsg_start__` 传给 remote init；长 4 MiB。`rk3576-64/build.sh` 的 Linux 侧 RPMsg base 是 `0x47d00000`，暗示 M0 地址可能经 `+0x20000000` 别名映射，但 `MCU_OFFSET` 宏只出现在外设寄存器定义中，**DDR 映射尚未证明**。 | DTS `rpmsg reg=<0x47800000 0x20000>`；driver `rk_set_vring_phy_buf` 从 `reg` 取首 ring。无论别名如何，当前缺两端相同物理地址的证据。**若** M0 `0x27d00000` 映到 `0x47d00000`，ring 相差 `0x500000`，4 MiB 区 `0x47d00000..0x480fffff` 还覆盖 DTS `mcu@48000000..0x481fffff` 的前 1 MiB。 |
| ring 数量/尺寸 | 每 instance 两个 queue；`rpmsg_platform.h` `VRING_SIZE=0x8000`、`VRING_ALIGN=0x1000`；`rpmsg_config.h` `RL_BUFFER_COUNT=64`，`RL_BUFFER_PAYLOAD_SIZE=496` / endpoint 512。`rpmsg_lite.c` ring[1]=base+0x8000，buffer 区从 base+0x10000。 | `rockchip_rpmsg.h` 2 ring、0x8000、0x1000、64、496/512；常数匹配，**地址不匹配**。 |
| buffer pool | RTOS remote 从 ring 后的共享地址访问；测试只检查 `2*RL_VRING_OVERHEAD=0x20000` 不越过 4 MiB，未验证 Linux DMA pool 实际落点。 | DTS `rpmsg-dma@47a00000` 2 MiB、`memory-region`；driver `of_reserved_mem_device_init`。与 RTOS 4 MiB 声明分离；需追实板 DMA 分配地址/可寻址别名和 cache 属性。另有**确定的加载地址冲突**：`Image/amp.its` 的 M0 firmware load `0x47800000` 与 DTS RPMsg ring base `0x47800000` 相同，且 bin 190,968 B 超过 ring node `reg` 的 128 KiB。 |
| mailbox / IRQ | `rpmsg_platform.c` 非 MOS `rl_pMBox[0..7]`，`mbox_clr[MBOX_CNT]` **只填 0..3**；M0 收 `MBOX_BB0..3_IRQn`，注册时按 remote ID 选 client，通知 queue0 用 master-ID MBOX、queue1 用 remote-ID MBOX；`MBOX_CH_0`。`H/.../soc.h` `MBOX_CNT=14`、BUS M0 `MBOX_BB0_IRQn=9`，其余经 INTMUX。 | DTS `mboxes=<&mailbox0 0 &mailbox3 0>`，CPU3 的 IRQ route `113/174` 指向 A-core CPU3。对 `remote_id=3` 是代码形态匹配；**不证明 MCU 的 controller/IRQ**。若简单改 `remote_id=4`，`mbox_clr[4]` 为零初始化，现 port 无有效 client/IRQ。 |
| MBOX payload | `rpmsg_config.h` `RL_RPMSG_MAGIC=0x524D5347`，CMD 低 8 位 link-id；`platform_notify` 发送。 | `rockchip_rpmsg.h` 同 magic，driver 同样取低 8 位；协议常数匹配。RTOS `rpmsg_remote_cb` 只在首通知时送 queue0，其后均送 queue1，依赖未证明的握手顺序。 |
| NS / service | library 含 `rpmsg_ns.c`；测试 `rpmsg_ns_announce(...,"rpmsg-mcu0-test",RL_NS_CREATE)`，endpoint `0x3004`；`.config` 测试关闭，默认 ELF 无 announce。 | Linux transport `get_features` 宣告 NS；`rockchip_rpmsg_test.c` 的 ID 表包含 `rpmsg-mcu0-test`、`rpmsg-ap3-ch0`。**源码名称可匹配**，但运行内核该 test driver 是否启用/存在未证；`CONFIG_RPMSG_CHAR=n`、`TTY=n`，无通用用户态设备保证。 |
| cache / barrier | `RT_USING_CACHE=y`，`SystemInit` 开 cache；`platform_cache_all_flush_invalidate` 实体空，clean/invalidate 被注释；`platform_notify` 非 MOS 的调用也注释。`env_mb/rmb/wmb` → `MEM_BARRIER` → ARM GCC `dsb`。 | Linux `ioremap` vring、virtio/DMA API；仅屏障匹配不能证明两端共享 buffer 一致。须明确共享区 cache 属性或实现双向 range clean/invalidate 并验证。 |
| 等待 / 生命周期 | `env_wait_for_link_up` 用 RT-Thread event，但传入 `timeout_ms` 被忽略，`RT_WAITING_FOREVER`；`env_init` 有 semaphore、`env_isr` 直接通知 virtqueue。测试 `rpmsg_queue_recv(...RL_BLOCK)` 100 次；没有 destroy ept、queue、NS unbind 或 deinit；重启 epoch/重同步逻辑未见。 | driver 有 probe/remove、handshake；`rockchip_amp.c` 提供 SIP 启动控制而非 remoteproc 装载。Linux 或 M0 单侧重启后的再握手、清 ring、重建 endpoint 无共同协议证明。 |

## 代码级风险与结论

- `rpmsg_test.c::rpmsg_ns_cb` 声明成 `rpmsg_ns_new_ept_cb rpmsg_ns_cb(...)`，与 `rpmsg_ns.h` typedef `void (*)(...)` 不符；启用示例 Host 构建确有 `-Wincompatible-pointer-types`。`INIT_APP_EXPORT` 同样注册了 `void` 返回函数，框架要求 `int`。
- 测试收到的数据按 `%s`/`strlen` 处理，未由 `payload_len` 保证终止；这不是未来控制/状态协议可复用的安全序列化。即使服务可联通，也需实现有界 payload、长度/版本/seq 校验。
- `RPMSG_CMD` 组件源码在 `R/components/rpmsg_cmd/`，默认 `CONFIG_RT_USING_RPMSG_CMD` 未开；它不是已就绪的 Linux/M0 服务。
- echo/ping-pong 范例确实存在：`rpmsg_test.c` 与 `rk3576-64/applications/fl_rpmsg_test.c`；默认 M0 固件不带 Linux 测试 task。Linux test ID 表能匹配 announce 名，但用户给出的配置只保证 transport 组件，不保证 test 模块或用户态节点。

结论：**RPMsg-Lite 代码框架和 RK3576 port 存在，M0 ↔ 当前 LubanCat Linux 的完整通信链未证明且参考参数冲突。** 当前不能据 Host 构建把它评为完整 port；先在离板 worktree 提交可审查的地址、MBOX/IRQ、cache 与生命周期修订，再谈受控板测。
