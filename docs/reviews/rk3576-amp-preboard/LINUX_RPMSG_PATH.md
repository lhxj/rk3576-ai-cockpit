# Linux 最小 echo 选择：B，小型匹配 service 的 kernel test driver

当前运行 kernel 的 `CONFIG_RPMSG_ROCKCHIP_MBOX=y`, `RPMSG_VIRTIO=y`, `RPMSG_NS=y`，但 `RPMSG_ROCKCHIP_TEST/CHAR/CTRL/TTY=n`。参考 Rockchip `rockchip_rpmsg_test.c` 的 id table 只含 `rpmsg-ap3-ch0`、`rpmsg-mcu0-test`，业务报文是 `Rockchip rpmsg linux test!`，不会主动发 `HELLO/PING`。单纯启用 test 或 RPMSG_CHAR 不能验证本轮协议/服务。故选择 **B**。

Host 候选 `scripts/board/amp_echo_linux/rk3576_amp_echo_test.c` 的 `rpmsg_device_id` 精确匹配 M0 NS `rk3576-m0-echo`；probe 发 `HELLO`，收 `HELLO_ACK` 后发 `PING`，收到 `PONG` 记录日志。此文件**未用当前板 exact kernel headers 编译、未装载**。内核实际源 commit/ABI 尚未锁定；若下一轮验证该模块，需先取得 `6.1.99-rk3576 #8 Apr 24 2026` 对应源码/构建头和 module signing/config，并 Host 编译。无需默认启用 CHAR/CTRL/TTY。若决定内建，则需明确 kernel 配置/打包 changeset；本轮不修改 kernel。

`CONFIG_RPMSG_NS=y` 只负责建立 channel；service name 由 M0 `rpmsg_ns_announce()` 发，Linux id table 负责绑定。参考 Linux transport `rk_set_vring_phy_buf()` 从 DTS `reg` 取 vring PA，`of_reserved_mem_device_init()` 接 DMA pool，`rockchip,link-id` 直接读取 DTS。当前板没有该 DT node，因此驱动即使编译也无 transport device。来源：[transport](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/drivers/rpmsg/rockchip_rpmsg_mbox.c)、[Rockchip test](https://github.com/rockchip-linux/kernel/blob/77168c8d5ab82399f65a80e9f807b50ba37cf483/drivers/rpmsg/rockchip_rpmsg_test.c)。
