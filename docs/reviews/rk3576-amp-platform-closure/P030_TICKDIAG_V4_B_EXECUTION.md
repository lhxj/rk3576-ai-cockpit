> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# P030 v4 实板执行记录

2026-10-04。COM5确认3055B/0xbef、新v4 banner、RSA p029dev及MCU payload hash通过、loader成功、配套6.1.99-rk3576-m0echo-p026/root p3/stage=B登录。COM6配置返回均0、CTRL3、LOAD239998、VAL239235→189841、PRIMASK0、向量匹配；1048576读的changes48752、wraps0、ISR/tick delta0；预检STOP，未进入RPMsg或RT延时。

**没有观察到回卷，不能据此判定IRQ故障。原固定读次数探针覆盖不足由主控承担。** 当前计数器在变化；24MHz频率假定需要核实。msh或tick0不能代替完整周期证据。

用户已完整冷断电回默认Debian；随后持锁有界只读SSH exit0确认6.1.99-rk3576 #8、root p3、无amp_test_stage、无echo模块/RPMsg设备。COM5附件结束于B会话，恢复由用户报告及独立SSH确认，未混写为附件内容。原附件SHA登记在JSON，原始登录日志不提交/复制。

固定SDK同SoC vehicle-evb配置32K tick，evb未启用，hal_conf分别定义32768或24MHz。此差异支持32K候选，尚不证明本板输入。v5使用UART5物理发送为粗略参照，条件匹配才修正本地LOAD；硬件结果仍待单次冷B。完整mapping/cache/RPMsg/D未通过，C关闭。Agent未自行启动M0、KO、MMIO或重启，未运行测试套件。
