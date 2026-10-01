# 当前 LubanCat-3 v2 只读启动基线（2026-10-01）

只执行 `scripts/board/amp_readonly_inventory.sh`、`amp_readonly_boot_files.sh` 经 SSH 的 `uname/cat/ls/find/grep/sha256sum/lsblk`，并在 Host 对读取的 DTB 用 `dtc` 反编译；板端没有写入、扫描 I2C、重启或触发 AMP。原始证据在项目忽略目录 `artifacts/local/amp-boot-20261001T120005Z-100504/` 和 `artifacts/local/amp-boot-files-20261001T120801Z-106653/`，这些 Host 只读副本**不是经过恢复演练的正式备份**。

| 项 | 当前值 |
| --- | --- |
| 板 / SoC | `EmbedFire LubanCat-3-v2`；compatible `embedfire,rk3576-lubancat-3-v2`, `rockchip,rk3576` |
| Linux | Debian 12；`6.1.99-rk3576`；boot cmdline `console=ttyFIQ0 console=tty1` |
| boot firmware 自报 | `ddr-v1.09-2f85f4b2d4`, `bl31-v1.14`, `bl32-v1.05`, `uboot-8f53f800da-04/24/2026`；仅版本指纹，不证明当前编译选项 |
| `/boot/uEnv/uEnv.txt` | **普通文件**，SHA256 `4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64`；`enable_uboot_overlays=1`，仅 CAM0 OV8858 overlay active；UART5 m0 overlay 注释 |
| `/boot/uEnv/uEnvLubanCat3-V2.txt` | SHA256 `1bf53a0f2e95349d6034d3b11943225f4d8d4b90994c6c88c8e10f9587f50f9f`；不是当前 uEnv.txt 的逐字副本 |
| `/boot/dtb/rk3576-lubancat-3-v2.dtb` | 307,297 B，SHA256 `76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90`；已 Host 只读复制并反编译。运行 FDT 本体不可读，不能证明其哈希与 boot 文件相同 |
| `/boot/Image` | 指向 `Image-6.1.99-rk3576`，SHA256 `e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e` |
| boot script | `boot.cmd` SHA256 `9f0262e807a8188ec5dffe52411401bd82b5fb0d8e81af8107284725e2a7db55`；`boot.scr` SHA256 `c498d9be3e8dc91883124cc734be54c42c271fad0501555c1a69f7ea7fc38325` |
| eMMC | `mmcblk0p1 uboot 8M`, `p2 boot 128M`, `p3 rootfs 29G`；**无 `amp` PARTLABEL**，`/dev/disk/by-partlabel/amp` 不存在 |
| kernel config | `MAILBOX/ROCKCHIP_MBOX/RPMSG/RPMSG_NS/RPMSG_ROCKCHIP_MBOX/RPMSG_VIRTIO/ROCKCHIP_AMP=y`；`REMOTEPROC/RPMSG_CHAR/RPMSG_CTRL/RPMSG_TTY/RPMSG_ROCKCHIP_TEST=n` |
| 运行设备 | `/sys/rk_amp`、remoteproc、mailbox class 均不存在；RPMsg bus 存在但无设备；dmesg 有限关键词无 RPMsg/AMP 初始化 |
| DTB 静态节点 | 无 `rockchip,rpmsg`/AMP reserved-memory；mailbox0/4、UART5 disabled；I2C3 okay 且有 RTC `hym8563@51`，I2C6/7/8 disabled；仅 `ramoops@40110000+0xe0000` 等既有预留 |

boot.cmd 从 uEnv 导入环境、装 `/boot/Image` 和 DT/overlay；没有看到 AMP 固件加载逻辑。AMP U-Boot loader 源码从专用 `amp` 分区读取，与当前三分区布局不相容。若将来选择其他装载机制，需要独立审查，不能擅猜 FIT 放在 `/boot` 即能启动。
