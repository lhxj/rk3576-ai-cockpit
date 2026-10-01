# UART5 M0 console：UNVERIFIED

派生 M0 `board/common/iomux_base.c:uart5_m0_iomux_config()` 设置 `GPIO3_D4/D5` 为 UART5 m0 func9；`board/evb/iomux.c` 由 `RT_USING_UART5` 调用；echo defconfig console `uart5`/115200。LubanCat [40Pin 文档](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/40pin/40pin.html) 将物理 16/18 标为 `GPIO3_D4 UART5_RX_M0` / `GPIO3_D5 UART5_TX_M0`。当前板 DT 的 `serial@2ad80000` 为 disabled，已挂 pinctrl `uart5m0-xfer`，其 `rockchip,pins=<3 0x1c 9 ... 3 0x1d 9 ...>` 与 RTOS 源吻合；Linux console 为 `ttyFIQ0`+`tty1`，uEnv 中 UART5 m0 overlay 注释（BOARD_OBSERVED_READONLY）。参考 Rockchip AMP DTS 用 UART5 **m2**，不能沿用其 pinctrl。

需要 v2 原理图/实物确认 40Pin 路由、电平与 3.3 V USB-TTL 接法；确认所有当前 overlay/pinctrl 没占 GPIO3_D4/D5，以及 Linux AMP DTS 不同时使能 UART5 作为 Linux consumer。当前未接线、不改 pinmux、不读寄存器。**AVAILABLE 不能成立；判定 UNVERIFIED。** Linux 原 console 保持独立；首次测试若获批准，只允许 M0 UART5 banner 与 Linux 既有 console 并行观测。
