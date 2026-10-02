# Linux-only recovery：识别对象与缺口

## 最新补充（2026-10-03）：整机恢复实测通过

用户明确确认 **“整机恢复实测通过”**，`WHOLE_BOARD_RECOVERY_TEST=PASS`，证据等级 **BOARD_OBSERVED_USER_REPORT**；Agent 本次没有访问开发板。无需重复整机恢复演练。具体模式、实际刷入镜像和恢复后 hash 未提供，不外推为两种模式均通过或恢复后仍与旧 hash 一致。详见 [RECOVERY_USER_CONFIRMATION.md](RECOVERY_USER_CONFIRMATION.md)。

恢复物料/文件身份和用户整机恢复实测已闭合；AMP 变更后的精确 rollback、用户数据备份和恢复后只读基线仍需形成。整体保持 **C. HOST_BUILD_PASS**。以下历史缺口保留其当时语境；P024 已核完整镜像、工具与原 boot 副本见 [ROLLBACK_FINAL.md](../rk3576-amp-platform-closure/ROLLBACK_FINAL.md)。

## 当前可核的原配置，BOARD_OBSERVED_READONLY

`/boot` 在 eMMC `mmcblk0p2` ext2，Linux 从 `mmcblk0p3` rootfs 启动。`/boot/boot.cmd` 导入 `uEnv/uEnv.txt`，加载 `/Image`、`/rk-kernel.dtb`/overlay 后 `booti`；当前运行 DT 为 LubanCat-3 v2 Linux-only，无 AMP/RPMsg 节点。下表是**板上当前 hash 清单，不是本轮已做的离线备份**：

| 板端对象 | SHA256 |
| --- | --- |
| `/boot/uEnv/uEnv.txt` | `4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64` |
| `/boot/boot.cmd` | `9f0262e807a8188ec5dffe52411401bd82b5fb0d8e81af8107284725e2a7db55` |
| `/boot/boot.scr` | `c498d9be3e8dc91883124cc734be54c42c271fad0501555c1a69f7ea7fc38325` |
| `/boot/Image` → `Image-6.1.99-rk3576` | `e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e` |
| `/boot/dtb/rk3576-lubancat-3-v2.dtb` | `76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90` |

Board fingerprints：U-Boot `8f53f800da-04/24/2026`，BL31 `v1.14`；**actual binary hash 未知**。普通 `cat` 用户不能读 `uboot` 原始分区；无 BL31/U-Boot 文件在 `/boot` 或常见软件目录。没有已验证的 bootloader image、BL31 image、分区表全量备份、救援介质或 USB-TTL 实际接线/串口启动观察点。

## 后续证据更新（2026-10-02）

上段“actual binary hash 未知/未观察串口”是初次盘点状态。后续已只读复制 8 MiB `uboot` 分区，SHA256 `ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`，并提取 BL31；见 [CON16_CON17_RUNTIME_DIAGNOSTICS.md](CON16_CON17_RUNTIME_DIAGNOSTICS.md)。用户在 MobaXterm 串口确认实际 U-Boot 版本与 bootcmd，CON16 直接读取异常后，重新上电 Linux 正常启动；见 [UBOOT_SERIAL_RUNTIME.md](UBOOT_SERIAL_RUNTIME.md)。该恢复为 **BOARD_OBSERVED_USER_REPORT**，不是 Agent 执行的恢复演练。

仍缺 `/boot` 原件的完整离线备份、GPT/第一阶段 loader/rootfs 备份，以及经过核验的救援镜像/介质和 Maskrom 恢复流程；本次正常启动不能覆盖这些缺口。

## 已知官方恢复入口及其限制

[野火 LubanCat RK3576 烧录说明](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/flash_img/flash_img.html) 说明 RKDevTool ≥v3.30/DriverAssistant、Type-C OTG、Recovery 与 Maskrom 入口；U-Boot 无法正常进入时需 Maskrom。其 [镜像更新说明](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/baidu_cloud/update_history.html) 列 `lubancat-rk3576-debian12-gnome-20260424_update.img`（发布 MD5 `11c82c312eb2d1746e0b380671a48a2f`），包含 LubanCat-3 v2 与 `6.1.99-7` 内核；它与板端日期和 kernel package 相近，**仅是可能的恢复镜像，INFERRED**。未下载并验证该镜像/MD5，不能声称它逐字还原当前 bootloader、BL31、分区或用户数据。全量刷写会影响项目数据。

若未来仅改 Linux boot files、U-Boot/BL31/GPT 保持完整，可通过离线访问 boot 分区恢复上表原件并核 SHA256 后回到 Linux-only；但具体命令需建立在原件已离线保存、串口和介质已验证的事实之上。若涉及公开 `CONFIG_AMP` loader 所需 `amp` GPT 分区或替换 bootloader，则必须另备原始分区表、`uboot`/loader/BL31 与 rootfs 数据，并验证 Maskrom/RockUSB 完整镜像恢复链。**当前未满足“具体可执行且不依赖未知文件”的 rollback；RECOVERY_BLOCKED。** 本轮没有备份、刷写或进入 Recovery/Maskrom。
