# 当前步骤：已读回新镜像，冷恢复默认 Debian

2026-10-04：用户已在 P030 上直接执行一次 B；主控随后只读完整8MiB，确认新镜像 SHA f9beef07…72c0b3 匹配，最终DT及配套Linux启动通过。COM6 缺预期等待/15s超时，完整B未通过，C/KO继续关闭。**当前不需要再次刷U-Boot或重跑B。**

请保存两路日志，在当前 Linux 正常关机（`sudo poweroff`），关机完成后完整断开板端供电及反供电连接，再按默认方式上电；不执行任何 stage 脚本。保存 COM5 从 DDR/SPL 到默认 Debian 登录的日志，回报当前 IP，并发送 `uname -a`、`cat /proc/cmdline`。期望原 `6.1.99-rk3576`、root p3、uboot149b1c5、无 amp_test_stage。本次B证据已记录在项目审核目录的 P030_B_EXECUTION.json。以下写入步骤保留为历史和必要回滚参考。

# 历史交付流程：P030 最终设备树检查修复（非当前操作）

2026-10-04。沿用本任务已有“批准所有操作”。本包只更新现有 eMMC `uboot` 分区；物理操作由用户完成。新镜像须在机器记录中标记 `HOST_REVIEW_PASS_PARENT_ACCEPTED` 后使用。交付时新镜像的板端读回、默认 Linux 和下一次 B 均尚未验证；最新结果和当前步骤以本文顶部为准，整体 C，D 关闭。

## 文件与修复

桌面目录：`C:\Users\27432\Desktop\RK3576-AMP-P030-FinalFdt-v1`。

| 用途 | 文件 | SHA256 |
| --- | --- | --- |
| 本次写入，8388608 字节 | `uboot-P030-final-fdt-8MiB.img` | f9beef07f807123a48f5115458dedb7bdf47337306df1a981190abf65372c0b3 |
| 回滚到已验证默认启动的 signed-v8，8388608 字节 | `rollback-P029-signed-v8-8MiB.img` | ef857b106476cb6fc775cd3837db3b5d254d6ff10103a9281669dffd7e67f1e4 |

U-Boot 的最终检查误拒绝了厂商导出的 9 个 `(0,0)` 空内存槽。修复仅跳过这类占位，仍要求真实 RAM 覆盖和三个精确 `no-map` 预留区；同时修复属性读取覆盖循环长度、漏查后续 reserved tuple 的问题。公钥控制 DT、M0 签名 FIT、BL31、OP-TEE、内存地址和尾部 4MiB 均保持字节一致。

此前实板已通过 M0 签名和 payload 校验，UART5 观察到 M0 运行及 `bypass=1`；随后最终 DT 检查失败，没有启动配套 Linux，尚不能判定完整 B、有效 mapping 或 RPMsg 成功。已补真实 vendor 最终 DT 流程 Host 回归，仍须以下实板门。

## 1. MASKROM 和 RAM Loader

当前保持默认 Debian。正常关机结束后，按此前成功的方法完整断电并进入 MASKROM。使用已实测 v3.32 恢复工具：`RK3576-Recovery-Tools\RKDevTool_P028_Uboot_RestoreOnly`，保持下载后自动 reset 关闭。

需要下载 RAM Loader 时，下载镜像页仅勾 **LoaderToDDR**，选择原 `factory-MiniLoaderAll.bin`；地址 0，取消“强制按地址写”，执行一次。文件为 770553 字节，SHA256 `4c0d04c6234b6e783aeedf317f4bb661700d9ef2ef97f876e67597807aa86206`。失败则保留日志，停止此次操作。

读取“设备分区表”和“当前存储”，确认 **EMMC**、`uboot` 起始 LBA **0x4000**、扇区数 **0x4000**、每扇区 512 字节；不一致则停止。

## 2. 只写 Uboot

下载镜像页取消其他行，仅勾 **Uboot**：

- 存储：**EMMC**。
- 地址：设备分区表对应的 **0x00004000**。
- 路径：本目录 **uboot-P030-final-fdt-8MiB.img**。
- “强制按地址写”：取消勾选。

确认文件全名后点一次“执行”，保存“下载完成”日志。不要选 Parameter、trust、boot、rootfs 或普通 Loader。

## 3. 完整读回

高级功能页确认当前存储 EMMC，填：

```text
起始扇区：0x00004000
扇区数：  0x00004000
```

点击“导出镜像”，成功后告诉主控“导出完成”。文件通常在：

`C:\Users\27432\Desktop\RK3576-Recovery-Tools\RKDevTool_P028_Uboot_RestoreOnly\Output\ExportImage.img`

主控先核整个 8MiB 的逐字节一致性和完整 SHA256，期望 **f9beef07…72c0b3**。下载成功或版本 banner 不能代替读回。

## 4. 读回通过后，默认冷启动

完整断电再正常上电，COM5（1500000、8N1、无流控）保存从 DDR/SPL 到登录的日志。此次不执行 stage 脚本，让默认 Debian 启动。

预期 proper U-Boot **149b1c5**，SPL `Checking uboot` hash 前缀 **7d8fe670d9**，`Checking fdt` 前缀仍为 **43164981ef**，`PROJECT` policy=0，原 `/boot.scr` 启动原 Debian。登录后发送日志和：

```sh
uname -a
cat /proc/cmdline
```

预期原 `6.1.99-rk3576`、`root=/dev/mmcblk0p3`、无 `amp_test_stage=`，并回报当前 IP。主控确认新镜像读回、实际控制 DT 与默认 Linux 后，再提供下一次 B 指令。原 v8 B/C 指南当前仍撤回。

## 回滚

若读回不一致或默认启动失败，保留日志并停止本次回归。按相同 MASKROM/RAM Loader 流程，仅把 Uboot 路径改为本目录 **rollback-P029-signed-v8-8MiB.img**；完整读回应为 `ef857b…67f1e4`，再完整断电默认启动。该镜像已验证默认 Debian，但阶段 B 有已知最终 DT 检查缺陷，回滚后不执行 B/C。

原 P028 和原厂 8MiB 备份继续保留在此前目录；不要混淆回滚目标。
