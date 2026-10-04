# rtos/rpmsg

状态：**BOARD_TESTED_MINIMAL_ECHO / FROZEN**。RK3576 BUS M0 RT-Thread RPMsg-Lite remote已与配套Linux完成HELLO/HELLO_ACK和PING/PONG，测试后冷恢复默认Debian通过。

运行源码由固定SDK import及项目patch提供，不vendor完整SDK：[源补丁顺序](../../patches/rk3576-amp-integration/series.json)。应用为bsp/rockchip/rk3576-mcu/applications/amp_echo.c；Linux peer在scripts/board/amp_echo_linux。M0 console只作取证，消息实际走RPMsg/mailbox/共享内存。

当前交接、参数/hash和实板证据见 [AMP_RPMSG_INTEGRATION_TIP](../../docs/amp/AMP_RPMSG_INTEGRATION_TIP.md)。用户冻结最小链，不开发RTOS业务/心跳/sensor/control，不合UI/Voice/Media；未声称用户态ABI或热重连/长期稳定性完成。
