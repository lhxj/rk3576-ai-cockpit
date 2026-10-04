# 第一轮结果：资源、接线与基线审查

2026-10-05。**RESOURCE_REVIEW_IN_PROGRESS / WIRING_NOT_READY**。
没有传感器业务源码、派生固件构建或部署。所有传感器阶段均NOT_RUN。

## 实时盘点与本轮测试

| 项目 | 命令/结果 |
|---|---|
| Git基线 | 起点/远端系统tip均6e0aa7c83dd51de88e9f767dde09aadbba9336ea；3个锚点ancestor退出0；独立分支/worktree创建成功 |
| 首次只读SSH | 既有alias地址超时，SSH255；未重配全局SSH或忽略主机指纹 |
| 成功只读SSH | 2026-10-04T15:57:44Z起，`AMP_SSH_HOSTNAME=<历史共存地址> bash scripts/board/amp_post_recovery_readonly.sh`，持共用board_lock；SSH0、普通boot/运行DT副本取得 |
| 补充只读 | 同锁普通用户sysfs/name/of_node/driver/进程/RPMsg/模块有界读取，timeout40秒，退出0；没有sudo、debugfs内容、MMIO或I2C事务 |
| 实时boot | Debian默认6.1.99-rk3576 #8，model EmbedFire LubanCat-3-v2，root p3/boot p2；无amp_test_stage，RPMsg0/仅rpmsg_ns；项目进程/amp模块未见 |
| 总线 | I2C1 PMIC、I2C2 TypeC、I2C3 CAM0/VCM/codec/RTC全部保留；I2C9无子节点/adapter/绑定 |
| 实物信息 | 用户最终确认EBF410513V2R0 20260521，与图纸一致（更正先前V0R1输入）；风扇4/6、5V4A、电流无标；用户随后确认模块VCC→2/GND→14，测得VCC约5V、SDA/SCL各3.3V、AD0=0V；LDO未直接测 |
| 冻结产物 | 本地10/10已登记最小AMP产物重新hash匹配；SI_HEALTH七文件另存hash；未覆盖原源码/产物 |
| Host CI | `bash scripts/dev/host_ci.sh`退出0：31/31 CTest、47/47 Python、撤回保护5/5与shell检查通过；未删测试，仅当前系统基线回归 |
| ASan/UBSan | 本轮为文档/静态审查，未新运行；既有共存记录的sanitizer是历史，不冒充本轮sensor测试。后续实现门仍要求适用sanitizer |
| 新MPU业务Host测试 | NOT_RUN / 未实现FakeI2C/FakeTransport/codec/adapter/UI增量；不授予HOST_PASS |

原始日志/boot副本含环境信息，保存在Git忽略目录，不提交；脱敏索引见
[resource-evidence.json](resource-evidence.json)。读取没有启动M0/加载KO/改变DT/boot/
采流/录音。当前仍默认环境；本轮不曾进入AMP，因此没有本轮退出/冷恢复测试。

## 阶段等级

| 等级/测试 | 状态与限制 |
|---|---|
| HOST_PASS | NOT_ACHIEVED；基线CI通过不代表sensor逻辑通过 |
| RTOS_SENSOR_PASS / T2 | NOT_RUN；没有WHO_AM_I、配置读回或真实采样值 |
| RPMSG_SENSOR_PASS / T3–T4 | NOT_RUN；没有Linux sensor接口/真实样本核对 |
| UI_SENSOR_PASS / T5 | NOT_RUN；Qt业务值/人工变化确认未做 |
| sensor五分钟共存 / T6 | NOT_RUN；SI_HEALTH历史302.24秒仅保留原范围 |
| sensor退出与获批恢复 | NOT_RUN |
| MPU6050_RTOS_RPMSG_INTEGRATION_PASS | NOT_ACHIEVED |

当前候选I2C9_M1 Pin19/23，VCCIO3设计3.3V；模块SDA/SCL电平检查通过，Pin1方向/19/23空闲已确认，Pin20接串口调试器。
I2C9 clock/reset/权限、M0启动唯一epoch、NS容量、新固件内存预算等仍为明确设计门。
待审批变更清单是草案，没有新产物目标hash，不请求现在批准部署。
下一步先闭合测量/ownership，给WIRING_READY_FOR_USER并等待断电接线确认；
Host实现可以在信号未接时推进，但不把Fake算硬件证据。本轮结束在第一审查里程碑。

## 续审增量（2026-10-05）

用户静态电压回报仅作为USER_REPORTED_MEASUREMENT；没有WHO_AM_I或数据样本。
重新核对TRM p618得PD_BUS常开虚拟域来源；BSP审查发现I2C自动PREV probe早期写控制器、
所有阶段gate切换、通用I2C条件配置I2C7、timeout单位和错误返回遗漏。
[接线/ownership](WIRING_OWNERSHIP.md)补充必须修正的最小适配与资源就绪时序。
本次续审只有Host只读源码/PDF与文档更新，未再次SSH，未写SDK或运行板端程序。
仍WIRING_NOT_READY / OWNERSHIP_NOT_CLOSED，不能发部署审批包，sensor阶段仍NOT_RUN。

## 2026-10-05 实板准备尝试（最新状态）

先前段落的“当前默认环境”是对应只读审查当时快照；此刻已正常关机，不能据历史快照写成默认系统仍在运行。

- v2八文件被动新增安装及精确读回PASS，原默认/冻结/SI资产保持；权限诊断仍未执行。
- 首双UART尝试因COM5 AccessDenied停止，未关机或启动新固件；原空日志保留。PowerShell失败关闭修复后，用户确认断开串口软件连接。
- 第二次双UART成功发出UART_CAPTURE_READY；主控关机前检查与正常shutdown持共享board_lock。COM5 `resource-probe-live-com5-attempt2.log`记录Power down，支持正常关机已观察。
- 关机导致SSH预期断开；锁会话61397因set-e退出（exit1），锁随该会话释放。不能表述整个cold窗口始终持锁；下一实际进入必须主控重新持锁并正确处理预期SSH断开。
- 用户已被要求拔主电、等10秒再上电，但当时尚无用户完成回复，也未见soc cold boot。采集startup120秒自然结束：会话88023 exit0，COM5=4702B、COM6=2B、source_once=False。当前采集与锁均已结束。
- 未发送LOAD/INSPECT/SOURCE；无新FIT/M0启动、KO加载、权限诊断MMIO或MPU I2C事务。正常shutdown自身的默认系统行为不冒充诊断读。

最新等级：`PASSIVE_INSTALL_READBACK_PASS / NORMAL_SHUTDOWN_OBSERVED / USER_COLD_POWER_ACTION_PENDING`。板当前已关机，等待用户接回主电后默认启动与实时核验；没有诊断PASS或恢复PASS。不得沿用该次自然结束的采集会话/旧锁盲启动，也不自动warm reboot、M0reset或同会话retry。下一动作仅由主控重新持锁、重新采集、核实际启动身份后决定。

本条不改变HOST_PASS/RTOS_SENSOR_PASS/RPMSG_SENSOR_PASS/UI_SENSOR_PASS/最终集成均未取得的结论。原始尝试日志保留，不覆盖或把缺冷启动/缺END改写为通过。

## 2026-10-05 attempt2后默认冷恢复（最新实时状态）

用户已回复上电；主控最新只读baseline+实际validator成功，日志resource-probe-root-after-attempt2.log。默认kernel6.1.99-rk3576/rootp3boot2、无probe marker/KO/RPMsg或项目进程；25文件大小/hash一致，`DEFAULT_COLD_RECOVERY_25_HASH_PASS`。前一条已关机等待状态仅为历史，此刻默认系统已恢复运行。attempt2没有新固件/KO/定向读，因此仍不取得资源访问诊断或传感器PASS。

人工准备采集修为300秒等待首次实际cold marker→120秒coldstartup→一次SOURCE后120秒diag，绝对总540秒/每路总256KiB；无自动retry，生产M0/Linux120/90秒窗口及v2包不改。六模式实际PS factory/clock fixture通过。主控最新host_ci31/72/5通过。下一尝试由主控重新持共享锁，并显式处理正常shutdown的预期SSH断开，未获实际输出前不写诊断通过。
