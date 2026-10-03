# P028 新 U-Boot 的 Linux-only 测试审批清单

2026-10-03。**APPROVED_BY_USER；AMP C，非 D；仅按本清单执行Linux-only测试。**

用户在本聊天回复“批准”，明确批准上轮请求：只更换P028 U-Boot、验证原Linux启动、保持M0关闭。候选hash/原件/MiniLoader在Host再次核对一致。此前完整读回已PASS，本次串口确认新proper U-Boot、实际policy0、原boot.scr及Debian12登录：**Linux-only测试完成PASS**。此批准不扩大为AMP/M0测试。精确范围和执行状态见 [P028_LINUX_ONLY_EXECUTION.json](P028_LINUX_ONLY_EXECUTION.json)。

## 审批范围

AGENTS.md L3要求每次U-Boot写入/重启单独明确批准。此前中午12点前权限只覆盖诊断且已结束。本清单记录已批准并完成的用户手工测试对象，Agent本轮未访问或操作实板。

| 对象 | 确切内容 |
| --- | --- |
| 板卡 | LubanCat-3 v2 / RK3576，当前恢复的原Linux |
| 新文件 | `C:\Users\27432\Desktop\RK3576-AMP-P028-LinuxOnly-PendingApproval\uboot-P028-linux-only-8MiB-PENDING-APPROVAL.img` |
| 大小 | 8388608 bytes |
| 新SHA256 | `9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e` |
| 原完整分区SHA256 | `ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a` |
| 目标 | EMMC现有GPT `uboot`，起始LBA0x4000、长度0x4000个512B扇区，即8MiB起始/8MiB大小 |
| 可改变内容 | proper U-Boot代码；原三BL31/OP-TEE/control DT payload与原尾4MiB保持逐字节相同 |
| 启动文件与分区 | `/boot`、uEnv、Image、DTB、initrd、GPT、boot/rootfs/IDBlock均不属于变更 |
| 正常上电 | 用户手动，Debug串口全程记录 |
| AMP | 不部署RTOS、不启用RPMsg、不调用 `amp_m0load`、不读/写SYS_SGRF |

## 批准后才实施的流程

1. 记录Debug串口；电脑先核新文件SHA。用户按此前已成功的方法进入MASKROM，使用RKDevTool3.32。
2. 仅LoaderToDDR行选择原factory-MiniLoaderAll.bin（SHA4c0d04c6234b6e783aeedf317f4bb661700d9ef2ef97f876e67597807aa86206），下载到RAM。不要选择普通Loader/Parameter，不写IDBlock。
3. 读取设备GPT和当前存储，确认EMMC及uboot起始/长度均0x4000。若不同，停止，不按历史值强制写入。
4. 仅勾Uboot/EMMC，选择上表8MiB新文件，使用读取GPT自动填的地址，强制地址写关闭；只执行这行。保留下载日志。
5. 按本次已成功的高级功能只读导出：EMMC，起始扇区0x4000、扇区数0x4000。Host比较导出8MiB的新SHA。只有完整一致才进入正常冷启动；不一致立即停止并恢复原件。
6. 用户断开所有供电，释放MASKROM键，正常上电；只观察新proper U-Boot到原Debian登录。不要输入AMP命令。

## 预期观测和失败标准

- SPL检查新uboot SHA前缀 `2e1b965bfb`；BL31/OP-TEE原hash/版本保持。
- proper U-Boot提交 `f8b4554`，`Cmd interface: pending runtime board policy`。
- 正常策略打印 `Verified-boot: 0` / `PROJECT: factory boot/CLI allowed by runtime policy=0`。
- 3秒autoboot，原 `/boot.scr` → Image/DT/uEnv → 原 `6.1.99-rk3576 #8` → Debian12登录。
- 新镜像 `Can't read verified-boot flag`、policy非零/错误、`factory boot/CLI closed`、找不到FIT、无法进入原Linux或异常均判FAIL；不得setenv绕过、关闭验签或继续启动M0。
- Host包装/CRC/默认env/策略fault回归已完成；本次实板日志补齐原factory HUSH脚本与Linux启动路径，补充已测试Ctrl+C/help/boot交互，AMP仍未测试。仍不自动进入D。

## 精确回滚

原件：`C:\Users\27432\Desktop\RK3576-Recovery-Tools\P026-Originals-20261003\uboot-original-8MiB.raw`，SHA见上表。

若新Linux启动失败，用同一已成功的MASKROM → 原factory LoaderToDDR到RAM → 读取现有GPT/确认EMMC → 仅Uboot行写原8MiB → 起始/数量0x4000只读导出核原SHA → 用户正常冷启动原Linux。这个路径本次已经实测通过，证据 [恢复记录](P028_RECOVERY_READBACK.json)。不要刷完整update.img、不要动boot/rootfs/GPT/用户数据，不自动进行整机重刷。

若MASKROM/RAM Loader/GPT不能按本次方式访问，停止并保留日志；整机镜像虽可恢复但会覆盖用户数据，不在本次部分回滚审批范围。

新proper U-Boot策略失败仍可能关闭CLI并需要MASKROM，这是已知风险，不保证此次启动必成功。
