> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# P030 v5 首次延时恢复的实板执行记录

2026-10-04。**本次已观察到 tick 推进、首次 RT 延时返回和有界等待退出。用户已确认完整冷断电，补充串口信息匹配默认Debian。** Linux RPMsg transport 在B关闭，当前未证明双向通信或共享内存有效映射。

## 实板事实

COM5：cold boot、proper g149b1c5/policy0；新v5脚本3055B、fileaddr0x4c000000/filesize0xbef；RSA p029dev及预期payload hash通过，单次loader成功；配套6.1.99-rk3576-m0echo-p026/root p3/stage=B登录。原附件末尾uname/cmdline仍是B；用户随后单独补充完整冷断电确认与默认系统串口身份，两份来源分开登记。

COM6：cache entry CTRL0x6cc/bypass1、HAL返回均0、PRIMASK0、向量匹配。

| 观察阶段 | LOAD | 计数增量 | 回卷 | ISR增量 | RT tick增量 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 原重载值的UART参照 | 239998 | 17448 | 0 | 0 | 0 |
| 条件修正后的UART参照 | 326 | 17449 | 54 | 54 | 54 |

随后rate gate PASS，remote_init返回非空，link_probe up0。first_mdelay begin tick61、end tick167、delta106，**首次延时确实返回**。106包含两端串口快照输出，不能当成精密阻塞时长；日志无独立墙钟时间戳。heartbeat elapsed ticks507/1007/1507，最终按预期打印link timeout 15s并退出echo任务，MCU仍运行。

## 原因和修复范围

每组6144个115200 8N1字符名义占0.533秒；17448计数对应粗略32715Hz，支持SDK同SoC的32768Hz候选，明显不符合24MHz。原LOAD239998按24MHz假设生成，在32768Hz输入下每tick约7.324秒，一次请求100tick的“一秒延时”约732秒。v5条件重配本地LOAD326后ISR/RT tick同步推进、延时返回，实板证据支持**输入频率与重载计算不匹配**是此前表面停滞的原因；没有证据支持“SysTick中断损坏”。这不是精密振荡器校准。

本次修复了所观察窗口中的首次延时及计时等待；未证明长期稳定性、精密时钟精度或缓存开启模式。B关闭Linux transport，所以up0与最终link timeout是预计结果，不应重跑B来消除此STOP。共享DDR有效映射、Linux实际收到HELLO_ACK/PONG与cache link/after-pong快照待新C阶段。

## 恢复与下一步

用户已明确完整冷断电恢复默认Debian；补充uname为6.1.99-rk3576 #8，cmdline root p3/boot p2、无amp_test_stage、fwver uboot149b1c5。**本次B运行与冷恢复通过，首次延时问题在本次实测窗口已解决。** 本轮未重复SSH，只依据用户物理确认和实际串口身份；不把原B附件误记成恢复日志。

新C准备门已打开，但新C可执行入口尚未准备/暂存/读回，所以C执行仍关闭。需要沿用已证明的v5签名FIT和配套C DT/KO，修正旧脚本签名物料路径、FIT长度以及正确legacy SCRIPT单组件结束项，保留配套Linux与全部内存保护，再准备一次冷C。**不要执行旧C或在本次B加载KO，不需要重复B。** 新C仍需同时核Linux真实rings/DMA backing、HELLO_ACK/PONG、M0实际共享指针和cache link/after-pong快照；D尚未通过。

本轮只分析用户日志及保存脱敏记录，没有Agent板端访问/写入/MMIO/M0/KO/重启，没有新增或运行测试。两个原附件仅登记SHA，原COM5登录日志不提交或复制；完整机器证据见同名JSON。
