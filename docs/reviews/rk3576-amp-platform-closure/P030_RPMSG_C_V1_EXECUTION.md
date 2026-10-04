# P030 RPMsg C v1 实板执行结果

2026-10-04，Asia/Shanghai。**本次 HELLO/HELLO_ACK、PING/PONG 双向通信通过，测试后完整冷断电恢复默认 Debian 通过。** 证据来自用户提交的 COM5、COM6 日志，以及本轮恢复身份和物理断电确认。Agent 仅离线审查和保存记录，本轮没有 SSH 或板端操作。

## 启动与身份

COM5 记录 `soc cold boot`、proper `2017.09-g149b1c5`、policy=0，SPL U-Boot/control DT hash 前缀 `7d8fe670d9` / `43164981ef` 均 OK。执行：

```text
load mmc 0:2 0x4c000000 /amp-p029/rpmsg-c-v1/stage-C.scr
printenv fileaddr filesize
source 0x4c000000
```

实测 3100 bytes、fileaddr=0x4c000000、filesize=0xc1c；仅观察到一次 source。新版 banner、RSA p029dev/hash 和 loader 均成功。MCU payload SHA 为 `28691e30790f1585782f3528dbd611c1e8005f8790947462574d73b8a75b6124`，load=0x47800000、entry=0x141。随后配套 Linux `6.1.99-rk3576-m0echo-p026` 到登录，root=/dev/mmcblk0p3，stage=C。

## 时基、共享路径与收发证据

| 项目 | 实际结果 |
| --- | --- |
| M0 程序 | RT-Thread build Oct 4 2026 16:54:02；local_fn=0x66e5，link=4 |
| 旧 reload | LOAD239998，UART参照 counts17449，wrap/ISR/tick 增量均0 |
| 条件修正 | LOAD326，counts17449，wrap/ISR/tick 增量均54，rate gate PASS |
| 首次延时 | begin tick62、end167、delta105，已返回；包含快照打印耗时 |
| link | `P029 link up` |
| Linux 实际 rings | 0x47d00000 / 0x47d08000 |
| Linux 实际 DMA base | 只读 preflight 输出 0x47d10000，源代码确定池长度0x10000 |
| 服务 | virtio0.rk3576-m0-echo.-1.12292（远端地址0x3004） |
| Linux 通道 | src=1024、dst=12292 |
| Linux 收到 | HELLO_ACK（165.030892）、PONG（165.034843） |
| M0 实际收到 | shared_va=0x27d18010 / len5；0x27d18210 / len4 |
| M0 回复 | `P029 PONG sent`；`echo task complete; no warm restart` |
| cache | entry、link、after-pong 均 ctrl=0x000006cc、bypass=1 |

Linux 日志显示 RPMsg host online、NS 创建服务。用户运行：

```sh
sudo -n timeout 10s python3 /boot/amp-p029/rpmsg-c-v1/preflight-c.py
sudo -n timeout 20s insmod /boot/amp-p029/rk3576_amp_echo_test.ko
```

观察到两次只读 preflight PASS；它在读取 dmesg 时匹配实际 DMA backing，在模块加载前核验 C 身份、DT/no-map/System RAM、不重复启动 M0、服务未绑定、配套 KO 字节/vermagic 等条件。原始 console 没有单独显示 `buffers: va ..., dma ...` 行，DMA base 证据来自已核验脚本的 PASS 和明确输出，不补写未提供的原始行。

首次 insmod 后 Linux 收到 HELLO_ACK 和 PONG；配套 KO 的实际源代码在 probe 发送5字节 HELLO，在匹配9字节 HELLO_ACK 后发送4字节 PING。与 M0 两次接收长度、PONG sent 及 Linux 回调对应，提供实际双向通信证据。日志时间戳包含模块/打印/调度，不据此声称精确往返时延。没有单独的 `echo $?`，不能伪造首次 insmod 数字退出码。

**偏离指南记录：** 日志还显示第二次 insmod，返回 `File exists`。这是模块已经加载，不能当作首次收发失败；也不证明另一次发送或 M0 重启。无需卸载或重复验证。内核提示 out-of-tree module taint 为此次配套测试模块加载的正常标记。本次相关记录未出现 STOP、link timeout、Oops 或 panic；不由短测推断长期无异常。

## 冷恢复

附件 COM5 结束于 C 会话；恢复证据来自用户随后粘贴的命令及确认：**“已完整断电再上电”**。

```text
Linux lubancat 6.1.99-rk3576 #8 SMP Fri Apr 24 16:46:57 CST 2026 aarch64 GNU/Linux
```

cmdline：root=/dev/mmcblk0p3、boot_part=2、无 amp_test_stage、fwver uboot-149b1c5。物理冷恢复为 USER_CONFIRMED，默认身份为用户串口记录；没有新 SSH 读回或恢复后 SPL 全文，不将 C 启动前的 cold marker 冒充恢复后的 marker。

## 裁决与范围

- C 启动、只读运行核验、HELLO_ACK/PONG 双向收发、实际访问子范围的共享映射功能、三处 cache bypass 快照：PASS / BOARD_OBSERVED_USER_LOG。
- C 后完整冷恢复默认系统：PASS / USER_PHYSICAL_CONFIRMATION_AND_SERIAL_IDENTITY。
- 当前 source 静默拒绝及首次 RT 延时停滞已在修正版实际启动路径解决；本次正常完成 echo。
- 这证明已访问代码/共享路径可用；pa_proposal 是合同换算，不能当 CON16/17 原始寄存器读回，也不证明整个512MiB窗口、缓存开启模式或长期稳定性。
- 本次关闭最小 RPMsg 通信验证里程碑；不自动修改 canonical final 合同、提升整体 D 或验收业务心跳/错误恢复。后续可独立推进业务协议、序号/超时/心跳及有界重复通信验证，保持当前恢复入口。

## 来源与交付

原始日志含登录/网络信息，仅保留在用户附件，不提交或复制原始 COM5 全文。提交本脱敏记录和 JSON；Windows 同步到 `C:\Users\27432\Desktop\RK3576-AMP-P030-RPMsg-C-v1`。

| 附件 | SHA256 |
| --- | --- |
| COM5：50914c1e-a014-4b04-8f31-920a96c26796 | 0076eb99c9a2d0403a3e2332173a201d2a1ff8b9b616137f0725957775fbc6d8 |
| COM6：29f81f97-4eed-4290-9300-446e8a9d96ee | cb2e2aa97e5dbce2a4faa289089fa2b771541e7dd8f4169b832c0341bae30491 |

本轮没有新增测试、重新构建、修改启动包或触碰实板。离线核对双日志、既有 preflight/KO 源代码与交付身份，文档 diff 和 Windows 字节同步检查。Git 结果以实际提交/推送记录为准，Draft PR 保持未合并。
