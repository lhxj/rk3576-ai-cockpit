# P030 初始化诊断：source-fix-v2 操作指南

当前人工入口已更新为 [tickdiag-v4 指南](P030_TICKDIAG_V4_GUIDE.md)。本页旧步骤保持暂停，不重跑旧脚本。

**已执行一次，当前暂停重跑（2026-10-04）：** 用户 v2 B 已启动 M0 和配套 Linux；remote_init 返回、link probe 完成，但首个 mdelay 返回未观察到。用户已完整冷断电恢复，串口和实时 SSH 确认默认内核/cmdline；等待 tick/IRQ/定时器新诊断方案。本指南后文只保留本次操作流程，不是再次执行指令。见 [本次结果](P030_SOURCE_FIX_V2_B_EXECUTION.md)。

## 已执行流程（历史）

日期：2026-10-04。**修正版已被动暂存并读回；默认 Debian 冷恢复通过。下一步由用户在新的冷启动会话执行一次 B。**

## 1. 本次修复及身份

v1 脚本长度表的结束项为 `0xffffffff`，当前 U-Boot 要求 `0`，会在正文执行前静默拒绝。v2 只改结束项及两处 CRC。真实 source Host 回归 190 项通过；安装器快照 7 项通过。U-Boot 和 M0/FIT 无需重刷或重建。板上完整 B、初始化 checkpoint、有效 mapping/cache 和 RPMsg 仍未验证。

| 项目 | 已核对身份 |
| --- | --- |
| 新目录 | `/boot/amp-p029/initdiag-source-fix-v2` |
| stage-B.scr | 3062 字节，SHA256 `2f9bcdf69de2e8b72cdbcd8453b72122720a207529bdff3f2db1d97d36aff0d8` |
| stage-B.cmd | 2990 字节，SHA256 `593bf63f7e7ce785de22e9068a37ee75c7f75b3c614b9005ffdf6fd16d24e6f8`，原正文保持 |
| 仍引用的 FIT | `/amp-p029/initdiag-v1/amp-signed.itb`，129024 字节，SHA256 `cc1c99d5532e5cc1cd63e690b5d205964ba75a7f68692eff3282f02ec308df30` |
| 板端安装回执 SHA256 | `ec5a8a958e888efcd17e2fec38c8a1d15941e52fcdd89e77631a31330c09d745` |

## 2. 打开双路日志，再完整冷断电

COM5：1500000、8N1、无流控。COM6：115200、8N1、无流控。两路均开始保存全文后，正常关机并完整冷断电再上电，在 COM5 中断自动启动到 U-Boot 提示符。

冷启动必须匹配：`soc cold boot`；SPL U-Boot hash `7d8fe670d9` OK；control DT hash `43164981ef` OK；proper `2017.09-g149b1c5`；factory/default policy `0`。若身份不匹配或有 abort/panic，停止并保存日志。

## 3. 先 load 新路径，核值，再 source 一次

在 COM5 输入：

```text
load mmc 0:2 0x4c000000 /amp-p029/initdiag-source-fix-v2/stage-B.scr
```

必须成功读入 **3062 bytes**。随后输入：

```text
printenv fileaddr filesize
```

数值必须为 `fileaddr=0x4c000000`、`filesize=0xbf6`（3062）；十六进制显示大小写可不同。加载失败、长度或地址不符时停止，不手工设置变量绕过门。

确认后输入一次：

```text
source 0x4c000000
```

load 与 source 之间只做上述 printenv；不要插入其他 load/size。不要先 source 再 load。进入脚本正文的第一个输出应为：

```text
P030 M0 initialization diagnostic B - factory default unchanged
```

脚本按已有门检查/加载配对 Linux 与 B DT，调用一次签名 FIT loader，仅 loader 成功后立即 boot 配对 Linux。脚本仍引用 initdiag-v1 中的 FIT，这是预期路径。

## 4. 保存实际结果

COM5 保存 banner、每个 STOP/验签/loader 结果及 Linux 启动全过程。若 Linux 登录，执行并保存：

```sh
uname -a
cat /proc/cmdline
```

预期配对内核 `6.1.99-rk3576-m0echo-p026`、root p3 和 `amp_test_stage=B`，需以实测核对。

COM6 保存完整 RT-Thread 启动及以下 checkpoint；缺失哪条就是诊断信息：

- `P030 diag remote_init begin/end`、tick 与 delta、returned
- `P030 diag wait begin`、limit_ticks
- 首次 `link_probe begin/end` 与 up
- 首次 `first_mdelay begin/end` 与 tick delta
- 至多三条 waiting heartbeat
- 15 秒 link timeout（仅 remote_init 返回且 RT tick 正常推进后才可到达）

B 阶段 Linux RPMsg transport 仍关闭，本轮不期待 HELLO_ACK/PONG。保留 COM5、COM6 两份完整日志回传。

若又静默、出现 STOP/abort、卡住或超时，只保存日志并结束本次。不要同会话重跑 source/amp_m0load、另行 booti、加载 KO 或进入 C。测试后正常关机（若已进入 Linux），完整冷断电回默认 Debian，保存恢复日志。Agent 本轮只暂存文件，未启动 M0 或重启。

## 5. 记录

[执行记录](P030_INITDIAG_B_EXECUTION.md) · [机器证据](P030_INITDIAG_SOURCE_FIX_V2.json) · [原 v1 主机准备记录](P030_M0_INITDIAG_HOST_PREPARATION.md)。桌面同目录提供修正版 cmd/scr 与 manifest，供查看；板上文件已暂存，无需从 Windows 再上传。
