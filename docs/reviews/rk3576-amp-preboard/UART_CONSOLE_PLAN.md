# Gate 5 — UART5 控制台

候选 `board/common/iomux_base.c:uart5_m0_iomux_config()` 实际设置 `GPIO_BANK3` 的 `GPIO_PIN_D4/D5` 为 `PIN_CONFIG_MUX_FUNC9`；派生 `board/evb/iomux.c` 在 `RT_USING_UART5` 时调用。`board/evb/defconfig` 的 console 是 `uart5`、115200。LubanCat-3 官方 40Pin 文档列物理 **16=GPIO3_D4/UART5_RX_M0**、**18=GPIO3_D5/UART5_TX_M0**；v2 硬件变更单未列这两脚的变更，仍须在 v2 原理图和下一轮无电连接检查中复核。不能仅信候选提交注释。[厂商 40Pin 说明](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/40pin/40pin.html)。

当前板：运行 DTB `serial@2ad80000` 即 UART5 `status="disabled"`，`uEnv.txt` 中 `rk3576-lubancat-uart5-m0-overlay.dtbo` 被注释；Linux `console=ttyFIQ0 console=tty1`，不占 UART5。候选 Rockchip `rk3576-amp.dtsi` 使用 **UART5 m2** `GPIO4_B0/B1`，与这套 m0 管脚不一致，不能整份 include。Linux AMP DT 应保留 UART5 Linux disabled，并避免 pinctrl 抢 `GPIO3_D4/D5`；M0 配 UART5 m0；USB-TTL 按 3.3V UART 接 40Pin 16/18/GND，由下一轮上板计划明确接线，Linux debug UART0 仍走现有调试口。

现阶段无 M0 UART 输出，v2 实物管脚和电压须再核；**板级适配未闭合**。当前 DTB SHA-256 `76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90`，只读反编译在本地忽略目录 `artifacts/local/amp-boot-files-20261001T120801Z-106653/`。
