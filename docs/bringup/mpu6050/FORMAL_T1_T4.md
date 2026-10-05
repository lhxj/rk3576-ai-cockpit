# MPU_SENSOR_V1 正式 T1–T4 运行包

当前只完成可审阅产物，**未安装/未启动新固件/未读取MPU**。完整Host闭环仍HOST_PASS，真实RTOS_SENSOR/RPMSG_SENSOR/UI_SENSOR与最终集成等级NOT_RUN。用户已确认断电接线与重新上电，且明确下一步不需额外审批；主控仍须审阅确切组合并唯一持板锁操作。

## 实际构建与当前默认环境

主控普通用户盘点2026-10-05T13:43:47+08：默认6.1.99-rk3576、rootp3/boot2，boot_id f1699d67-29e1-4e50-b90a-8f885fe81c47，RPMsg空/无sensor或probe KO。正式before日志`mpu-root-review/sensor-window-default-before.log`与原before的25个默认/冻结/SI文件hash一致；这是本轮进入AMP前基线，不称本轮恢复。

ARM应用由主控在新的`/home/cat/cockpit/mpu-sensor-app-v2`实际构建，-j2、配置90s+编译360s、合计日志≤2MiB；未执行测试或应用。实际readelf AArch64、ldd无not found。v1导出遗漏tools导致配置失败，保留日志；v2完整282成员导出包Host解压配置通过，删除tools负例正确拒绝，不降低BUILD_TESTING/feature。导出是73cb1de基点的working-tree快照，精确归档sha5563ee…，不是纯73cb1de源码。后续M0 trace fixture差异另记，固件生产源由独立v7清单固定。

M0 v7原生SCons、ELF/map预算、配套FIT签名验证通过：entry0x141、image138392B、FIT143872B(0x23200)、bss_end0x26cbc、heap364356B；mainstack1024/sampler2048/service4096，运行高水位未测。原load/shared-memory/mailbox/CON16/17/transport/linker不变。实际FIT大小为0x23200，与清单和stage guard逐字一致。固定loader接受FIT≤0x90000、image≤0x80000；未修改bootloader。

`MPU_SENSOR_TRACE_V1`仅生成一次成功初始化WHO/power/fs/DLPF/divider原始读回行和最多100个**成功**采样raw行；驱动另有10项初始化读回一致性检查，不把未逐项打印的读回称完整原始寄存器日志。采样/发布目标各20Hz，实际间隔待实板。UART仅验收诊断，应用数据唯一来自硬件I2C→RTOS→RPMsg→Core。错误不刷新last_valid_ms、不消耗成功trace配额；连续3采样错误停止采样并STATUS，health继续。

## 确切安装与运行顺序（仅主控执行）

1. 审核`FORMAL_SENSOR_BUILD.json`与忽略目录`artifacts/local/mpu-sensor-package-v3/PACKAGE_BUILD.json`。10文件packet/installer已Host exact-members+hash验证。只新增`/boot/amp-p029/mpu-sensor-v1`，不覆写原最小AMP/SI/诊断包/默认文件。安装命令：`sudo -n python3 <staged>/install-sensor.py --install-MPU_SENSOR_V1 <staged>/packet`，安装器只拷贝及exact读回，失败停止，不启动M0/KO。
2. 主控持共享锁，核当前身份/占用/25文件before；COM5=1500000、COM6=115200、8N1/noflow/DTR RTS false。使用生成的`capture-sensor-dual-uart.ps1`并等UART_CAPTURE_READY，随后正常shutdown，用户拔主电等10秒重新上电；不得warm reboot替代。COM5/6各总256KiB，等待人工≤300s/首次实际cold后startup≤120s/SOURCE后正式窗口≤300s，总≤720s。CANCEL仅结束采集并释放串口，不发硬件命令。
3. collector只在实际`Hit key to stop autoboot('CTRL+C')`发一次Ctrl+C。主控核真实SOCcold/SPL7d8fe670/controlDT43164981/Uboot149b1c5/policy0再逐一写LOAD、INSPECT、SOURCE tokens；LOAD固定`/amp-p029/mpu-sensor-v1/stage-sensor.scr`到0x4c000000，INSPECT须filesize=c51(3153B)，SOURCE仅一次。脚本无saveenv/reboot，使用冻结Image/initrd、新ownerDT、新签名FIT；默认开机路径保持原样。
4. Linux上线后执行`sudo -n python3 /boot/amp-p029/mpu-sensor-v1/run-sensor-t1-t4.py`。只insmod固定health与sensorKO各一次，先HELLO_ACK1/PONG≥3/timeout0/error0，sensor设备由配套KO确认owner_ready之后用户服务才发送HELLO/READY。仅该固定char节点chown cat/mode0600，无全局udev。实际getrandom新nonce/session/subscription、两次各100成功Core样本，每个client≤120s，单行≤4KiB/每client日志≤256KiB；chunk缓冲完整换行转发避免health插断JSON。health每秒检查/每阶段进展。整个runner内部共享300s截止（client/命令剩余预算受此上限约束），外部timeout仅兜底。
5. T2看M0一次WHO68/config；100真实raw行。T3第一client100样本；T4正常匹配UNSUB RESULT后第二client新subscription100样本，再正常UNSUB。Host执行`python3 scripts/board/mpu6050/compare_sensor_raw.py <COM6-log> <runner-log>`，按(seq,m0_ms,config)交集≥10逐raw字段比较/同epoch/两subscription/seq跨客户端递增。早期M0trace可早于Linux订阅，不要求100条全交集；不足交集明确证据不足，不伪造。Linuxrx monotonic不与M0时钟相减。两client显式UNSUB RESULT必须确认，不能只把lease兜底称退订通过。
6. 正常runner确认每阶段health进展、无timeout/error，再rmmod sensor/health并查sysmodule/node释放。M0无订阅不持续发布，采样独立有界900s；health与sensor双owner退出后才释放transport。不要求额外M0reset。WHO/config初始化失败由M0发布错误并不启动采样；采样I2C错误先发布错误，连续3次才停止sampler。当前probe只输出有效SAMPLE，初始化失败或断流可能等至单client120s截止再失败，不能称runner在第一I2C错误立即停止。主控观察UART WHO/config异常或错误风暴时立即停止推进；runner的health错误、日志上限、deadline或退出失败则明确失败，不自动重试/扩探。失败保留KO/CCF引用供主控核查与冷恢复，不强卸载。外部kill不自动恢复硬件。
7. 继续双UART采集/锁，主控正常shutdown；用户再次拔主电10秒再上电，默认启动**不发送LOAD/SOURCE**。重新L1盘点到新after日志：`bash scripts/board/mpu6050/read_resource_boot_baseline.sh <after-log>`，再Host运行`python3 scripts/board/mpu6050/verify_resource_cold_recovery.py artifacts/local/mpu-root-review/sensor-window-default-before.log <after-log>`。必须25默认/冻结/SI文件hash/bytes/link一致、6.1.99/rootp3boot2/RPMsg空、无sensor/probe/healthKO、无项目进程、无mpu_sensor启动marker；锁保持至默认确认再释放。失败只记录，不擅自多次cold恢复。

T5人工Qt观察与T6至少5分钟全系统共存留待后续同组合窗口，本包不将T1–T4或Host数据冒充最终集成PASS。ARM Qt只构建，未显示/未人工确认。风扇4/6保持，MPU已接2/14/19/23；不再改线/重测LDO/接INT。

## 本轮Host检查

host_ci34/34 CTest、84/84 Python、5/5撤回通过；最后runner完整行/共享截止修改后必要3/3 fixture再通过。实际包正例与extra/missing/manifest/checksum/FIT/symlink六类拒绝通过，health错误/timeout/age拒绝通过，raw交集正例/原始字段错/epoch错/不足交集拒绝通过。主控独立trace实际100/1000Hz与who/config/setup失败入口ASan/UBSan/Werror/LSan1通过；nativefiles/source/patchhash与ELF/FIT审核通过。新派生collector仅AST解析通过，未将原diagnostic collector的factory冒称新sensor实物测试。

主控最终独立host_ci34/34 CTest、84/84 Python、5/5撤回exit0，日志`mpu-root-review/host-ci-package-final.log`；actual v3十文件hash/installer pin/validate_packet全部PASS。主控独立sensor派生collector AST PASS，sha adf3ddfa409803e8ffb507798cd98a7159b98cbb4ce8545d9864b7ad515aed8f。ARM v1导出失败仍保留，不将v2成功改写历史。

最终恢复reader补充实际BOOT_IDENTITY保留mpu_sensor标记；validator同时比较resolved/symlink（旧fixture缺字段兼容）。实际reader AST筛选及resolved/symlink变化拒绝回归，恢复专项7/7 PASS；固定v3包及ARM/M0产物未改变。


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
