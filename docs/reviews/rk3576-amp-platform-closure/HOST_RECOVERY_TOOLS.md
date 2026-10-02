# P024：Windows 恢复工具准备

2026-10-03。用户明确要求“帮我安装合适版本到桌面”。本轮仅Host下载/解压及预装签名Rockusb驱动，未连接/控制开发板、未打开刷写工具。

## 来源与版本

野火[RK3576官方说明](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/flash_img/flash_img.html)要求RKDevTool >=3.30。LubanCat/tools固定`210be81d659a6bc4e7a648744ae77837f394be0f`的现有RKDevTool为3.19，低于该要求，未采用。

改用板厂Radxa[官方工具下载目录](https://dl.radxa.com/tools/windows/)中的Rockchip包：

| 文件 | 来源 | SHA256 |
| --- | --- | --- |
| RKDevTool_v3.32.zip | `windows_RKDevTool_Release_v3.32.zip` | `5016bbf8542892dd1eadc8b4473dd12be5749f7d84db872cb8532d06009066ab` |
| DriverAssitant_v5.14.zip | `DriverAssitant_v5.14.zip` | `ffc71ccf8aa40afe74b485393dc5572e431b6ecc84b13eee5360907c7ec84276` |
| RKDevTool.exe | PE FileVersion 3.3.2.0，revision.txt 3.32 | `0d66e8e8fbf324885309ad1c82aaeb73ad6f6bf0cb7e1f3781bfade912500081` |

目录：`C:\Users\27432\Desktop\RK3576-Recovery-Tools`。桌面快捷方式：`RKDevTool 3.32.lnk`；目标为目录下 `RKDevTool_v3.32_for_window\RKDevTool.exe`，工作目录也设置为该目录。免安装工具已准备，未启动。

RKDevTool.exe自身Authenticode为NotSigned；不将下载SHA冒充厂商签名。来源为官方HTTPS下载，记录hash；没有解除执行策略或SmartScreen。zip成员先检查目录边界/大小/链接后解压。厂商其他打包程序未执行。

## 驱动安装：HOST_TESTED

- 64-bit Windows，原进程非管理员；用户UAC确认后仅调用Windows自带PnPUtil。
- 选`DriverAssitant_v5.14\Driver\x64\win10\rockusb.inf`：`DriverVer=05/20/2024,5.14.0000.0`，NTamd64；包含VID2207/PID350E、350F等。
- CAT `Get-AuthenticodeSignature`：**Valid**，Signer=`Microsoft Windows Hardware Compatibility Publisher`。Windows目录签名验证与PnP安装负责包成员完整性，不仅检查裸SYS签名。
- INF SHA=`fd9ab315f9fde7b6f036761844bc3a0807fc72b74171be2d6c8dad74da93bcc7`；CAT SHA=`bbcce164e88fefa81dc15ff238a5d5343a99d08007dd23103625bbbd73d7a6fd`；SYS SHA=`ac095074b9aaa649f795d1075010c073582421535021af069743c5df9d93fda8`。

实际命令：

```powershell
pnputil.exe /add-driver "C:\Users\27432\Desktop\RK3576-Recovery-Tools\DriverAssitant_v5.14\Driver\x64\win10\rockusb.inf" /install
pnputil.exe /enum-drivers
```

首条exit=0：1个driver package成功添加，发布名`oem71.inf`。第二条确认Fuzhou Rockchip / Rockusb Device / 5.14.0.0 / Microsoft signer。没有卸载已有驱动、安装ADB、关闭签名、使用force、请求或执行重启。日志/结果文件在桌面工具目录；metadata加入 [RECOVERY_IMAGE_IDENTITY.json](RECOVERY_IMAGE_IDENTITY.json)。

**已安装驱动 ≠ 该板MASKROM/LOADER识别通过。** 此次没有进入这些模式或加载恢复loader；USB线/OTG口、实际设备枚举、串口和恢复动作仍属于后续受控验证。

## 桌面原件副本

同目录的 `Original-Board-Boot-20261002` 保存已有Host原件的第二份副本，复制后SHA核对：

| 文件 | SHA256 |
| --- | --- |
| boot-originals-complete.tar | `d893cc9fc7fa379cfc127565142ef3c48be3d67ac8df3d483f9a8743ca37130d` |
| running-device-tree.tar | `61fae8ebac570606f536a8f4817425d04bf0226701bf97b3fd87cc59f7b01dde` |
| boot-members-manifest.json | `daf0618a7477e660df2f3b4529f8c0fc0f44a166a5c2952921e10430ef08200a` |
| uboot-partition.img | `ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a` |

这不是整盘/GPT/rootfs用户数据备份。原始镜像、原件tar及厂商工具未提交到Git。工具许可/再分发范围未作法律保证。
