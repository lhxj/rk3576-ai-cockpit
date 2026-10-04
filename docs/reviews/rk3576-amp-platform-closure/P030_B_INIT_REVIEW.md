> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# P030 上的阶段 B：启动已观察，等待超时尚未闭合

2026-10-04。主控已全读并接受既有子代理的独立 Host 审核。等级保持 **C. HOST_BUILD_PASS**，D 关闭；不执行阶段 C、不加载 echo KO、不重复 source。

## 本次已有实板证据

- COM5：冷启动 proper U-Boot `149b1c5`，SPL payload 前缀 `7d8fe670d9`、公钥控制 DT 前缀 `43164981ef`；一次 load/source、required-conf 与 MCU hash 通过；最终 DT 检查通过，配套 `6.1.99-rk3576-m0echo-p026` / B 到 Debian 登录。
- COM6 用户摘录：RT-Thread、`local_fn=0x66ad`、`link=0x4`、初始 `cache ctrl=0x6cc bypass=1`、`msh >`。用户确认无后续输出。该 Thumb 指针与实际 ELF 符号匹配；link 是配置 ID，并非连接证明。
- 主控一次持锁、40 秒外部上限的只读 SSH exit0：完整 8MiB 存储与 P030 镜像匹配；运行 DT 的代码/两段共享区精确 no-map、没有 reusable；12 内存槽、iomem 的 code/shared 为 reserved。Linux transport/两 mailbox/UART5 禁用，无 RPMsg 设备或 echo 模块。原 six、旧23和 signed-v8六文件哈希一致，没有写板、MMIO、启动 M0 或重启。

当前存储读回证明镜像身份；用户直接执行 B 前未回报的默认冷启动门不能倒写为已通过。P030 上原厂默认 Linux 冷启动仍待确认。详见 [机器记录](P030_B_EXECUTION.json)。

## 缺少的关键观测

预期 B 在 Linux transport 关闭时输出等待信息，再报告 15 秒 link timeout。当前均未观察到。

实际 `amp_echo.c` 在 `rpmsg_lite_remote_init()` **返回之后**才开始 15 秒计时。初始化会经过 heap、RT 信号量、虚拟队列对象、平台 mailbox/IRQ；它没有设计上的 Linux link 等待，应用层超时也没有包住初始化。队列创建只计算共享 ring 指针，本身不能证明共享物理映射可用。

因此，缺输出还无法区分初始化未返回、首次延时/tick 未推进或串口观测缺口。`msh >` 也不能证明 M0 tick 持续推进。不能把它唯一归因于 CON17 映射，或通过打开 Linux transport 绕过 B 的观察要求。

实际 v3 配置选 `evb`，tick 使用 100Hz 私有 SysTick，外部源为 `PLL_INPUT_OSC_RATE`；冷方案没有使用 TIMER11。Linux `[25.896090] clk: Disabling unused clocks` 只是时间相关记录，尚不能证明它停掉了 M0 tick。

## 独立发现的源码边界问题

固定 `rpmsg_platform.c` 的局部 `mbox_cl[MBOX_CHAN_CNT]` 数组只有一项，M0 分支却按 remote ID 4 索引。这是源码越界缺陷；当前 v3 ELF 中该局部数组已被优化消去，反汇编直接传递选中的 client 指针，不能判定它导致本次现象。下次构建前须单独处理和审核此源码问题，不能以未经证实的因果声称当前硬件故障已修复。

## 下一步

先由用户正常关机、完整冷断电，按默认流程回原 Debian，保留 COM5 日志并确认 IP。随后最小诊断范围是 `remote_init` 前后、首次延时前后和 RT tick checkpoint；若初始化没有返回，再逐层缩小 heap/env/queue/platform-IRQ。这些仅是下一轮待审方案，本轮没有生成或运行新固件。

UART5 当前只有 RX+GND，不能直接输入 MSH 命令。完整映射、持续 cache/coherency、HELLO_ACK/PONG 和 D 验收仍没有证据。独立报告及原始输出只保存在忽略目录，身份记录在机器记录中。
