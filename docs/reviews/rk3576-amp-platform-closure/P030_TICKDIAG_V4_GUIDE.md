> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

> 2026-10-04：此入口暂停；改用 [v5当前指南](P030_TICKDIAG_V5_GUIDE.md)。

# P030 tickdiag-v4：一次冷启动诊断指南

2026-10-04。**v4 已被动暂存并逐项读回；默认 Debian 身份核对通过。硬件 tick 根因尚未确定，需要用户一次双串口 B。旧 v1/v2/v3 入口暂停。**

## 1. 新包解决什么

当前已确认 remote_init 返回、首次 link probe 完成，但首次 mdelay 未观察到返回。新包在进入 RPMsg 初始化/延时前做一次有读次数上限的 SysTick 检查，记录配置返回值、局部寄存器、向量、ISR 与 RT tick 增量；失败时打印 STOP 并返回，不进入首次延时。它提供故障区分和失败停止，不提前声称时钟/IRQ 已修好，也不能检测此后才发生的 tick 停止。

没有盲换时钟或修改共享映射、Linux transport。CTRL 读取会清 COUNTFLAG，但既有 tick ISR 不使用该位；没有外部时钟/安全寄存器读写。固件读取的是 MCU 本地 SysTick/SCB，Agent 没有通过 SSH 访问 MMIO。

v3 曾被动暂存；交付前发现其一条打印超过 128 字节，已暂停。v4 分行输出，最大格式长度按最长 phase/32位数字核为 **99 字节**。旧目录保留，不执行 v3。

## 2. 物料身份

| 文件 | 长度 / SHA256 |
| --- | --- |
| `/boot/amp-p029/tickdiag-v4/stage-B.scr` | **3055 字节**；`3430e1760b8aaffbee6122983fcc61438d3cd66175c8e9eb29e9a134ddac28d2` |
| `/boot/amp-p029/tickdiag-v4/amp-signed.itb` | 130048 字节；`354e820a6d324c49187a63e492671eb09f4c069dea85a8bc7dc5690968e5d9a4` |
| 新 BIN | 124840 字节；`4fafc33bdf6fb27802117415a87c22dee9afe3406ff57248daa49754171cbc54` |
| 安装回执 SHA256 | `198b4eea46b55cc9b4b47599996da57307a3bc3b015c9d2ec0a39600ff17e8da4` |

干净编译、当前 control-key FIT 验签、payload/entry/签名覆盖/容器长度和单组件 script CRC/结束项校验通过；没有新增或运行测试套件。板上 v4 验签与 tick/延时仍待本次实测。新目录已装好，无需 Windows 上传或重刷 U-Boot。

## 3. 开始保存双串口，再完整冷断电

COM5：1500000、8N1、无流控。COM6：115200、8N1、无流控。都开始保存全文后，正常关机、完整冷断电再上电，在 COM5 中断自动启动。

核对 `soc cold boot`、SPL U-Boot `7d8fe670d9` OK、control DT `43164981ef` OK、proper `g149b1c5`、policy=0。若不匹配或有 abort/panic，停止。

## 4. 一次 load → 核值 → source

```text
load mmc 0:2 0x4c000000 /amp-p029/tickdiag-v4/stage-B.scr
printenv fileaddr filesize
```

必须读入 **3055 bytes**，数值为 `fileaddr=0x4c000000`、`filesize=0xbef`。加载失败或值不符就停止；不要手工设置变量绕过门。load 与 source 之间不要插入其他 load/size。

匹配后只执行一次：

```text
source 0x4c000000
```

首行应是 `P030 M0 tick diagnostic v4 B - factory default unchanged`。脚本仍只调用一次 loader，仅 loader 成功后 boot 配对 Linux。M0 预检 STOP 不等于 loader 失败：loader 负责装载/释放 MCU，不验证 RT tick；Linux 可能仍正常登录，这种情况仍算诊断失败，不能进入 C。

## 5. 需要回传的完整输出

COM6 应包含 `probe-begin`/`probe-end` 的四类短行：`cfg`、`regs`、`irq`、`vector`，随后两条 `P030 tick probe` 的 changes/wraps/isr_delta/tick_delta。请回传全部，连同 STOP 或后续 init/link/first-delay/heartbeat/timeout。

| 观察 | 可区分的方向；仍需结合其余字段 |
| --- | --- |
| src_rc 非 0 | 外部时钟选择请求被 HAL 拒绝；检查 CALIB.NOREF 与当前 CLKSOURCE |
| VAL 无变化 | 计数器关闭或没有时钟等候选，结合 CTRL/配置结果 |
| VAL 变化、wraps 非 0，但 ISR/tick 无增量 | 中断使能/屏蔽/向量/交付路径候选 |
| ISR 增加而 RT tick 无变化 | ISR 到 RT tick 路径候选 |
| 预检通过，首次 delay 仍不返回 | 进一步核定时器唤醒、调度及进入 idle 后的 tick 行为 |

探针执行固定 **1048576 次**寄存器读取；这不是精确毫秒等待，采样的 wraps 也不代表无遗漏的全部硬件回卷。预检失败只是停止 echo 线程，不会关闭或复位 MCU。保留双路日志，完整冷恢复。

COM5 保留全部验签、loader、Linux 启动；如已登录，保存 `uname -a` 与 `cat /proc/cmdline`。B 仍关闭 Linux RPMsg transport，本轮不期待 HELLO_ACK/PONG。

有 STOP/abort、卡住或日志截断时，不重跑 source/amp_m0load，不另行 booti，不加载 KO 或进入 C。测试结束后正常关机（能进 Linux 时）、完整冷断电回默认 Debian，保留恢复 COM5、当前 IP、uname/cmdline。

[执行与准备记录](P030_TICKDIAG_V4_PREPARATION.md) · [机器记录](P030_TICKDIAG_V4_PREPARATION.json)。
