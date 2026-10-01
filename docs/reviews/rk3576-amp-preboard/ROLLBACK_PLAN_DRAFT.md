# Linux-only 恢复草案（未执行）

当前 Linux-only 可工作基线：`uEnv.txt` 普通文件 SHA256 `4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64`；`rk3576-lubancat-3-v2.dtb` `76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90`；`Image-6.1.99-rk3576` `e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e`；`boot.cmd` `9f0262e807a8188ec5dffe52411401bd82b5fb0d8e81af8107284725e2a7db55`；`boot.scr` `c498d9be3e8dc91883124cc734be54c42c271fad0501555c1a69f7ea7fc38325`。运行 kernel `6.1.99-rk3576`，DT model `EmbedFire LubanCat-3-v2`；firmware 指纹见 CURRENT_BOARD_BOOT_BASELINE。`uEnvLubanCat3-V2.txt` hash `1bf53a0f2e95349d6034d3b11943225f4d8d4b90994c6c88c8e10f9587f50f9f`，**不能当作现用 uEnv 的字节级替代**。

在下一轮任何写板批准之前：①从板端只读取得完整 `/boot`、U-Boot/eMMC 分区表和必要 raw 分区镜像，离线校验 hashes；②准备 USB-TTL、电源、可启动救援介质与恢复命令，**先演练 Linux-only 恢复路径**；③只在后续已批准的可逆试验中启用单一 AMP change；④串口若无 Linux，使用离线介质还原原分区表/U-Boot/DTB/uEnv/boot 文件，核上表 hashes 后冷启动。仅 SSH 在线恢复不够，因为 U-Boot/DTB 失败时 Linux 可能不可达。

本轮只读复制的 DTB/uEnv 是静态审查证据，没有完整 `/boot`、`uboot` 分区、eMMC 分区表镜像，也未验证救援介质。`amp` 分区不存在；如果方案要求新增分区，恢复成本和风险远超小型 overlay。**Rollback Gate = BLOCKED**，不能以这份草案声称恢复路径已闭合。
