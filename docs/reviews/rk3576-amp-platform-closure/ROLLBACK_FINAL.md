# Rollback Gate：BLOCKED；不得按此部署

已只读识别的 Linux-only 基线：`/boot/uEnv/uEnv.txt` SHA256 `4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64`；`/boot/dtb/rk3576-lubancat-3-v2.dtb` `76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90`；`/boot/Image-6.1.99-rk3576` `e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e`；`boot.cmd` `9f0262e807a8188ec5dffe52411401bd82b5fb0d8e81af8107284725e2a7db55`、`boot.scr` `c498d9be3e8dc91883124cc734be54c42c271fad0501555c1a69f7ea7fc38325`。当前 Linux `6.1.99-rk3576`，DT model `EmbedFire LubanCat-3-v2`，U-Boot 指纹 `8f53f800da-04/24/2026`、BL31 `v1.14`。这些只是文件/版本证据；本轮没有把它们当作完整恢复备份。

若未来 **仅** 用独立 overlay/uEnv 改动且保留可启动 Linux：拟通过 USB-TTL 在 U-Boot 阻止新 AMP 加载，恢复原 `uEnv.txt` 与原 DTB，核 SHA256，再启动 Linux-only。若使用公开 `CONFIG_AMP` loader，则 FIT 在 GPT `amp` 分区而非 `/boot`；当前无该分区，不能写出不依赖未知镜像的具体 rollback 命令。若 Linux 不起，需可靠离线 RockUSB/Maskrom + 原 `uboot` 分区、DDR/BL31 loader、分区表和 boot/rootfs 镜像；当前这些 image 的 hash/备份、恢复介质、救援 PC/线缆/USB-TTL 实际验证均未闭合。

[LubanCat RK3576 官方烧录说明](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/flash_img/flash_img.html) 提供 LubanCat-3 Recovery/Maskrom 入口与 RKDevTool **v3.30+**，但本轮未进入模式，且 v2 实机按钮/线缆/正确完整镜像未核。因此不能给“可执行且不依赖未知文件”的恢复指令。**恢复路径 = BLOCKED**；在首个板端写入审批前必须离线取得完整镜像和实际演练验证。
