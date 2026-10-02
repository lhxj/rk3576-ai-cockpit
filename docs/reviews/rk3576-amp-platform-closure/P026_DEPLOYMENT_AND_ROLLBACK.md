# P026 精确部署/回滚方案：待审批，未执行

2026-10-03。整体C、D准入关闭。下面是审核对象，**不是本轮执行指令**。所有boot写入、firmware写入、重启、M0启动必须在后续范围明确的审批中处理。机器表 [P026_DEPLOYMENT_CHANGESET.json](P026_DEPLOYMENT_CHANGESET.json) 包含每个source/destination、old/new SHA、软链接文本hash、风险与恢复。

## 1. 先保存原件，再考虑改动

Windows独立恢复副本已真实复制并核SHA，目录：`C:\Users\27432\Desktop\RK3576-Recovery-Tools\P026-Originals-20261003`。这个副本不依赖未来待测Linux可启动，也不依赖WSL能联网。

| 原件 | Size/SHA256 |
| --- | --- |
| `uboot-original-8MiB.raw` | 8388608；`ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a` |
| `boot-originals.tar`，保留选定boot原件/链接 | 57006080；`ed3ef563e21be440de84f68f57093d1e977e6cc10db25d41e6b682ed312b0b7c` |
| `stock-modules.tar` | 19240960；`e467fbba7290759f9b6838ae9c16ea83a1dd0a7aec0f9412eb0bfdd2d973300e` |
| `factory-MiniLoaderAll.bin`，原恢复包Boot payload | 770553；`4c0d04c6234b6e783aeedf317f4bb661700d9ef2ef97f876e67597807aa86206` |
| `factory-parameter.txt`，原GPT描述 | `e93b841868d8c07991b4ac47875ce77a5692807ccf247ceb5d5e062303d8e193` |

板端sysfs已读到sector：uboot start=`0x4000`、size=`0x4000`（8MiB），boot start=`0x8000`、size=`0x40000`，rootfs start=`0x48000`。与原parameter吻合。未来写前再核设备身份、label、起始地址、大小及原件hash，不能只按历史mmc编号写。

## 2. 唯一 Host 候选部署对象

| 新对象 | 未来destination | 启动影响/rollback |
| --- | --- | --- |
| P026 paired Image | `/boot/Image-6.1.99-rk3576-m0echo-p026` | 新文件；原Image保留 |
| paired initrd | `/boot/initrd.img-6.1.99-rk3576-m0echo-p026` | 更新一个原initrd模块及metadata；原件保留 |
| AMP DTB | `/boot/dtb/rk3576-lubancat-3-v2-m0echo-p026.dtb` | 完整no-map/memory-region/link04/MBOX0/4；保留其它board节点/已有uEnv overlay |
| AMP FIT | `/boot/amp/amp.itb`，Ub文件系统路径`/amp/amp.itb` | passive file；上电不会自动读取/启动 |
| 255 paired modules | `/lib/modules/6.1.99-rk3576-m0echo-p026` | 只解包这个新release子树；不替换原modules；不复制Host build/source链接 |
| paired echo KO | `/opt/rk3576-amp-test/rk3576_amp_echo_test.ko` | 手工HELLO/PING；没有autoload/service |
| 三个boot软链接 | `Image`、`initrd`、`rk-kernel.dtb` | exact target见JSON；未来切换须原件先验hash |
| P026 Ub双slot4MiB | 现有by-partlabel/uboot前4MiB | 仅替换proper Ub；原DDR/SPL位置、ATF1/2/3、OP-TEE/controlDT不改变；raw8MiB可回滚 |

新package SHA=`4b6e615beba307d95beff1cbbff76b06bfe794f9a2f269599d2ccd5026ca4f1f`。预计分区最终8MiB=新4MiB包+原零尾4MiB，不把4MiB文件hash冒充整分区hash。**不新建amp，不改GPT，不移动/重建rootfs，不改uEnv/boot.cmd/boot.scr。** 原Ub没有项目文件命令，单独把FIT复制进boot不能使它生效。

## 3. 将来的首次测试顺序

先完成现存runtime门及逐项部署审批，之后才执行：

1. 放置passive新文件和孤立modules，核hash；先仅更换获批派生Ub，**保持原Image/initrd/DT三个链接不动、不执行amp_m0load**，验证Linux-only启动。后续切换paired links/AMP DT另按已批清单，不将此阶段合并为首次M0测试结果。
2. 准备两路独立观察：既有Debug UART1500000负责DDR/Ub/Linux；3.3V UART5 RX=40Pin16、TX=18、GND=硬件已标GND负责M0 115200。PC RX接M0 TX18、PC TX接M0 RX16，共地，不接TTL 5V/VCC供电。用户已有Debug接线；没有假定第二个适配器已接好，物理接线由用户完成。
3. cold boot prompt先加载正确AMP Linux FDT到已审查`0x48300000`，只执行一次项目入口 `amp_m0load /amp/amp.itb 0x48300000`。它拒绝stock DT、错误metadata/hash/policy或任何reserve冲突。不要先release M0再加载旧DT。code/shared即刻完整保护，一次cold boot不支持retry。
4. 正常boot脚本使用已批准的paired Image/initrd/AMP DT链接和原uEnv。最后DT prep再次校验no-map/memory。Linux transport/NS注册后手工运行匹配的echo driver。
5. 只验Linux正常启动、M0 banner/heartbeat、HELLO_ACK、PONG。无MPU/I2C扫描/Camera/Audio压力组合。失败即停止，保留串口/有限日志，按下节rollback。

这个顺序涉及重启和M0 release，**本轮没有执行、没有授予APPROVAL_GATE_BOARD_TEST**。

## 4. Linux仍可启动：恢复原Linux-only

下一次经明确恢复授权，在M0已由用户cold power-cycle停止的前提下，不调用amp命令。先对原Image/initrd/DTB hash检查，再恢复软链接；候选Ub默认disarmed，不执行入口即不会启动M0。

```sh
# 仅未来获批rollback；本轮不要执行。
cd /boot
printf '%s\n' \
 'e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e  Image-6.1.99-rk3576' \
 '425d2a68a4a67e807518c067cfefc1111d0a33af02236761ddaa3794c3205397  initrd.img-6.1.99-rk3576' \
 '76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90  dtb/rk3576-lubancat-3-v2.dtb' | sha256sum -c - && \
 sudo ln -sfn Image-6.1.99-rk3576 Image && \
 sudo ln -sfn initrd.img-6.1.99-rk3576 initrd && \
 sudo ln -sfn dtb/rk3576-lubancat-3-v2.dtb rk-kernel.dtb && sync
```

原uEnv/boot.scr、旧modules均没有替换，无需重建。先恢复links/观察原Linux成功，再删除**仅本任务创建**的passive文件/新release，禁止递归删除原模块或整个boot。恢复原Ub属于另外一项L3写入，按原raw8MiB处理。

## 5. Linux不能启动，但Ub prompt可用

先由用户**完全断电再上电**，在这次cold boot用Debug CTRL+C停止autoboot，且该次启动不调用amp_m0load。不能在已release M0的同一次启动直接换stock DT：M0可能仍活动，最终guard也会拒绝旧DT。不能用未验证的soft reset替代cold cycle。原文件都保留，可在RAM中手动启动旧Linux（不saveenv）：

```text
# 仅未来获批恢复；地址来自本板现有boot.cmd/日志。
load mmc 0:2 0x40400000 /Image-6.1.99-rk3576
load mmc 0:2 0x4a200000 /initrd.img-6.1.99-rk3576
setenv original_initrd_size ${filesize}
load mmc 0:2 0x48300000 /dtb/rk3576-lubancat-3-v2.dtb
setenv bootargs storagemedia=emmc androidboot.storagemedia=emmc androidboot.mode=normal root=/dev/mmcblk0p3 boot_part=2 earlyprintk console=ttyFIQ0 console=tty1 consoleblank=0 loglevel=7 rootwait rw rootfstype=ext4
booti 0x40400000 0x4a200000:${original_initrd_size} 0x48300000
```

使用前核本次实际boot MMC/partition，不能套到别的板。回滚选择旧baseDT暂不加CAM0 overlay，目的是恢复Linux，再按第4节恢复原正常脚本/链接；不宣称这条手动恢复已实测。本流程不要求删除GPT或先擦除eMMC。

## 6. 连Ub prompt都没有

现有Windows RKDevTool3.32 + Rockusb5.14，用户已确认整机恢复实测PASS。官方进入流程：[LubanCat刷机文档](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/flash_img/flash_img.html)，使用v2的MR键和指定Type-C OTG；本轮不进入MR。

厂商工具随包《开发工具用户手册v1.0》，SHA=`be6d064c5f70a7edc28a9f47a05db97aa7c3e3d9cd99cd46426b364bad7e919a`：PDF p3 §1.2 明确 **LoaderToDDR只下载到RAM，不写设备存储**，与同页§1.3的Loader烧写不同；p4 §1.4要求依据parameter配置分区；p7 §1.7支持按地址写。该手册最初对应工具2.88，随3.32发布，功能是来源证据，不是RK3576单分区恢复的实测证明。

保留用户数据的恢复优先方案：只选已核factory MiniLoader的LoaderToDDR；依据原parameter/当前分区只选择uboot项，source为Desktop **uboot-original-8MiB.raw**，目标仅既有8MiB uboot区，绝不勾parameter/rootfs/擦除/Loader持久烧写。地址单位及目标在实际3.32界面按原parameter再次核对；若工具不能显示/验证目标则停止，不能猜单位按执行。恢复后验证readback8MiB SHA，再用第5节原Image/DT进入Linux。**此单分区路径SOURCE_VERIFIED/未实测，不冒称用户已测过。** 官方CLI `rkdeveloptool wl` 的sector参数路径有公开源码，但本Windows没有把未安装的CLI当既有恢复工具。

最后兜底：用户已实测的3.32“升级固件”，选择桌面 `lubancat-rk3576-debian12-gnome-20260424_update.img`，size5793882755/SHA=`18247661a898821a751262d57f18f6edf6aab0f62b4b706cfc1b74a4ea277a66`。它是RKFW/RKAF update容器，不能作为raw全盘dd。这个方法会重建/覆盖rootfs、丢失未备份用户数据；boot+modules tar不是用户数据全备份。**需要整机回刷时先取得数据损失授权或完成所需用户数据备份，不能静默把两者等价。** 本轮文件方案没有GPT/rootfs搬迁，不能反过来声称所有事故都无数据风险。

## 当前裁决

文件/分区身份和恢复介质具体，原件已另存Windows；single-partition恢复属于有来源、未演练的计划，整机恢复有用户实测。Runtime映射/cache/新Ub能力仍缺，完整echo准入关闭。恢复方案清晰不等于本轮已批准刷写。
