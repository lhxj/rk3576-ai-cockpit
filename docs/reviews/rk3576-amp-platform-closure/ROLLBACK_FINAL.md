# 整机恢复实测：PASS（用户确认）；AMP changeset rollback：BLOCKED

## P025：恢复后基线与精确清单

已普通只读重新核六项boot hash及fwver/model/GPT，并复制保留symlink的原件tar；原hash未变。最新tarSHA=`ed3ef563e21be440de84f68f57093d1e977e6cc10db25d41e6b682ed312b0b7c`。新增 [部署/回滚步骤](P025_DEPLOYMENT_AND_ROLLBACK.md)、[机器changeset](P025_DEPLOYMENT_CHANGESET.json)、[manifest](P025_ARTIFACT_MANIFEST.json)：旧/新文件身份、两个原symlink、Linux可进入时的有限恢复、early loader失败时已核完整update.img/MR恢复路径。整机恢复用户实测PASS保持；用户数据/GPT方案、unsigned策略仍BLOCKED，不运行恢复命令。以下早期“恢复后hash未提供”已由本次普通SSH部分补齐，不覆盖raw U-Boot/BL31当前hash。

## 用户实测更新（2026-10-03）

用户明确确认 **“整机恢复实测通过”**，证据等级 **BOARD_OBSERVED_USER_REPORT**。整机恢复通路按用户确认记 PASS，无需重复演练；不是 Agent 执行的刷写。具体模式、刷入镜像和恢复后 hash 未提供，不能声称 MASKROM/LOADER 两者均验证。见 [RECOVERY_USER_CONFIRMATION.md](../rk3576-amp-board-evidence/RECOVERY_USER_CONFIRMATION.md)。

下文原 boot/镜像 hash 仍用于识别既有备份，不自动代表恢复后当前板状态。最终 AMP changeset 未确定，用户数据备份和逐文件 rollback 仍需闭合；公开 loader 早于 prompt 的风险保持。当前 **C. HOST_BUILD_PASS**，不得据此部署 AMP。

## P024补充（2026-10-03）：恢复文件已核，设备入口待验证

完整官方恢复镜像已取得、发布MD5/内部content MD5/SHA256通过；U-Boot/BL31/Kernel/DTB/boot脚本与当前板原件全字节一致。RKDevTool3.32已放桌面，微软签名Rockusb5.14预装exit0。详情见 [RECOVERY_IMAGE_ANALYSIS.md](RECOVERY_IMAGE_ANALYSIS.md)、[HOST_RECOVERY_TOOLS.md](HOST_RECOVERY_TOOLS.md)。以下旧段落保留当时缺口；它们不表示本轮仍未下载镜像。

### 已确定的恢复对象

- 完整固件：`C:\Users\27432\Desktop\lubancat-rk3576-debian12-gnome-20260424_update\lubancat-rk3576-debian12-gnome-20260424_update.img`。
- SHA256=`18247661a898821a751262d57f18f6edf6aab0f62b4b706cfc1b74a4ea277a66`；发布MD5=`11c82c312eb2d1746e0b380671a48a2f`。
- 工具：`C:\Users\27432\Desktop\RK3576-Recovery-Tools\RKDevTool_v3.32_for_window\RKDevTool.exe`，桌面有快捷方式。
- 当前boot/uEnv原件及8MiB uboot分区：同目录 `Original-Board-Boot-20261002`。Host/桌面两份hash一致，见工具报告。
- factory配置是uboot/boot/rootfs三分区，没有amp。整包包含DDR/loader、U-Boot/ATF/OP-TEE、boot和rootfs，不依赖尚未找到的恢复文件。

### 未来获批后的分级恢复程序（本轮未执行）

1. **仅boot文件改变且Linux可正常进入**：先对原件tar和manifest验hash，在Host将“本次实际改过的文件”提取成单独恢复包，保留类型/权限/symlink；通过批准的传输/写回流程恢复这些文件。当前active uEnv应恢复为SHA=`4f7fec...`的6061 B版本，保留CAM0 overlay；不要用factory329 B通用uEnv覆盖它。Image/DTB/脚本原hash见下面历史表；写回后逐项核hash。当前没有实板changeset，因此不能提前写一个覆盖整个boot的通用命令。
2. **Linux不起、但U-Boot还能进入prompt**：已有debug串口1500000 8N1可中断观察；若只改Linux DT/boot，可按确切changeset恢复文件。公开AMP loader在board_late_init执行，不能保证prompt早于M0 release；没有已确认的AMP disable env，不能把恢复uEnv当成停止早期AMP加载的方法。
3. **U-Boot/分区改变或M0在prompt之前导致失败**：使用官方MR/Maskrom入口，PC连板的Type-C OTG数据口；断开所有可能供电，按住板上MR，再接电，RKDevTool识别一个MASKROM设备后放开。在“升级固件”页选上述**完整**已核update.img并升级，恢复官方三分区/Linux-only系统。不要把该RKFW当作raw整盘镜像使用，或把Host提取的payload当成完整恢复固件。操作步骤来自[官方RK3576恢复文档](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/flash_img/flash_img.html)。若原系统正常且仅需LOADER模式，官方REC入口为按住REC上电，识别LOADER后放开；早期AMP故障优先依赖不经过U-Boot的MR入口。
4. **阻止反复加载RTOS**：在改过AMP U-Boot/GPT的场景，恢复官方整包会回到已核旧U-Boot和无amp的三分区设计；不能只删Linux overlay就声称停止U-Boot加载。未来若建立无GPT加载路径，必须将其disable/rollback规则写入最终changeset。
5. 官方系统恢复成功后，先只启动Linux、确认版本/DT/boot，再恢复原active uEnv及必要原boot文件和用户数据；不自动重装AMP。第一次启动所需v2 DTB在该已核官方包中。不要先加载M0/echo模块验证恢复。

整包恢复会重写eMMC/rootfs；已有boot tar不保存应用、模型和其它用户数据。**用户数据备份范围、实际OTG数据线/USB枚举、MR可达性和改后精确rollback清单仍待闭合**。本轮没有进入MR/REC、没有刷写或重启。Recovery物料/文件身份PASS，Recovery实机入口UNVERIFIED，整体gate仍BLOCKED。

## 历史裁决（2026-10-01）

已只读识别的 Linux-only 基线：`/boot/uEnv/uEnv.txt` SHA256 `4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64`；`/boot/dtb/rk3576-lubancat-3-v2.dtb` `76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90`；`/boot/Image-6.1.99-rk3576` `e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e`；`boot.cmd` `9f0262e807a8188ec5dffe52411401bd82b5fb0d8e81af8107284725e2a7db55`、`boot.scr` `c498d9be3e8dc91883124cc734be54c42c271fad0501555c1a69f7ea7fc38325`。当前 Linux `6.1.99-rk3576`，DT model `EmbedFire LubanCat-3-v2`，U-Boot 指纹 `8f53f800da-04/24/2026`、BL31 `v1.14`。这些只是文件/版本证据；本轮没有把它们当作完整恢复备份。

若未来 **仅** 用独立 overlay/uEnv 改动且保留可启动 Linux：拟通过 USB-TTL 在 U-Boot 阻止新 AMP 加载，恢复原 `uEnv.txt` 与原 DTB，核 SHA256，再启动 Linux-only。若使用公开 `CONFIG_AMP` loader，则 FIT 在 GPT `amp` 分区而非 `/boot`；当前无该分区，不能写出不依赖未知镜像的具体 rollback 命令。若 Linux 不起，需可靠离线 RockUSB/Maskrom + 原 `uboot` 分区、DDR/BL31 loader、分区表和 boot/rootfs 镜像；当前这些 image 的 hash/备份、恢复介质、救援 PC/线缆/USB-TTL 实际验证均未闭合。

[LubanCat RK3576 官方烧录说明](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/flash_img/flash_img.html) 提供 LubanCat-3 Recovery/Maskrom 入口与 RKDevTool **v3.30+**，但本轮未进入模式，且 v2 实机按钮/线缆/正确完整镜像未核。因此不能给“可执行且不依赖未知文件”的恢复指令。**恢复路径 = BLOCKED**；在首个板端写入审批前必须离线取得完整镜像和实际演练验证。
