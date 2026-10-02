# P023 恢复原件与离线恢复路径

**Recovery gate仍BLOCKED**；以下是已取得原件及待批准的恢复程序，未恢复/写入/重启任何设备。

## 已复制到Host的原件

`artifacts/local/p023-board-backup/boot-originals-complete.tar` SHA256：
`d893cc9fc7fa379cfc127565142ef3c48be3d67ac8df3d483f9a8743ca37130d`。

包含`Image` symlink及实际`Image-6.1.99-rk3576`、当前kernel config/initrd/System.map、`boot.cmd/boot.scr`、完整`uEnv`目录、`extlinux`和v2 DTB。不是整盘/rootfs/GPT备份。`running-device-tree.tar` SHA256：`61fae8ebac570606f536a8f4817425d04bf0226701bf97b3fd87cc59f7b01dde`。

| 恢复对象 | 原SHA256 |
| --- | --- |
| `/boot/uEnv/uEnv.txt`（regular file） | `4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64` |
| `/boot/dtb/rk3576-lubancat-3-v2.dtb` | `76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90` |
| `/boot/Image-6.1.99-rk3576` | `e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e` |
| `/boot/boot.cmd` | `9f0262e807a8188ec5dffe52411401bd82b5fb0d8e81af8107284725e2a7db55` |
| `/boot/boot.scr` | `c498d9be3e8dc91883124cc734be54c42c271fad0501555c1a69f7ea7fc38325` |
| 前轮8MiB `uboot`分区原件 | `ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a` |

版本由本轮只读cmdline再次确认：Kernel`6.1.99-rk3576 #8`、U-Boot`8f53f800da-04/24/2026`、BL31`v1.14`、BL32`v1.05`、DDR`v1.09-2f85f4b2d4`。前轮atf-1 payload SHA=`1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07`；与SDK rkbin固定pin对应PT_LOAD一致。版本/hash不等于当前AMP功能或签名策略证明。

## 官方整机恢复介质

选择用户可取得的 **`lubancat-rk3576-debian12-gnome-20260424_update.7z`**，与当前Debian12发行日期对应。官方发布页给其解压后`update.img` MD5=`11c82c312eb2d1746e0b380671a48a2f`，约5.39GB；下载后仍须实际计算MD5和SHA256，并静态核`parameter/package-file/loader/uboot/boot/rootfs`，不能只看压缩包名称。[官方20260424发布记录](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/baidu_cloud/update_history.html)

本轮桌面只发现`.7z.baiduyun.p.downloading`临时文件，没有完整archive或`update.img`。因此没有解压厂商loader、没有可核对的GPT/镜像payload、也没有形成可执行的“写回本文件”审批单。不能把文件存在当作下载完整。

官方RK3576路径：Windows安装DriverAssistant，使用**RKDevTool v3.30或更新**；PC连板OTG数据口，在断开所有供电后按住MR再供电，设备识别为MASKROM后放开；REC入口则对应LOADER。选择匹配`update.img`，由固件升级页恢复整个系统。按钮在用户规格书PDF第22/23页有板级示意。本轮没有进入MR/REC、没有安装驱动或执行刷写。[官方RK3576烧录说明](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/flash_img/flash_img.html)

## 按故障等级恢复

1. **Linux可启动且仅boot文件改变**：未来批准后，把Host tar放在恢复环境，按manifest验证tar和成员hash；恢复原uEnv/DTB/实际Image/boot脚本及Image symlink，恢复后再次sha256核对。不要直接覆盖整`/boot`；恢复清单应针对真实已改文件。
2. **Linux不可启动、原U-Boot仍可进console**：已有Debug USB-TTL（1500000 8N1）可中断启动，但当前没有官方AMP disable env开关。公开loader在`board_late_init()`调用，可能早于命令行；不能承诺敲掉uEnv就阻止它。本轮不设计必须依赖未知命令/镜像的serial写回路线。
3. **改过U-Boot/分区或M0在prompt前出故障**：需要上述可识别的完整update镜像、RKDevTool/USB驱动/OTG线与MR入口。整机恢复会覆盖eMMC及用户rootfs数据；已复制的boot原件不能替代用户数据备份。恢复完成后可进一步用已核原件恢复Linux-only boot，但须单独批准。

**未閉合：** 完整update.img内容/hash、first-stage DDR loader/parameter一致性、PC工具/OTG识别、实际M0 TTL电平/接线、变更后的确切恢复清单，以及如何在新loader重复失败时阻止再次加载RTOS。当前无amp分区；不创建它，也不以不明“amp file”代替公开GPT入口。
