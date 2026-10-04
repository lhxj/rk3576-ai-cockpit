> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 已由P030替代：本文件仅保留历史

2026-10-04 v8 B在M0启动后最终DT检查失败，B/C撤回。当前修复与新8MiB读回/默认冷启动使用桌面 `RK3576-AMP-P030-FinalFdt-v1\README.md`，不要依据以下历史步骤重试B。

# P029：写入带AMP公钥的U-Boot，再验证默认Linux

本次依据用户已有“批准所有操作”，只操作现有EMMC `uboot` 分区。Host包须已完成独立审核；机器记录未标REVIEWED时停在文件核对。**不执行旧stage-B或stage-C。** 新AMP签名在硬件上仍未验证，整体C，D关闭。

## 文件

桌面目录：`C:\Users\27432\Desktop\RK3576-AMP-P029-SignedFix-v8`。

| 用途 | 文件 | 完整SHA256 |
| --- | --- | --- |
| 本次写入 | `uboot-P029-required-conf-8MiB.img` | ef857b106476cb6fc775cd3837db3b5d254d6ff10103a9281669dffd7e67f1e4 |
| 回滚到已实测P028 | `rollback-P028-proven-8MiB.img` | 9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e |

两份文件都为8388608字节。包中的 `amp-signed.itb` 用于后续被动暂存；当前不要在串口加载它。私钥不在交付包。

## 1. 进入已实测的恢复工具流程

保留当前正常Linux；完成关机后，由你按此前成功的方法完整断电并进入MASKROM。使用桌面原 `RK3576-Recovery-Tools\RKDevTool_P028_Uboot_RestoreOnly` 中v3.32工具（下载后自动reset已关闭）。

若需要重新下载RAM Loader，下载镜像页只勾选 **LoaderToDDR**，选此前核验的 `factory-MiniLoaderAll.bin`；地址0正常，不勾“强制按地址写”，执行一次。该文件770553B/SHA `4c0d04c6234b6e783aeedf317f4bb661700d9ef2ef97f876e67597807aa86206`。LoaderToDDR与普通Loader的动作不同，本次使用已实测RAM下载项。

读取“设备分区表”和“当前存储”：EMMC，`uboot` 起始LBA **0x4000**、数量 **0x4000**，512字节/扇区。若不一致先停，不按历史地址强写。现有boot/rootfs/GPT不用修改。

## 2. 只写这一行

下载镜像页取消其它勾选，仅勾 **Uboot**：

- 存储：**EMMC**。
- 地址：读取设备分区表后对应uboot的 **0x00004000**。
- 路径：`C:\Users\27432\Desktop\RK3576-AMP-P029-SignedFix-v8\uboot-P029-required-conf-8MiB.img`。
- “强制按地址写”：不勾。

核完整文件名后点一次“执行”，保存“下载完成”日志。仍使用已实测的uboot分区下载流程，勿选Parameter/trust/boot/rootfs或普通Loader。

## 3. 完整读回，先交给主控核对

高级功能页确认当前存储EMMC，填：

```text
起始扇区：0x00004000
扇区数：  0x00004000
```

点“导出镜像”，保存成功日志。按已实测工具行为，文件在：

`C:\Users\27432\Desktop\RK3576-Recovery-Tools\RKDevTool_P028_Uboot_RestoreOnly\Output\ExportImage.img`

告诉主控“导出完成”。主控会读取整个8MiB，与新候选逐字节/完整SHA核对；本次新期望是 **ef857b…67f1e4**。不要仅凭下载成功或f8b4554 banner判断写入成功。

## 4. 核对通过后，默认冷启动

完整断电，重新正常上电，Debug COM5继续保存启动日志。本次**不执行任何stage脚本**，让默认Linux启动。应看到proper U-Boot仍为 `f8b4554`、fdt hash前缀 `43164981ef`、policy0和原boot.scr流程，最后原Debian桌面/登录。

登录后执行并发送结果：

```sh
uname -a
cat /proc/cmdline
```

预期原 `6.1.99-rk3576`、root p3、没有 `amp_test_stage=`。回复默认Linux状态/IP并提供Debug日志。主控完成只读确认、签名固件和新B脚本暂存后，才给下一次B指令。

## 回滚

若新8MiB完整读回不匹配或默认Linux无法启动，停止本次回归，保留日志。按同一MASKROM/RAM Loader流程，仅将Uboot路径改为本目录 **rollback-P028-proven-8MiB.img**；完整读回必须回到 `9bd8cc03…71c6d5e`，再完整断电默认启动。P028的默认Linux和CLI此前已实测。需要原厂8f53f800da时已有原8MiB备份仍保留于 `RK3576-Recovery-Tools\P026-Originals-20261003\uboot-original-8MiB.raw`，SHA `ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`；不要混淆两份回滚目标。
