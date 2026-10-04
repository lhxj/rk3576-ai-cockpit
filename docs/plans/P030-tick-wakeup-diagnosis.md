# P030：首次延时的 tick / ISR 诊断与失败停止

2026-10-04。用户要求解决当前问题。沿本任务全部操作授权；物理冷启动与单次 B 由用户完成，Agent 不 source/M0/KO/MMIO/重启。

## 目标与事实

source-fix-v2 实板已通过 source、FIT 签名/payload、M0 entry、配套 Linux；remote_init 返回，link_probe up=0，停在首次 mdelay begin，用户确认持续无后续输出。用户已完整冷恢复默认 Debian，串口和只读 SSH 身份通过。不能把全零 tick 或 msh 提示符直接当特定硬件根因。

## 本轮一个里程碑

1. 核实际 ELF/板级/SysTick/NVIC/线程定时器路径。增加仅 MCU 本地的有界 tick 预检：记录配置返回值、CTRL/LOAD/VAL/CALIB、PRIMASK/ICSR、handler 和 ISR 计数。失败停止 echo，不盲改共享时钟/映射/transport，不用可能不工作的 RT 延时给诊断自身计时。
2. fresh archive + 既有补丁 + 新诊断补丁，clean compile；核向量、尺寸、包内实际 bytes、签名工具身份、现有 control key 验签、正确单组件 SCRIPT 与长度门。用户未要求测试套件，本轮不新增或运行测试；编译、封装校验和必要读回属于交付所需工作。
3. 核默认 Linux / 完整 U-Boot / 原启动与旧包。持锁有界，只新增独立 tickdiag-v3 目录、逐项读回与 RAM 清理；交付 Windows 记录/指南。实际时钟/IRQ根因和 B 修复仍依赖下一次用户双串口实测，不提前标 PASS。

## 文件与回退

新增 patches/0013-m0-tick-preflight.patch、scripts/amp/p030_prepare_tickdiag_v3.py、该计划与 reviews/P030_TICKDIAG_V3_*。既有物料保持；二进制/原始日志/私钥在 ignored artifacts，不提交凭据。不得覆盖旧目录。任何不匹配/安装失败停止并保存原因；新 B 异常停止，完整冷断电回默认 Debian，C/D 仍关闭。

## 进度

- 已读实际旧 ELF board-init：确实调用外部源配置但不消费返回值；ISR动态安装，静态 weak SysTick vector 本身不是未安装的证据。
- 完成：最小预检与干净编译、现有 control-key FIT/容器/脚本校验；v3 暂存后发现128字节console截断风险，暂停v3，新增0014短行并fresh v4构建/签名。
- 完成：v3/v4均只新增独立目录并读回；factory/旧tree保持、RAM清理、当前default身份保持。v4最长诊断99字节。没有新测试套件、Agent M0/KO/MMIO/重启。
- 下一步：用户单次冷v4 B，按cfg/VAL/ISR/tick决定具体修复；不把预检包交付标作硬件故障已解决。
