# rtos/board

RK3576 BUS Cortex-M0最小链已真实启动并完成双向RPMsg。固定SDK/派生RTOS/HAL、UART5 M0 pinmux、linker/预留地址、v5时基修正及板端证据统一见 [AMP_RPMSG_INTEGRATION_TIP](../../docs/amp/AMP_RPMSG_INTEGRATION_TIP.md)。

本目录不vendor全SDK；实际改动以集成源patch为准。当前scope冻结，其他外设/RTOS业务没有因echo通过而验收。
