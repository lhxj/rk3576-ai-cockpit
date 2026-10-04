# P030 source-fix-v2 B：首次延时返回未观察到

2026-10-04。**修正版 source、签名 FIT/MCU 校验、M0 entry 与配套 Linux 启动通过；remote_init 已返回；首次 mdelay 返回未观察到。完整 B 尚未验收，C/D 关闭。**

## 实际操作与结果

COM5 全文 1280 行，附件 SHA256 `393cc880109871051f4f323b4e462d0bde2ff2a2d35125622632822fb7f0fb43`。原文含登录凭据，仅记录脱敏事实，未复制原文到项目或桌面：

| 证据 | 结果 |
| --- | --- |
| 冷启动身份 | soc cold boot；SPL U-Boot 7d8fe670d9/control DT 43164981ef OK；proper g149b1c5；policy 0 |
| 161–168 行 | load v2，3062 字节；fileaddr=0x4c000000、filesize=0xbf6；一次 source 后打印 P030 首行 banner |
| 173–187 行 | RSA p029dev 与 MCU payload SHA256 通过，payload e8b04bba…1579f；bus_mcu load 成功 |
| 188–210 行 | loader 返回成功，最终 DT/booti 通过，启动 6.1.99-rk3576-m0echo-p026 |
| 1251、1276–1279 行 | Debian 登录、uname 与 cmdline；root p3、amp_test_stage=B |

COM6 用户摘录及后续确认：

```text
P029 M0 entered local_fn=0x000066ad link=0x4
P029 cache entry ctrl=0x000006cc bypass=1
P030 diag remote_init begin tick=0
P030 diag remote_init end tick=0 delta=0 returned=1
P030 diag wait begin tick=0 limit_ticks=1500
P030 diag link_probe begin tick=0
P030 diag link_probe end tick=0 up=0
P030 diag first_mdelay begin tick=0 duration_ms=1000
msh >
```

用户确认“一直没输出，现在已经回到桌面”；精确观察秒数和完整 COM6 capture 未提供。

## 已缩小的范围

本次 `remote_init` 返回非空，首次 `link_probe` 完成且 link 尚未建立。因此本次缺输出已缩小到第一个 `rt_thread_mdelay(1000)` 未观察到恢复后的日志。不能再把本次描述为 remote_init 未返回。

实际源码链为：`rt_thread_mdelay` → `rt_thread_sleep` → 挂起当前线程、启动其 RT 定时器 → 调度；定时器到期需要 RT tick 及唤醒路径推进。15 秒检查在等待循环里；线程尚未从 mdelay 恢复时，这个检查无法运行，它不是独立 watchdog。

当前所有 checkpoint tick=0 可能发生在首个 tick 之前，单凭这些值不能证明 tick 永久停止；结合持续没有 delay-end，tick/中断/定时器唤醒是重点候选。msh 提示符说明能运行到另一线程，也不能证明 tick 持续推进。不能据此判定特定时钟、Linux unused-clock、共享映射或 mailbox 已是根因。

## Host 源码审查

实际 evb 配置使用 100Hz 私有 SysTick；tick ISR 调用 HAL_IncTick/rt_tick_increase。board 请求外部 24MHz 源后配置 reload，并未检查 HAL_SYSTICK_CLKSourceConfig 的返回值；HAL 在 CALIB.NOREF 时会拒绝外部源。当前板端寄存器状态未知，因此这里只记录待观察的分支，尚未换源、改时钟或生成新固件。

接下来最小观测应覆盖：SysTick 配置/返回值、handler 安装值、PRIMASK、tick ISR 是否进入、echo 线程定时器是否启动及是否被唤醒。输出需要有数量上限，不能依赖一个可能停止的 tick 来给自身计时。UART5 现有 RX+GND 不能直接输入 MSH 命令。

## 当前系统与操作边界

用户随后明确确认已完整冷断电再上电，并提供默认 uname/cmdline。按用户要求重试一次持锁、有界只读 SSH：exit0，确认 `6.1.99-rk3576 #8`、root p3、无 amp_test_stage；v2 脚本 SHA256 保持，未加载 echo KO、RPMsg devices 为空。此前一次 SSH exit124/无身份输出保留为失败历史，不用当前成功倒改。

默认恢复已由用户冷断电确认及串口/实时 SSH 身份共同支持；本次完整冷恢复 COM5 capture 尚未提供，不把 SSH 读回当作 SPL cold transcript。本轮无板端持久写、source、M0/KO/MMIO 或 Agent 重启，也未运行新测试。

不要再执行 v2 source、手动 amp_m0load/booti 或进入 C。下一步准备 tick/IRQ/定时器诊断，保持 B transport 与资源边界；当前已足够确认 default recovery，不要求同样重复操作。

[机器记录](P030_SOURCE_FIX_V2_B_EXECUTION.json) · [先前脚本格式根因](P030_INITDIAG_B_EXECUTION.md)。
