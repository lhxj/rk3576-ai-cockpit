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

## 2026-10-05 attempt3冷启动捕获与未进入诊断

主控持锁启动attempt3双UART、READY后正常shutdown。该次观察到真实soc cold boot及原SPL/controlDT/U-Boot149b1c5/policy0身份，但实际autoboot要求CTRL+C，而采集器旧提示匹配/ESC错误导致未中断、启动默认系统。未LOAD/SOURCE/新M0/KO/诊断MMIO/I2C。原日志resource-probe-live-com5-attempt3.log保留。采集会话71938自然结束exit0，COM5=77074B/COM6=17B、sourceFalse、COLD_STARTUP、total194.76秒。主控实际默认恢复核验resource-probe-root-after-attempt3.log为25文件hash一致PASS；共享锁55635处理预期SSH断开后持续持有至默认确认，随后退出释放。当前默认系统已恢复，资源权限诊断仍未执行。

已修固定实际提示和一次0x03，并增加无板命令CANCEL。八模式production factory回归通过，截止300/120/120/绝对540及每路256KiB保留。下一实际动作须主控审核该修正后统一执行；子代理没有操作板。

## 2026-10-05 attempt4接线前里程碑（最新）

**WIRING_READY_FOR_USER / USER_POWER_OFF_WIRING_CONFIRMATION_PENDING**。主控在共享锁内完成真实冷进入、一次独立I2C_RESOURCE_PROBE_V1诊断、正常退出及默认冷恢复；不是传感器业务验收。实测M0按白名单顺序读取10个地址一次，BEGIN=1/END=1/status=0，新增诊断寄存器写=0、I2C事务=0、样本=0。Linux预检确认派生owner绑定、I2C9禁用无adapter、固定24MHz及clock引用/保护；实际health握手后仅发一次诊断，HELLO_ACK=1、PING/PONG=3/3、timeout/error=0/0、RTT=2ms、elapsed=2508ms。KO正常卸载并再次核实不存在；双UART结束、正常shutdown后用户冷恢复，默认6.1.99-rk3576/rootp3boot2、RPMsg/项目进程为空、25个默认/冻结/SI文件hash一致。共享锁一直保持至默认确认后释放，所有会话结束。

M0实际白名单只读访问证据闭合，Linux ownership派生配置在本次启动中已实测保持。此证据不证明寄存器写权限、I2C交易、IRQ投递或WHO_AM_I，不推导整个BL31策略或suspend行为。完整HOST_PASS/RTOS_SENSOR_PASS/RPMSG_SENSOR_PASS/UI_SENSOR_PASS及最终集成等级全部NOT_RUN；未进行新的五分钟sensor共存。

见[脱敏实际结果](RESOURCE_PROBE_BOARD_RESULT.json)；原始日志与原review JSON保留忽略目录，原review恢复PENDING为生成时事实，不覆盖。最新Host CI31/31 CTest、72/72 Python、5/5撤回，八模式UART fixture通过；本次仅文档收敛，不改v2生产包、不再诊断或冷重启。下一步止于等待用户断电接线确认。


## 2026-10-05 正式MPU_SENSOR_V1被动安装（当前）

主控持共享锁，按用户既有授权仅新增`/boot/amp-p029/mpu-sensor-v1`，未覆盖原默认/AMP/SI/诊断路径。实际`MPU_SENSOR_PASSIVE_INSTALL_READBACK_PASS`，v3十文件hash均与精确清单一致；SENSOR.json身份sha256 `4bf4748465e184939bacd821e3c6c6491bce738eb35a90abafa901ad2a7991ec`。证据`artifacts/local/mpu-root-review/sensor-package-passive-install.log`，实际输出`boot_chain_started=false`、`module_loaded=false`。

该记录时尚未shutdown/启动新固件/加载KO/访问MPU，正在主控串口准备；真实sensor阶段与最终集成等级仍NOT_RUN。忽略目录v3/SENSOR.json和跟踪FORMAL_SENSOR_BUILD.json内`board_deployed=false`保留构建时事实，不因后续安装改写原清单。当前安装记录不代表T1–T4/恢复/Qt或五分钟共存通过。


## 2026-10-05 UART日志live-read修正与正常停机（当前）

主控双UART_CAPTURE_READY后正常shutdown；SSH255属预期disconnect，COM5保存证据`reboot: Power down`。初版collector日志FileShare.None阻止主控live读取并核Uboot身份，主控发CANCEL，采集会话27924正常exit0关闭端口；此记录时主控锁会话20277仍持有。板已正常关机，用户尚未获cold上电操作请求，未LOAD/INSPECT/SOURCE、未启动新M0/加载KO/访问MPU；不称冷进入/诊断/传感器或恢复PASS。

外部独立collector-v2仅将日志File.Open改四参FileShare.Read，hash `47a3ae26520aac69352a31aa0abc7fcd26d36409355882c47035443e08d3a926`，实际.NET从生产Open表达式验证writer保持打开时Write/Flush与第二reader完整读取PASS，第二writer拒绝PASS。产物`artifacts/local/mpu-sensor-collector-v2/capture-sensor-dual-uart.ps1`，测试`tests/powershell/test_sensor_capture_share.ps1`；见SENSOR_COLLECTOR_V2.json。固定v3十文件/原collector历史/诊断v2均不改；该修正不增加任何板命令。后续仍由主控审核并统一操作。


## 2026-10-05 真实T1–T4证据与UART截断调查（当前）

主控唯一执行runner exit0：health HELLO_ACK1/PINGPONG14/14/timeout0/error0，两独立client各100有效样本、各matching UNSUB确认1/gaps0/protocol_errors0，正常rmmod health/sensor及节点释放。WHO68、power01、accel/gyro fs00/00、DLPF03、divider31与config00010331实际读回。原始100 M0raw与Linux首组100 raw按seq/m0_ms/config匹配，全部7原始int16字段100/100一致；后组seq103..202，跨客户端递增，无旧sample复活证据。实际每组subscription都是3，client完成QUERY/BIND后++request形成3，续租不改subscription，仅请求号递增；id是session内作用域，两个进程可相同，session未打印不得称日志证明其不同。

实际BSP `rtconfig.h` RT_CONSOLEBUF_SIZE=128，`src/kservice.c:rt_kprintf`以size127调用vsnprintf，再clamp length127发送，长行终止NUL被发出且丢换行。配置尾的read_target_hz/publish_cap_hz/raw_trace_limit缺失，不重建；epoch及WHO/power/fs/dlpf/divider/config/odr20完整保留。紧随NUL的真实MPU_RAW1与另99行全部完整；Host比较器仅恢复这个唯一NUL+MPU_RAW边界，拒绝值内NUL/非完整raw字段。真实日志fixture与合成同framing/错误拒绝2/2通过，不把合成数据算硬件证据。原始日志保留忽略目录，脱敏结果与hash见SENSOR_T1_T4_RESULT.json。后续固件配置行应缩短到≤126B含换行；本次没有修改/重建固件或再启动M0。

实际100行M0时间间隔50..60ms、均值50.303ms（99个间隔；见JSON实测统计），目标内部ODR/轮询/发布cap各20Hz，不能将目标称实测20Hz。尚未温漂/精度校准。启动ES8323/I2C3若干-6错误保留，T6前必须复核，不能据此称媒体无回归。

启动身份角色纠正：实际SPL version为`8f53f800da-241224`；历史“7d8fe670”是loader检查的uboot-image hash前缀，不是SPLcommit。旧日志与当时表述保留，本段明确纠正角色，后续核identity按真实角色。

主控随后正常shutdown/Powerdown、CANCEL释放采集，用户已确认默认恢复上电；此记录时冷恢复hash核验仍待主控报告。真实RTOS/RPMsg字段核对证据交主控审核，尚不自行升板阶段等级；Qt人工/五分钟共存/最终集成NOT_RUN。


## 2026-10-05 T1–T4后默认冷恢复完成（当前）

主控实际默认冷恢复PASS25：`artifacts/local/mpu-root-review/sensor-window-default-after.log` UTC2026-10-05T06:24:12Z，6.1.99-rk3576/rootp3boot2、RPMsg设备/相关模块/项目占用空；25保护文件bytes/hash/resolved/symlink与正式before一致。主控锁20277正常释放、collector关闭、全部板会话结束。本轮已停止，不再部署/重启。

RTOS_SENSOR_PASS（真实WHO/配置/100raw）与RPMSG_SENSOR_PASS（实际200接收、首组100逐raw字段核对）可依据现有真实证据记录；T4两实际客户端各matching UNSUB RESULT与跨组seq递增/正常卸载通过，numeric session未打印保留限制。UI_SENSOR_PASS、静止合理性/安装轴系观察、Qt人工确认、至少五分钟组合共存与最终MPU6050_RTOS_RPMSG_INTEGRATION_PASS仍NOT_RUN。

用于核对的是测试结束时原样复制的`sensor-live-m0-snapshot.log`（确切hash见SENSOR_T1_T4_RESULT.json），不是关机后的final日志。后者shutdown尾部另含NUL，strict解析拒绝，保留原始final及sha89cb97fabd6fb43dfa04e606630ca039fddb9f85003fe91b3e7896a9f78232a1；未放宽为忽略所有NUL，未声称final全日志解析通过。ES8323启动-6错误与配置长行截断仍为后续T6前待修/待核，不作媒体无回归声明。

主控独立对同一原始snapshot核对100/100 PASS，证据sensor-root-raw-correlation.json。200有效样本统计：第一组M0间隔50..60ms平均50.303ms，第二组全50ms；加速度模长0.92584..0.94708g、均值0.93664g；陀螺平均(-6.63878,1.20511,0.07817)°/s；MPU芯片温度29.165..29.804°C。模块是否固定静止尚未人工确认，不能声称陀螺近零或精度校准PASS，X轴偏置待后续固定静止复核；风扇原接线保持。seq101/102在两订阅间未发布，而publish_seq连续1..200，不能计为传输丢包。本轮到T1–T4里程碑及默认恢复停止，不准备/执行新T5/T6窗口。

## T5/T6准备续步（未执行）

用户刚重新固定模块。主控当前默认L1保护25文件及上一轮v3十文件读回hash/metadata通过，known ES8323启动错误保留。四ARM ELF/native-v8/独立11文件package-v2及主控精确审核通过；尚未安装/启动该窗口，UI/静止/五分钟共存仍NOT_RUN。见[具体执行与冷恢复](FORMAL_T5_T6.md)。

本窗口后续准备事实：主控sole执行v2被动安装11hash读回PASS（artifacts/local/mpu-root-review/t5t6-passive-install.log），双UART_READY后持全窗口锁21685正常shutdown，SSH255与COM5 Power down；记录时等待用户cold动作，未LOAD/SOURCE/新M0/KO/MPU访问。构建JSON board_deployed=false保留原始事实，实际安装记录另列。

首正式T5/T6窗口：fuser --在板端返回中文usage而guard停止，Qt/WHO尚未开始；无--定向验证FREE。独立app-only未执行（预算不足），主控health407→409/error0，正常两KO卸载/node释放/Power down/CANCEL；用户已冷上电恢复默认Debian，生产validator确认25默认/冻结/SI加旧v3十文件共35 hash/metadata PASS（t5t6-guard-stop-default-after.log、t5t6-guard-stop-previous-v3-after.log）；锁/双UART关闭。不是MPU硬件失败，T5/T6仍NOT_RUN。冻结十一文件不变，下一窗口使用已审独立userdir loaded-controller并重新核remaining/ABI/live build-id。

当前下一窗口仅用户目录loaded controller480秒；Linux health720000ms、M0 900000ms，SOURCE真实age<=390秒及health elapsed<=200000ms分别核验。旧冻结runner禁止重跑。最新Host CI34/111/5 PASS；T5/T6/UI/最终集成仍NOT_RUN，Qt方向确认必须等待用户实际观察。


## 2026-10-05 MPU6050 T5/T6 attempt6：SOURCE起点超时，默认启动已确认（当前）

冻结collector-v2未修改，也未创建v3；attempt3/4在UBoot等待人工确认期间120秒startup窗口到期，均未LOAD/SOURCE/新M0。用户明确要求保留原v2并重测。attempt6实际一次SOURCE成功（2026-10-05T08:42:37.0239293Z），readonly preflight PASS，主控正常加载原health KO（PONG14/14、elapsed14505ms、timeout/error0、window720000ms）及原sensor KO。

loaded入口因传入SOURCE age>390秒触发守卫而fail-closed；入口精确age未记录。失败后主控追加诊断，于2026-10-05T09:01:09.4982628Z测得距SOURCE 1112.4766秒，该值不是入口精确age。Qt、WHO及T5/T6业务未执行，本次无新MPU业务读取；不是错误风暴或MPU硬件失败。collector95656按DIAGNOSTIC900秒正常到期，source_once=True，COM5=83197B/M0=105948B；退出前末health PONG693、timeout/error0，但phase=DONE/elapsed1132408ms/last_pong_age412459ms，不能称运行时health仍READY。

主控正常卸载两KO并确认节点释放exit0；随后纯被动collector37207记录正常shutdown及UART1291.859648 Power down，CANCEL exit0释放端口。用户随后默认冷上电；主控首次SSH No route，等待10秒后第二次SSH成功，实际uname -r确认默认6.1.99-rk3576已启动。按用户要求未重复保护hash，本次仅默认启动identity确认，不能记录35项新核验完成。主控释放marker已写、锁会话75137 exit0，全部串口已关闭。历史HOST_PASS、RTOS_SENSOR_PASS、RPMSG_SENSOR_PASS保留；UI_SENSOR_PASS、五分钟共存及最终集成仍NOT_RUN。证据artifacts/local/mpu-root-review/t5t6-attempt6-{preflight,runtime,normal-release,final-shutdown}.log及Windows ready-attempt6/attempt6-shutdown。

下一窗口仅提出主控审核的有界单入口：在SOURCE前准备好用户目录编排与日志，保持原collector-v2/identity/人工LOAD→INSPECT→SOURCE门；SOURCE后一次入口读取实际SOURCE时间、核age与冷启动身份、执行冻结readonly preflight、确认无旧模块、正常加载原health KO并有界等HELLO_ACK/PONG>=3，再正常加载原sensor KO并立即调用同SHA loaded脚本。入口需在每阶段重新核剩余时间、异常停止且保留引用供主控正常退出，不能重试/自动SOURCE/延长Linux720000ms或M0900000ms。loaded仍SOURCE age<=390、health elapsed<=200000、总480秒；Qt静止窗后方向观察需用户实际确认。此项是计划，尚未实现/执行。

---

## 2026-10-05 MPU6050真实Qt通过，T6同载失败，默认启动已确认（当前）

**UI_SENSOR_PASS / USER_CONFIRMED；T6_FAILED；最终MPU6050_RTOS_RPMSG_INTEGRATION_PASS未通过。** 用户手动runner首调用SOURCE/入口健康/ABI均通过，首次fuser守卫停止，未启动Qt；原rc/stdout/stderr未导出，具体原因UNKNOWN，后续FREE不能倒推首次。主控在同获批窗口持锁43624，确认实际FREE后只重跑原loaded一次（SOURCE age185.826秒，health入口<=200秒，不重载KO/不重启/不改冻结脚本）。

T5真实90秒Qt观察及正常退出：90组1Hz记录，seq/pubseq20..1804、age0..11ms、gaps/sample_errors/protocol_errors0、UI merges40..2813；退出matching UNSUB确认。固定静止[首次VALID+10,+25)秒15组，加速度模长0.923374..0.968406g/均值0.936823g，gyro均值(-6.34249,0.73028,0.14453)°/s，预设功能容差通过。用户明确确认此前模块固定静止、页面正常且数值随方向变化；这是USER_CONFIRMED功能观察，不是精度/零偏校准PASS，不减偏置。

T6实际Qt/Core、CAM0预览、RKNN、单MPP RTSP及录像、真实ALSA/VAD/Sherpa启动门先后通过（synthetic_FINAL=0不等于真实FINAL=0）。紧随T7进入全组件循环，四consumer active的合取守卫失败：`coexistence consumer stopped`。未满300秒，无APPLICATION_METRICS/SENSOR_COEX_METRICS/COEXISTENCE_DURATION或application success；Qt preview149 converted/delivered、fps6.8194、sensor UI merges848及RTSP decoder421帧仅为短时观察，不能作为共存PASS。实际四bool/错误详情未导出，具体consumer及根因UNKNOWN。资源4组间隔约5秒，仅启动短窗证据，不外推稳定性。主控第二次Qt/T6期间原manual collector已CANCEL，无同期M0 raw UART，因此本次不声称raw逐字段关联；历史T1–T4 raw100关联仍保留原范围。

主控随后实际health353/353、elapsed367706ms、READY、timeout/error0，正常rmmod两KO及node释放exit0，fuser空/8554空/项目actor空；kernel-after已保留。纯shutdown collector观察UART435.001991 Power down，CANCEL exit0。用户默认冷上电后主控uname确认6.1.99-rk3576，锁43624释放exit0、串口关闭。遵用户要求本次未重复保护hash，仅默认启动identity确认，不能称新35项恢复核验。证据t5t6-manual1-loaded-second.log、t5t6-manual1-board-logs/{application.log,resources.jsonl,rtsp-progress.log,rtsp-decoder.log}、t5t6-manual1-normal-release.log及主控kernel-after/恢复记录。

下一步仅诊断增量：失败时有界输出四consumer快照、媒体计数/last_error hex及stop请求计数，另记canonical四状态/真实Voice FINAL与intent计数；保留原四项active/错误/300秒门，不改资源或业务。Host源码审查未发现sensor canonical更新覆盖media或UI状态回调自动stop路径；当前尚不能声明根因已修。新source-v4与独立app-v4 native builder已准备，旧archive/v3 ELF/collector/FIT/KO/DT保持冻结；主控在默认6.1.99完成独立app-v4 AArch64原生构建exit0、295成员/source一致及四ELF readback/hash/ldd全resolved PASS（t5t6-diagnostic-native/application-build.json及t5t6-diagnostic-native-readback.log）；没有运行应用或改变boot/KO。原生构建锁16069已释放。新v4独立controllers已由主控仅部署fresh用户目录并核3脚本size/hash读回PASS（t5t6-v4-controller-stage-readback.log），无boot/KO/runtime操作；manual-run-v2已准备，20项production controller测试及PS5检查通过，诊断实测仍NOT_RUN；清单DIAGNOSTIC_V4_BUILD.json。Host CI35/119/5 PASS（t5t6-consumer-diagnostic-host-ci-final.log），不是板端诊断PASS。主控v4测试前默认预检确认无两KO/node/应用/8554占用，随后正常shutdown（SSH255、UART1266.208720 Power down）；日志Windows Documents/MPU6050-Test-20261005-v1/v4-pretest-shutdown/linux-com5.log，collector22805 CANCEL exit0、锁76035释放exit0，串口已关闭。当前板已正常关机，manual-run-v2准备就绪但NOT_RUN，等待用户手动命令与冷上电；未声明共存问题已修复。

---
