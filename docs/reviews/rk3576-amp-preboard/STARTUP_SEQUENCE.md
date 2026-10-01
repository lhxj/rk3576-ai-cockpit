# Gate 4 — 最小启动顺序与失联

**目标顺序：U-Boot → BL31 SMC → BUS M0 RT-Thread → Linux → RPMsg transport → NS → Linux echo driver。** 当前板没有 `amp` 分区且 DTB 不含 RPMsg，所以这只是候选启动链，不是实板验证。由于 `CONFIG_REMOTEPROC=n` 且没有板端用户态 MCU loader，**Linux 先启动、RTOS 后启动**在当前基线没有装载入口，不可作为第一版方案。

1. U-Boot `CONFIG_ROCKCHIP_AMP` 若开启，`amp_cpus_on()` 从分区名 `amp` 读取 FIT，`boot_get_loadable()` 装入 ITS `load`。当前 eMMC 仅 `uboot/boot/rootfs`，此步不能执行；实际 U-Boot 配置未从运行二进制确认。
2. `fit_standalone_release("bus_mcu", entry)` 经 `sip_smc_mcu_config(BUSMCU_0_ID, MCU_CODE_START_ADDR, entry)` 配置 M0 code remap 后释放 M0 clock/reset。`entry` 从 load 回退；CON17 shared remap 未见设置。
3. M0 初始化 vector/NVIC/SysTick/UART5，然后 echo `rpmsg_lite_remote_init()`，每秒检查 `rpmsg_lite_is_link_up()`，每 5 秒输出 heartbeat/等待日志。**不会无声地永久卡在 `rpmsg_lite_wait_for_link_up()`**；旧环境 port 仍忽略该 API timeout，新应用不调用它。
4. Linux 从现有 boot.cmd / DTB 启动；以后须用明确的 LubanCat AMP DT 补丁启用 mailbox0/4、reserved memory、RPMsg node，transport 注册 virtio 设备并初始化 vring/NS。
5. M0 link-up 后创建 queue 与 `0x3004` endpoint，announce `rk3576-m0-echo`；Linux 小型 RPMsg driver 的 id table 必须匹配，probe 发送 `HELLO`，M0 回 `HELLO_ACK`，Linux 发 `PING`，M0 回 `PONG`。

失败行为：若 Linux 未到，M0 每 5 秒记录可诊断等待状态；若创建 queue/endpoint/NS 失败，代码清理 endpoint/queue/instance 并返回主线程。若 Linux 已建链后重启，当前 port 没有经过验证的 link-state 重置和 NS 重宣告，**不支持热重连**。若 RTOS 单侧重启，Linux transport 的 handshake/vring 清理也未验证。首轮受控试验只允许整机从固定顺序冷启动；任何一端异常，先回滚 Linux-only。此项在更改 boot 链前还需串口/日志超时现场验证。

证据：派生 `applications/amp_echo.c`；`rpmsg_env_rt-thread.c:env_wait_for_link_up` 使用 `RT_WAITING_FOREVER`；Linux [transport](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/drivers/rpmsg/rockchip_rpmsg_mbox.c) 的 handshake work；板版本 [LubanCat U-Boot](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/drivers/cpu/rockchip_amp.c)。
