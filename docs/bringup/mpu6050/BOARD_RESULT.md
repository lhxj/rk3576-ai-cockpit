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
| 实物信息 | 用户最终确认EBF410513V2R0 20260521，与图纸一致（更正先前V0R1输入）；风扇4/6、5V4A、电流无标；模块尚未供电，测量待回报 |
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

当前候选I2C9_M1 Pin19/23，VCCIO3设计3.3V；模块电平/Pin1待测与确认。
I2C9 clock/reset/权限、M0启动唯一epoch、NS容量、新固件内存预算等仍为明确设计门。
待审批变更清单是草案，没有新产物目标hash，不请求现在批准部署。
下一步先闭合测量/ownership，给WIRING_READY_FOR_USER并等待断电接线确认；
Host实现可以在信号未接时推进，但不把Fake算硬件证据。本轮结束在第一审查里程碑。
