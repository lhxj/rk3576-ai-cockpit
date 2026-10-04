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

## 2026-10-04 v4 实测后：UART 参照与条件重载修正

用户 v4 COM6：配置均 HAL_OK，CTRL=3、LOAD=239998、VAL 变化、PRIMASK=0、handler 匹配；1048576 次读取只观察到 48752 次变化，wraps=0、ISR/tick delta=0，预检 STOP。**未跨周期，不能据此判 IRQ 失效。** 之前固定读次数覆盖不足由主控承担。用户明确完整冷恢复；获准持锁只读 SSH exit0 再确认默认 6.1.99-rk3576 #8、root p3、无 stage、无 echo 模块/RPMsg 设备。

实际固定 SDK：evb defconfig 未启用 RT_USING_32K_TICK_SRC；同 RK3576 vehicle-evb 启用该配置，hal_conf 映射到32768。24MHz常量不是本板输入频率证据。TRM 下载超时，无完整文档证据，不据搜索摘录判断实际输入。

本轮一个里程碑：新0015补丁及v5沿原授权先编译/签名/核封装，只新增独立目录。固件使用已分配UART5的115200 8N1物理发送作粗略独立时间参照；检查板配置和LCR，每阶段固定6144 raw bytes，发送前后等待移位寄存器空，新增轮询均有1048576读预算。基线保留原LOAD，不依赖RT delay；计数15000..21000且wrap/ISR/tick一致才接受32K候选并短暂屏蔽IRQ，关闭/配置/开启**MCU本地SysTick**（327 cycles，LOAD326），保持EXT源与共享时钟。第二阶段要求ISR40..75且RT tick同增量才允许首次延时/RPMsg。不同频率或异常停止，不继续猜测别的频率。不是精密校准、硬实时看门狗或任何后续故障保证。

UART5 raw THR写入只发送诊断字符，LSR/USR为该已分配外设的状态读取；没有新pin/clock/reset/安全寄存器写入。Agent仍不通过SSH读写MMIO或自行启动M0/KO/重启。用户另一次冷B确认率门、首次delay返回、约15秒link timeout；C/D仍关闭。新记录和指南存Windows桌面；不新增/运行测试套件。

完成：v5 fresh构建无warning/error、现有control-key验签、payload/容器/SCRIPT核验；最终短行上界107字节。RAM上传后安装连接等待密码超时、未运行installer；随后重新持锁核同archive，原default/完整U-Boot8MiB/factory/旧tree/paired检查通过，一次新目录安装及四项独立读回通过，回执b55739cb9d7da4f6e9c9e194f86a1766d0ed922cc411e175c7aa7da0482f0ac8，RAM清理。Windows桌面RK3576-AMP-P030-Tickdiag-v5包含指南/执行/准备/身份索引及包；旧Windows目录添加暂停通知。待用户单次冷B确认实际时基和首次延时，不能把被动暂存写成修复已通过。


## v5 实板结果 2026-10-04

用户两路日志已证实：baseline17448计数/无wrap；LOAD修正为326后ISR/tick均54增量；first_mdelay end delta106（含打印）；heartbeat507/1007/1507后预期15s超时退出。所观察窗口首次延时和计时等待恢复，支持约32K输入与24MHz重载假定不匹配。COM5 source/FIT/M0 loader/paired Linux通过。附件仍B身份，完整冷恢复待确认；共享mapping/RPMsg/长期稳定性未证。当前一个里程碑为归档实际结果、确认冷恢复、梳理新C准备门；不复跑B，不自行加载KO或重启。无测试套件。


用户随后明确完整冷断电恢复默认Debian并补uname/cmdline：6.1.99-rk3576 #8/root p3/无stage/uboot149b1c5。本次B运行与冷恢复通过，所观察首次延时问题解决。未重复SSH或改板；C Host准备门打开，可执行C入口仍缺新版签名FIT路径/长度/正确SCRIPT结束项/manifest/安装读回，不执行旧C。下一独立里程碑为沿用v5 M0的新版C通信验证物料；整体D/共享映射/双向RPMsg未证明。
