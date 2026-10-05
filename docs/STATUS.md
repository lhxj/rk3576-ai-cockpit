# 2026-10-05 manual-v4：应用301.301秒通过，解码器接收UDP溢出，默认已恢复（当前）

本次真实T5 Qt观察后用户明确确认“已看到变化，此前固定静止”，记UI_SENSOR_PASS/USER_CONFIRMED；原应用日志human_confirmation=USER_CONFIRMATION_PENDING保持原文，人工确认独立映射。T6全组件实际301301ms、APPLICATION_EXIT0与APPLICATION_PROBE_PASS；RTSP解码9362帧但1245B真实H264错误，controller FAIL，最终集成未PASS。

v5父观察时间线证明decoder owned RTP socket inode41186 drops累计371，RTCP inode41187 drops0；global UDP InErrors/RcvbufErrors baseline0→371。十批解码错误父观察时刻191.088..420.005均在运行阶段、早于shutdown439.902及clientstop440.158，与随后有界UDP快照drop增长对应，不能归因仅启动或关停。接收socket溢出已证明，具体每AU/RTP序号映射未采集；sender无节奏连续sendto是候选机制，不宣称packetizer字节损坏或修复已实板通过。证据t5t6-v5-board-logs五文件及t5t6-v5-udp-causal-analysis.json。

PA实际IDLE/无USER→SUSPENDED/USER→结束SUSPENDED/IDLE/无USER，恢复成功，entry原失败保留。主控fresh post-health PONG620→623、error0，正常rmmod/node/actors/fuser/8554释放PASS；正常shutdown UART697.547536 Power down、collector/锁exit0。用户默认冷上电后实际uname6.1.99-rk3576且无两KO/node确认PASS（t5t6-v5-default-identity.log）；本轮未重复35项保护hash。

L0待实板验证修复：单active UDP client sender逐包20Mbps pacing，高于原codec8..8.5Mbps预算；按完整RTP datagram含12B header计算，默认1212B包间隔484800ns。steady_clock实际发送完成锚定下一时刻，不做延迟追赶burst；只worker在正常运行修改时刻，start安全复位，PLAY不复位。采集/录像submit仍仅入原有界队列1024；stop拒绝新enqueue并保留受限drain，不改变FU-A/codec/接收buffer/ffmpeg/zero-error门。默认满队列理论发送间隔预算约0.497秒，不含scheduler/transport开销；大payload配置按字节更久，不外推默认上限。Host完整35 CTest/176 Python/5 withdrawal PASS（rtp-pacing-host-ci-final.log）；旧未pacing发送实现配新无delaytest实际rc1在包间隔断言失败（rtp-pacing-old-negative/result.json），主控独立ASan/UBSan media_rtsp PASS（rtp-pacing-root-sanitizer.log）。独立app-v5原生builder已准备；当前尚无新native ELF/hash或实板修复PASS。

---

# 2026-10-05 v5：Qt前真实PulseAudio占用守卫停止（历史）

manual-run-v3本次在Qt/coexistence启动前被原fuser守卫拒绝：rc=0、stdout PID2201、stderr `/dev/snd/pcmC0D0c: m`。主控实际/proc核实PID2201为uid1000的pulseaudio、session pulseaudio.service；这是有证据的真实ALSA占用，稍后FREE不能倒推入口占用不存在。v5 Qt/共存/UDP诊断本轮NOT_RUN，无自动重试或窗口延长。证据Windows manual-run-v3/logs-20261005T133003317Z及t5t6-v5-guard-occupant.log。主控已正常rmmod两KO rc=0/node gone；health DONE693/693、elapsed912246ms、window720000ms，不能记录fresh post-PONG。正常shutdown UART970.584240 Power down（Windows v5-guard-shutdown/linux-com5.log），collector8307 CANCEL=0/串口关闭、共享锁28562释放=0；当时等待用户默认冷恢复；本轮随后实际恢复与仲裁检查见下文。

下一步仅评估可逆PulseAudio设备仲裁：普通cat会话下pasuspender持有suspend并运行原sudo入口，全部identity/hash/SOURCE/health/fuser守卫保留；不杀PulseAudio、不永久禁用或修改配置。pasuspender连接失败仍可启动child，必须child内验证实际本地PA与目标source已SUSPENDED；正常child结束有显式resume，异常退出不能声称DBus断开自动恢复。主控仅确认/usr/bin/pasuspender与/usr/bin/pactl存在，版本与capture source属性尚未核实。需比较仅对alsa.card=0/alsa.device=0唯一capture source做pactl suspend-source并恢复原状态，与pasuspender暂停所有source/sink的范围；后者正常resume可能改变原先已suspend状态，不能默认全设备方案必要。默认恢复后先只读核版本/实际source/原状态，再审核最小仲裁。该候选评估阶段结束后已完成本轮默认内核仲裁检查，完整负载未执行；旧v4 RTSP实际解码错误根因仍待有界诊断。

用户默认冷上电后主控实际uname=6.1.99-rk3576确认恢复。PA16.1本地unixsocket与唯一alsa.card0/device0 capture source已只读核实，pacmd原因为IDLE、USERfalse；独立source-only包装已仅部署fresh用户目录并hash读回PASS。默认内核原生--arbitration-check-only rc=0，实际IDLE/USERfalse→IDLE|USERtrue→IDLE/USERfalse，structured sink signature一致；证据pa-arbitration-wrapper-stage-readback.log及pa-arbitration-default-native-check.log。未启动传感器/媒体/KO/boot测试。独立manual-run-v4已准备，12项Host生产函数测试与发布manual-v4真实PS5 HostCheckOnly PASS；commit4524532全Host CI exit0，35 CTest/176 Python/5 withdrawal PASS（pa-arbitration-host-ci.log），不是完整实板负载PASS；完整Qt/五分钟/v5 UDP诊断仍NOT_RUN，原v5入口/全部守卫与collector不变。


---

# 2026-10-05 MPU6050 v4：真实五分钟应用通过，RTSP解码拒绝，默认已恢复（历史）

本次应用真实并行运行 300072 ms，并输出 `SYSTEM_COEXISTENCE_APPLICATION_PROBE_PASS`；端到端控制器仍为 **FAIL**，不能记为最终 T6 PASS。UDP loopback RTSP 客户端实际解码 9329 帧，但 955 字节 stderr 含 CABAC qscale、intra block unavailable 与 MB/bytestream 解码错误。原错误日志没有时间戳，不能归因于启动加入或结束尾包；具体RTSP/UDP或未录制startup AU根因尚未隔离。录制 ffprobe 返回 0、1632×1224、30 fps，只证明流信息可读，默认内核下完整录制软件解码已完成：rc=0、9348帧、progress=end、84.363865497秒、stderr=0；最终关闭文件315190329 bytes。最后循环快照9344 packets/315088953 bytes与关闭后结果属不同时点，不是计数不匹配。录制启动晚于RTSP，clean recording不能排除未录制startup AU；调查收窄至RTSP/UDP路径或未录制startup AU。

应用六个 STAGE 均 PASS。最后应用指标：capture 9558、29.8661 fps、sequence gap/dqbuf/qbuf error=0；record packets 9344、315088953 bytes、overflow=0；encoder instances=1、frames=9404、error/overflow=0；RTP packets=267544、sender drop=0；vision frames=2367、7.46676 fps、queue drop=0、peak=1；audio frames=4834240、xrun/overflow=0、peak=1。UDP sender drop=0 不证明接收端无丢包。58 行 sensor metrics 的 age 最大 10 ms，末 seq=8250/pubseq=8236、lease active=1，sample/protocol/send/control/overwrite/gap/duplicate 错误计数为 0。真实 raw M0 summary 为 attempt/seq=8201、valid=1、error=0、interval_max=60 ms；保留原日志，不把该 summary 当作完整逐条相关性审计。

Qt 最后 converted/delivered=2166、fps=6.76441；SENSOR_GUI ui_merges=12349。上一窗口方向变化有用户明确确认；本次用户也明确确认页面数值随方向变化，记 USER_CONFIRMED；两次观察分别记录，不输出校准 PASS。

root 已正常卸载两 KO（rc=0）且 node gone，actors/fuser/8554 均空。释放时 health 已 DONE，690/690、errors=0；elapsed≈960 s 已超 Linux 720 s 窗口，不能称 fresh post-process PONG 验证 PASS。正常关机 UART `1045.203724 Power down`，collector CANCEL=0；用户已默认冷上电，主控实际 uname 确认 6.1.99-rk3576 默认启动 PASS；独立只读录制解码锁70686已释放exit0，默认无两KO/node，无UART或活动RTOS测试。本轮遵用户要求未重复保护 35 项 hash，不能写本轮 35 PASS。

证据：`t5t6-v4-board-logs/{application.log,rtsp-progress.log,rtsp-decoder.log,resources.jsonl}`、`t5t6-v4-manual-logs`、`t5t6-v4-normal-release.log`、`t5t6-v4-recording-ffprobe.log`、`t5t6-v4-kernel-after.log`；精确指标与源日志 SHA 已另存 `t5t6-v4-evidence-analysis.json`。未更改业务代码、控制器或冻结产物；最终文件SHA256=9b53900ffe8197a0505a30173aaf652e30ae5b9f827994cee299adbaf9503b30，FFmpeg5.1.8-0+deb12u1；完整解码证据t5t6-v4-recording-complete-decode.jsonlog及t5t6-v4-recording-identity-and-default-release.jsonlog；下一步仅独立v5控制器有界时间/UDP诊断，保持原零错误门，不推定业务根因。

独立v5诊断控制器已审核并仅部署fresh用户目录，三脚本size/SHA读回PASS（t5t6-v5-controller-stage-readback.log）；manual-run-v3发布及真实PS5 HostCheckOnly、25项production Host tests PASS，诊断实测NOT_RUN。应用仍冻结native v4；保持原UDP/解码零错误/全部窗口门，仅新增有界父观察timestamp/phase/global UDP与owned socket inode drops，RTP序号证据明确unavailable；清单scripts/board/mpu6050/manual-run-v3/ASSETS.json。


---

# 2026-10-05 MPU6050真实Qt通过，T6同载失败，默认启动已确认（历史）

**UI_SENSOR_PASS / USER_CONFIRMED；T6_FAILED；最终MPU6050_RTOS_RPMSG_INTEGRATION_PASS未通过。** 用户手动runner首调用SOURCE/入口健康/ABI均通过，首次fuser守卫停止，未启动Qt；原rc/stdout/stderr未导出，具体原因UNKNOWN，后续FREE不能倒推首次。主控在同获批窗口持锁43624，确认实际FREE后只重跑原loaded一次（SOURCE age185.826秒，health入口<=200秒，不重载KO/不重启/不改冻结脚本）。

T5真实90秒Qt观察及正常退出：90组1Hz记录，seq/pubseq20..1804、age0..11ms、gaps/sample_errors/protocol_errors0、UI merges40..2813；退出matching UNSUB确认。固定静止[首次VALID+10,+25)秒15组，加速度模长0.923374..0.968406g/均值0.936823g，gyro均值(-6.34249,0.73028,0.14453)°/s，预设功能容差通过。用户明确确认此前模块固定静止、页面正常且数值随方向变化；这是USER_CONFIRMED功能观察，不是精度/零偏校准PASS，不减偏置。

T6实际Qt/Core、CAM0预览、RKNN、单MPP RTSP及录像、真实ALSA/VAD/Sherpa启动门先后通过（synthetic_FINAL=0不等于真实FINAL=0）。紧随T7进入全组件循环，四consumer active的合取守卫失败：`coexistence consumer stopped`。未满300秒，无APPLICATION_METRICS/SENSOR_COEX_METRICS/COEXISTENCE_DURATION或application success；Qt preview149 converted/delivered、fps6.8194、sensor UI merges848及RTSP decoder421帧仅为短时观察，不能作为共存PASS。实际四bool/错误详情未导出，具体consumer及根因UNKNOWN。资源4组间隔约5秒，仅启动短窗证据，不外推稳定性。主控第二次Qt/T6期间原manual collector已CANCEL，无同期M0 raw UART，因此本次不声称raw逐字段关联；历史T1–T4 raw100关联仍保留原范围。

主控随后实际health353/353、elapsed367706ms、READY、timeout/error0，正常rmmod两KO及node释放exit0，fuser空/8554空/项目actor空；kernel-after已保留。纯shutdown collector观察UART435.001991 Power down，CANCEL exit0。用户默认冷上电后主控uname确认6.1.99-rk3576，锁43624释放exit0、串口关闭。遵用户要求本次未重复保护hash，仅默认启动identity确认，不能称新35项恢复核验。证据t5t6-manual1-loaded-second.log、t5t6-manual1-board-logs/{application.log,resources.jsonl,rtsp-progress.log,rtsp-decoder.log}、t5t6-manual1-normal-release.log及主控kernel-after/恢复记录。

下一步仅诊断增量：失败时有界输出四consumer快照、媒体计数/last_error hex及stop请求计数，另记canonical四状态/真实Voice FINAL与intent计数；保留原四项active/错误/300秒门，不改资源或业务。Host源码审查未发现sensor canonical更新覆盖media或UI状态回调自动stop路径；当前尚不能声明根因已修。新source-v4与独立app-v4 native builder已准备，旧archive/v3 ELF/collector/FIT/KO/DT保持冻结；主控在默认6.1.99完成独立app-v4 AArch64原生构建exit0、295成员/source一致及四ELF readback/hash/ldd全resolved PASS（t5t6-diagnostic-native/application-build.json及t5t6-diagnostic-native-readback.log）；没有运行应用或改变boot/KO。原生构建锁16069已释放。新v4独立controllers已由主控仅部署fresh用户目录并核3脚本size/hash读回PASS（t5t6-v4-controller-stage-readback.log），无boot/KO/runtime操作；manual-run-v2已准备，20项production controller测试及PS5检查通过，诊断实测仍NOT_RUN；清单DIAGNOSTIC_V4_BUILD.json。Host CI35/119/5 PASS（t5t6-consumer-diagnostic-host-ci-final.log），不是板端诊断PASS。主控v4测试前默认预检确认无两KO/node/应用/8554占用，随后正常shutdown（SSH255、UART1266.208720 Power down）；日志Windows Documents/MPU6050-Test-20261005-v1/v4-pretest-shutdown/linux-com5.log，collector22805 CANCEL exit0、锁76035释放exit0，串口已关闭。当前板已正常关机，manual-run-v2准备就绪但NOT_RUN，等待用户手动命令与冷上电；未声明共存问题已修复。

---

# 2026-10-05 MPU6050 T5/T6 attempt6：SOURCE起点超时，默认启动已确认（历史）

冻结collector-v2未修改，也未创建v3；attempt3/4在UBoot等待人工确认期间120秒startup窗口到期，均未LOAD/SOURCE/新M0。用户明确要求保留原v2并重测。attempt6实际一次SOURCE成功（2026-10-05T08:42:37.0239293Z），readonly preflight PASS，主控正常加载原health KO（PONG14/14、elapsed14505ms、timeout/error0、window720000ms）及原sensor KO。

loaded入口因传入SOURCE age>390秒触发守卫而fail-closed；入口精确age未记录。失败后主控追加诊断，于2026-10-05T09:01:09.4982628Z测得距SOURCE 1112.4766秒，该值不是入口精确age。Qt、WHO及T5/T6业务未执行，本次无新MPU业务读取；不是错误风暴或MPU硬件失败。collector95656按DIAGNOSTIC900秒正常到期，source_once=True，COM5=83197B/M0=105948B；退出前末health PONG693、timeout/error0，但phase=DONE/elapsed1132408ms/last_pong_age412459ms，不能称运行时health仍READY。

主控正常卸载两KO并确认节点释放exit0；随后纯被动collector37207记录正常shutdown及UART1291.859648 Power down，CANCEL exit0释放端口。用户随后默认冷上电；主控首次SSH No route，等待10秒后第二次SSH成功，实际uname -r确认默认6.1.99-rk3576已启动。按用户要求未重复保护hash，本次仅默认启动identity确认，不能记录35项新核验完成。主控释放marker已写、锁会话75137 exit0，全部串口已关闭。历史HOST_PASS、RTOS_SENSOR_PASS、RPMSG_SENSOR_PASS保留；UI_SENSOR_PASS、五分钟共存及最终集成仍NOT_RUN。证据artifacts/local/mpu-root-review/t5t6-attempt6-{preflight,runtime,normal-release,final-shutdown}.log及Windows ready-attempt6/attempt6-shutdown。

下一窗口仅提出主控审核的有界单入口：在SOURCE前准备好用户目录编排与日志，保持原collector-v2/identity/人工LOAD→INSPECT→SOURCE门；SOURCE后一次入口读取实际SOURCE时间、核age与冷启动身份、执行冻结readonly preflight、确认无旧模块、正常加载原health KO并有界等HELLO_ACK/PONG>=3，再正常加载原sensor KO并立即调用同SHA loaded脚本。入口需在每阶段重新核剩余时间、异常停止且保留引用供主控正常退出，不能重试/自动SOURCE/延长Linux720000ms或M0900000ms。loaded仍SOURCE age<=390、health elapsed<=200000、总480秒；Qt静止窗后方向观察需用户实际确认。此项是计划，尚未实现/执行。

---

# 2026-10-05 MPU6050 T5/T6首窗口guard停止，默认冷恢复35检查PASS（历史）

HOST_PASS、接线USER_CONFIRMED、历史RTOS_SENSOR_PASS/RPMSG_SENSOR_PASS保留；UI_SENSOR_PASS、五分钟全负载、MPU6050_RTOS_RPMSG_INTEGRATION_PASS仍NOT_RUN。用户刚重新固定模块；上一轮静止仍未确认，不减零偏。主控L1默认before25保护文件及已部署v3旧十文件hash/metadata核验PASS，当前默认6.1.99/rootp3boot2，无项目/RPMsg占用。默认同样有ES8323 I2C3启动-6，保留known issue；T6必须监视同窗新增错误，不能据枚举称音频PASS。

四个AArch64 ELF在独立mpu-sensor-app-v3实际构建/ldd/hash PASS（仅编译未运行）；M0诊断缩短至BSP128字节预算，native-v8/ELF预算/FIT验签 PASS。新exact11文件coexistence package-v2已由主控被动安装至独立mpu-sensor-coexistence-v1，11hash读回PASS（t5t6-passive-install.log）。主控双UART_READY后正常shutdown，SSH255预期断连且UART Power down；该准备历史随后进入真实cold/SOURCE并加载原exact两KO；original runner在Qt/WHO之前因实板psmisc不接受fuser --而fail-closed约4秒退出。兼容无--实测FREE；独立loaded修正第一版未执行，预算不足未扩大。主控health407→409无timeout/error后正常rmmod/node释放、shutdown Power down、CANCEL关闭，用户已冷上电恢复默认Debian；主控生产validator确认25默认/冻结/SI及旧v3十文件共35项hash/metadata PASS，锁与双UART已关闭。未Qt/WHO/T6，不算硬件MPU失败，该guard停止与关机历史时刻不能称默认仍在运行；随后已冷恢复默认。v1预算被否决保留。新collector300/120/900、总1320秒、每UART256KiB，下一窗口仅用户目录loaded controller480秒；Linux health720000ms、M0 900000ms。旧冻结630秒runner只作历史且禁止重跑。最新Host CI34/111/5 PASS，主控actual sampler八case sanitizer PASS；新collector actual.NET共享读取及factory八mode PASS。主控独立核验v2 exact11/hash、四ARM清单/native-v8清单一致并审核通过；尚未操作本窗口业务。最新retry只读before25及旧十文件PASS；root新窗口持锁78175，仅部署新用户目录loaded脚本并核SHA、冻结包hash读回PASS，未启动新固件/KO/应用（t5t6-retry-loaded-stage.log）。详见[正式T5/T6执行与恢复](bringup/mpu6050/FORMAL_T5_T6.md)、[精确配套清单](bringup/mpu6050/FORMAL_T5_T6_BUILD.json)。

---

# 2026-10-05 MPU6050 RTOS / RPMsg T1–T4与默认恢复（历史验收记录）

实现提交`af2c70decd785702ebf658d68203fb19d9a59126`，分支`agent/mpu6050-rtos-rpmsg`。
**HOST_PASS / 接线 USER_CONFIRMED / RTOS_SENSOR_PASS / RPMSG_SENSOR_PASS / 默认冷恢复PASS25**。
实际驱动、协议、Linux有界桥、VehicleCore typed canonical与Qt显示/退出完成Host闭环；
RTOS/RPMsg真实样本、100逐字段一致与两次匹配退订/正常卸载通过；UI_SENSOR_PASS及最终
MPU6050_RTOS_RPMSG_INTEGRATION_PASS仍**NOT_RUN**。

最新T1–T4 parser/恢复增量host_ci34/34 CTest、87/87 Python、5/5撤回通过：`artifacts/local/mpu-root-review/t1-t4-parser-host-ci.log`。
此前正式包34/84/5由主控独立复核exit0：`artifacts/local/mpu-root-review/host-ci-package-final.log`。
此前Linux/Core/Qt实现阶段34/79/5是历史Host记录，保留其证据。
最后deadline修正后主控独立重建实际source并验证：
`artifacts/local/mpu-root-review/deadline-runtime-test.log` 2/2 PASS（ASan/UBSan、LSan=1）；
`deadline-qt-test.log` 1/1 PASS（ASan/UBSan、Qt LSan=0，不能称Qt泄漏测试通过）。
配套KO-v4源码/hash、vermagic与固定导出符号核验通过；MODVERSIONS=n，CRC=N/A。

历史一次SSH超时保留；用户随后确认上电，主控持锁L1重新核验默认6.1.99/rootp3boot2/RPMsg空，正式before25默认/冻结/SI文件hash通过。AArch64应用v2实际编译通过（未运行），M0有限观测v7原生/FIT验签通过，exact10文件正式包v3已由主控被动安装到独立`/boot/amp-p029/mpu-sensor-v1`并全部hash读回PASS；manifest SHA`4bf4748465e184939bacd821e3c6c6491bce738eb35a90abafa901ad2a7991ec`，证据`artifacts/local/mpu-root-review/sensor-package-passive-install.log`。被动安装时尚未shutdown/新固件/KO/MPU访问，构建清单board_deployed=false保留原始事实。随后主控UART_READY后正常shutdown；FileShare.None阻止live读取，CANCEL正常退出，COM5记录Power down。该历史准备阶段板已正常关机、锁仍由主控持有，未LOAD/SOURCE/新M0/KO/MPU访问；随后外部collector-v2 FileShare.Read修正及实际.NET双句柄回归通过，主控完成下述真实窗口与默认恢复。详见[正式运行包](bringup/mpu6050/FORMAL_T1_T4.md)。详见[Host结果与限制](bringup/mpu6050/LINUX_CORE_QT_HOST_RESULT.md)
及[配套KO清单](bringup/mpu6050/LINUX_SENSOR_HOST_BUILD.json)。

最新实板结果：两client各100有效样本/退订确认/gaps0/protocol_errors0，health14/14无timeout/error，M0raw100与Linux100一致，采样间隔50..60ms（均值50.303ms）。配置尾日志截断已按唯一NUL+raw边界恢复完整raw，缺尾字段未重建；关机final另含NUL原样保留，不称全日志解析通过。主控随后默认冷恢复25bytes/hash/resolved/symlink PASS，全部板会话结束。ES8323启动-6错误待T6前核验；Qt人工/静止合理性/五分钟共存未跑。当前默认6.1.99/rootp3boot2已运行，锁/collector均释放，本轮到T1–T4停止。第二组实际间隔全50ms；accel模长均值0.93664g、gyro X均值-6.63878°/s，但固定静止未人工确认，不称校准/近零PASS。seq101/102在订阅间未发布、pubseq连续1..200，不计传输丢包。详见SENSOR_T1_T4_RESULT.json。

以下全部为各阶段历史记录；旧“当前/最新”正文描述仅指记录时刻，不作为本轮实时状态，
旧失败、未完成项与原始计数保留，后续结果不会改写其当时证据。

---

# 2026-10-05 MPU6050断电接线确认与driver Host里程碑（历史记录）

用户已明确回复“已断电接线完成”：接线门USER_CONFIRMED。driver与有界20Hz sampler的实际生产代码Fake/ASan/UBSan通过，独立MPU_SENSOR_V1 native SCons/ELF预算/FIT验签通过；health源未改。READY入口没有transport调用者，无自动初始化/采样，因此产物NOT_DEPLOYABLE。完整业务HOST_PASS及RTOS_SENSOR_PASS/RPMSG_SENSOR_PASS/UI_SENSOR_PASS仍NOT_RUN，未读取真实WHO或样本。Host CI31/74/5通过。当前只完成driver/sampler Host子里程碑，继续下一独立sensor service/codec/epoch/订阅Host工作，板端操作由主控统一。

见[driver结果](bringup/mpu6050/DRIVER_HOST_RESULT.md)及[原生清单](bringup/mpu6050/DRIVER_HOST_BUILD.json)。以下接线等待及历史缺口为当时事实，保留不改写。

---

# 2026-10-05 MPU6050接线前里程碑（历史记录）

**WIRING_READY_FOR_USER / USER_POWER_OFF_WIRING_CONFIRMATION_PENDING**。主控attempt4真实冷进入独立I2C_RESOURCE_PROBE_V1，Linux ownership/clock预检PASS，M0十地址白名单各读一次、BEGIN/END各1/status0、新增诊断writes0/I2C transactions0。实际health HELLO_ACK1/PING3/PONG3/timeout0/error0；正常卸载诊断KO，随后正常关机与用户冷恢复，默认6.1.99-rk3576/rootp3boot2/RPMsg空、25保护文件hash PASS，锁/所有会话已结束。

I2C9_M1接线：VCC→Pin2、GND→14、SCL→19(GPIO1_B5 mux10)、SDA→23(GPIO1_B4 mux10)，VCCIO3设计3.3V与用户实测上拉3.3V相容。Pin1丝印方向已确认，风扇4/6、串口GND20保持。当前SDA/SCL仍未接，等待用户断电接线确认，不继续诊断/重启。白名单读不证明写、I2C交易、IRQ或WHO；完整sensor HOST_PASS/RTOS_SENSOR_PASS/RPMSG_SENSOR_PASS/UI_SENSOR_PASS及最终等级均NOT_RUN。最新Host CI31/72/5，UART八fixture PASS。

见[接线](bringup/mpu6050/WIRING_OWNERSHIP.md)、[实际结果](bringup/mpu6050/RESOURCE_PROBE_BOARD_RESULT.json)、[板端记录](bringup/mpu6050/BOARD_RESULT.md)。以下保留历史状态，不改写旧失败/缺证据。

---

# 2026-10-05 MPU6050 / RTOS RPMsg 第一轮审查

分支`agent/mpu6050-rtos-rpmsg`从最新系统tip
`6e0aa7c83dd51de88e9f767dde09aadbba9336ea`派生，三个给定锚点均为祖先。
实时普通用户持锁SSH确认默认#8、无stage/RPMsg设备，未进入AMP或改变板端。
候选I2C9_M1（Pin19 SCL/23 SDA，VCCIO3设计3.3V），当前Linux禁用且无从设备/绑定；
CAM0/codec/RTC共用I2C3保留。用户最终更正确认PCB为EBF410513V2R0 20260521；
风扇保持Pin4/6、5V4A。模块VCC→2/GND→14，用户测得VCC约5V、SDA/SCL各3.3V、AD0=0V；Pin1方向/19/23空闲已确认，Pin20接串口调试器。
续审已核TRM PD_BUS归属、I2C9独立24MHz父时钟；发现早期自动probe、gate切换及
启用I2C会配置I2C7、timeout单位/初始化错误处理的BSP问题；四项已完成派生Host修复，
Linux clock/reset/pinctrl ownership与M0访问权限仍待闭合。
**WIRING_NOT_READY / OWNERSHIP_NOT_CLOSED**；不发接线就绪、不请求部署审批。
本轮主控审核及Host CI31/31 CTest、52/52 Python、5/5撤回通过；新传感器业务未实现，四阶段PASS及最终
`MPU6050_RTOS_RPMSG_INTEGRATION_PASS`均未取得。历史证据不改写。
见[计划](plans/mpu6050-rtos-rpmsg.md)、[接线/ownership](bringup/mpu6050/WIRING_OWNERSHIP.md)、
[结果与限制](bringup/mpu6050/BOARD_RESULT.md)。

---

# 历史系统集成状态（2026-10-04）

本节为两个权威 tip 的收敛状态；下方保留各自有日期的历史记录，不将后来的
结果改写成当时已经完成。系统集成仅合并已有成果，没有新增功能或改动冻结 AMP。

| 范围 | 状态 | 证据与边界 |
|---|---|---|
| Linux Application stack | PASS（既有分项基线） | `8585c66d27fa65ef11a6531b95656acfa3dc9e8b`；Host 31/31 CTest、6/6 Python，RK3576 35/35 CTest；不是全系统同载证据 |
| AMP/RPMsg minimal link | BOARD_PASS / FROZEN | `ec56833276d31df1e1ce8d36741a552042e2b6ca`；RT-Thread BUS M0 + paired Linux HELLO/HELLO_ACK、PING/PONG；测试后冷恢复默认 Debian |
| System source/Host convergence | SYSTEM_INTEGRATION_HOST_PASS | 最新31/31 CTest、47/47 Python（原41项保留并新增6项健康fixture测试）、AMP/shell/harness及sanitizer通过；见[构建结果](bringup/system-integration/BUILD_MATRIX.md) |
| Full-system coexistence | SYSTEM_INTEGRATION_BOARD_PASS（302.24秒有界共存） | 2026-10-04获批SI_HEALTH_V1：Qt/Core/CAM0/单MPP Recording+RTSP/RKNN/真实VoiceRuntime+M0重复echo；退出后PONG继续；默认冷恢复及18项hash不变。不是RTOS业务/长期稳定性PASS，见[本次结果](bringup/system-integration/BOARD_COEXISTENCE_RESULT.md) |
| MPU6050 over RPMsg | NOT_IMPLEMENTED | 未接入 RTOS 采样/业务 |
| VehicleCore ↔ RTOS business protocol | NOT_IMPLEMENTED | `apps/rpmsg_srv` 未提供真实业务适配；最小 Linux echo KO 不等于用户态服务 |
| RTOS sensor/control business | NOT_IMPLEMENTED | 最小 RT-Thread 链不代表完整业务子系统 |
| CAM1 integration | NOT_IMPLEMENTED | 历史单摄采流保留；当前项目并发/业务未接入 |
| TTS / RKLLM | NOT_IMPLEMENTED | 保留接口与历史未知项，未接入真实后端 |

当前事实与完整运行门见 [共存记录](bringup/system-integration/BOARD_COEXISTENCE_RESULT.md)。

---

# 历史状态：事实、约定和未知项（2026-10-02）

整理日期：2026-10-02。来源包括本次对话中用户提供的历史实板输出与确认；
不是本包生成过程对实体板的实时读取。历史日志日期可能受板端时钟影响。
新增记录注明时间、命令、实际输出、版本与证据位置。

## 1. 主机 / Git / SSH

| 项目 | 最后已知情况 | 证据等级 |
|---|---|---|
| WSL | Ubuntu 22.04.5 LTS，x86_64 | USER_LOG |
| WSL磁盘 | 当时df报告758G available；WSL虚拟磁盘视图不保证Windows宿主还有同等物理空间 | USER_LOG |
| 工作区 | `/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit` | USER_LOG |
| Git | 本轮开始为 `agent/reference-imx6ull-ui-strategy`、HEAD `75607d9`；已新建 `agent/voice-ai-foundation`，既存未跟踪审查材料保留 | HOST_OBSERVED_2026-10-01 |
| GitHub CLI | 用户lhxj已登录，Git走HTTPS | USER_LOG |
| GitHub仓库 | `origin` 配置为 `https://github.com/lhxj/rk3576-ai-cockpit.git`；远端可用性与推送状态未由本轮核验 | HOST_OBSERVED / UNVERIFIED |
| 板端SSH | WSL使用`ssh lubancat`，用户cat，公钥登录已由用户确认完成 | USER_CONFIRMED |
| USB-TTL | 已有 | USER_CONFIRMED |
| WLXray环境 | 曾在WSL激活其venv，不能用于本项目 | WORKFLOW_CONSTRAINT |

## 2. 开发板

- LubanCat-3 v2 标准单板，Rockchip RK3576。
- RAM约4GB，eMMC约32GB，依据用户lscpu/free/lsblk及板型记录。
- Debian GNU/Linux 12 Bookworm。
- Kernel `6.1.99-rk3576`，构建信息2026-04-24。
- 镜像 `lubancat-rk3576-debian12-gnome-20260424`。
- Device tree：`rk3576-lubancat-3-v2.dtb`。
- 初始CPU在线0-7；不能仅凭这条判断所有可能的片内RTOS方案。
- **历史** `/boot/uEnv/uEnv.txt -> uEnvLubanCat3-V2.txt`。
  之后曾建议使用`sed -i`，可能已将软链接替换为普通文件；第一轮必须重新`ls -l`和`readlink -f`。
  不擅自“修复”链接；先核对实际启动配置读取路径。

## 3. 已验证功能

| 功能 | 已有证据 | 尚未覆盖 |
|---|---|---|
| RTL8822CE | PCIe识别，驱动rtw_8822ce，Wi-Fi联网，SSH/SCP | 长期吞吐/恢复/RTSP质量 |
| 音频 | `hw:0,0`录5秒16kHz/mono/S16_LE，耳机可回放 | 全双工、长稳、ASR/TTS集成 |
| 音频命名 | ALSA卡名rockchip-es8388，日志出现ES8323 | 物理Codec型号不要只由卡名推断 |
| CAM0单摄 | OV8858，1632×1224 NV12，300帧约29.87~29.88fps，用户确认JPG颜色正常 | 双摄并发、长稳、编码/NPU同载 |
| CAM1单摄 | 更换到已知正常排线后完成30帧并正常停流 | 长稳与当前线缆状态 |
| Camera B | Camera B + Cable A采流正常 | 不据短测声称所有光学/AF能力通过 |
| USB触摸 | WaveShare WS170120，0eef:0005，hid-multitouch，历史event6 | event编号不是固定映射 |
| HDMI/触摸交互 | 用户最新确认正常显示且可以触控 | 真实mode、刷新率、X11/Wayland、Qt平台插件未盘点 |
| MPU6050 | 用户已有 | 未接入/未采样/未分配RTOS I²C资源 |

LED / 按键 / 蜂鸣器：用户明确决定软件模拟，不购买为前提，不宣称GPIO实测。

## 4. 摄像头当前约定与已知问题

- 当前软件开发约定：Camera A + 已知正常线缆，CAM0启用，CAM1禁用。
- CAM1替换BTB排线需要等待；“至少两天”是用户当时估计，不自动认定某日已到。
- Camera B使用Cable A可正常采流，问题**高度集中于第二套连接组件**。
  尚未把故障完全定位到哪段FPC、BTB接头或永久损伤，不能无证据写成烧毁。
- 双摄曾完成两路probe及节点创建，但CAM1曾ERR2；**双摄同时稳定出帧未通过**。
- 物理CAM0历史sensor `3-0036` → DPHY0 → rkisp-vir0。
- 物理CAM1历史sensor `4-0036` → DPHY1 → rkisp-vir1。
- 单摄mainpath曾为video11；双摄时曾为video22/video31。
- `/dev/video-camera0`在双摄时曾指向video31，不能作为前摄的永久别名。
- 当前每路真实节点须运行时核验；配置文件样例默认不绑定节点。
- 当前格式曾为MPLANE API、NV12、num_planes=1、stride=1632、sizeimage=2996352。

## 5. 尚需回归的警告

- Camera启用后日志出现ES8323寄存器写入-6，之后没有完整的音频回归证据；需受控回归。
- 单摄有`vblank need >=1000us ... cur 693us`；短测通过不等于长期/双摄无风险。
- 异常线缆采流出现`MIPI_CSI2 ERR2:0x10000000`；本包不硬解释未知位定义。
- 初期有regulatory.db缺失、DPK calibration警告；Wi-Fi实测联网，但无线性能/法规配置仍待核对。
- 曾有journal非干净关机提示；不可用等待固定秒数代替确认正常关机完成。

## 6. AMP / RPMsg：选定路线，尚未实现

历史运行kernel配置：

```text
CONFIG_MAILBOX=y
CONFIG_ROCKCHIP_MBOX=y
CONFIG_RPMSG=y
CONFIG_RPMSG_NS=y
CONFIG_RPMSG_ROCKCHIP_MBOX=y
CONFIG_RPMSG_VIRTIO=y
# CONFIG_REMOTEPROC is not set
# CONFIG_RPMSG_TTY is not set
# CONFIG_RPMSG_CHAR is not set
# CONFIG_RPMSG_CTRL is not set
```

已选择Linux + RT-Thread片内AMP + RPMsg；未确认RTOS固件、启动链、核号、
保留内存/vring、通知方式、Linux用户态接口、真实SDK是否齐全。
不保证有`/sys/class/remoteproc`、`/dev/ttyRPMSG0`或`/dev/rpmsg*`。
MPU6050资源分配必须等SDK/板级资源审查，不能抢走Camera/Audio所在I²C。

## 7. 软件状态

原启动包仅提供规则、任务、README、配置样例、只读脚本和host构建烟测。
2026-10-01 新增 Voice/AI Host 接口骨架：`libs/protocol`、`libs/ipc`、
`audio_srv`、`voice_srv`、`infer_srv` 的接口、内存 Mock 和生命周期测试。
这只是 `HOST_TESTED_INTERFACE_ONLY`：没有 ASR/TTS/RKLLM/RKNN、真实 ALSA、
ZeroMQ 或业务服务进程。Qt、media_srv、rpmsg_srv、RTOS业务仍未由该轮实现或
验证；vehicle_core 的后续 Host foundation 见第15节。厂商例程、完整SDK、模型和运行库仍需逐项获取/核验。
所有板端PASS指此前用户的具体测试，不代表当前Codex可以跳过盘点。

## 8. 2026-10-01 IMX6ULL archive audit update

- 本地已取得并静态审查 `build-QTMenu-IMX6U_rsync-Debug.rar`，SHA256 为
  `92e571eaeb171be6dcd73c2db8e895223f2fdabd3ee863b8dc66de9f5e39462c`。
- 归档是 Qt Creator/qmake `SHADOW_BUILD`，原始 Qt 工程源码 `INCOMPLETE`，
  包内主要是 ARM32 ELF/object、Qt 生成文件、旧 Makefile 和演示媒体。
- 正式定位：`Reference Role = UI_REFERENCE_ONLY`，
  `Migration Strategy = REIMPLEMENT`，`License = LICENSE_UNVERIFIED`。
- 该盘点只增加参考证据，不改变当前软件实现状态：`cockpit_ui` 仍为
  `NOT_IMPLEMENTED / CONTRACT_ONLY`，音乐、视频、传感器参考功能均未成为
  RK3576 应用功能。
- 仓库记录见 `docs/reviews/reference-audit/imx6ull-qt/`。更早的架构审查在当时
  记录“RAR尚未取得”仍是有效历史事实，不回写或覆盖。

## 9. 2026-10-01 Voice/AI foundation update

- LLM_Voice_Flow 固定审查 commit `be82e87cc334ae6e222f83f7555531d1ddebaa8b`；
  当前参考定位 `SOURCE_REFERENCE / REFERENCE_ONLY`，产品决策
  `BUILD_OWN_VOICE_AI_STACK`。根 LICENSE 未核，模型资产 `PARTIAL`；见
  `docs/REFERENCES.md` 与 `docs/reviews/reference-audit/llm-voice-flow/`。
- 本轮 Host 验证命令：`bash scripts/dev/host_ci.sh`；CMake Debug build、
  CTest 7/7、Python unittest 6/6、shell语法检查通过（exit 0）。
- VOICE-00/01 为 Host foundation，VOICE-02/03/04 为 Host 接口/Mock；
  F14-F20 实际产品功能状态不因此改变。未连接开发板或占用 ALSA 设备。

## 10. 2026-10-01 ASR-01 file backend update

- 开发分支 `agent/asr-sherpa-file-backend` 基于 `42c3c11`。命令
  `bash scripts/dev/host_ci.sh`：CTest 9/9、Python unittest 6/6，exit 0。
- 本地官方 Sherpa v1.11.3 x86_64 C API 库放在忽略的 `build/asr-deps/`，
  `cmake -DCOCKPIT_ENABLE_SHERPA_ASR=ON` configure/build 成功；
  `ctest --test-dir build/asr-sherpa-host -L sherpa-integration`：2/2 PASS，
  含旧session取消和新session真实识别。
- 参考仓外部 `test_wavs/0.wav` 经 `read_pcm_wav → SherpaAsrBackend →
  VoiceSessionController` 得到 `ASR_FINAL 昨天是 MONDAY TODAY IS THEY AFTER TOMORROW是星期三`。
  同一进程连续处理 `0.wav`、`1.wav` 仅输出一次 `MODEL_LOADED`。
- 级别 `ASR_FILE_RECOGNITION_PASS` 限定 Host 文件输入。没有访问开发板、ALSA、
  麦克风或外设；模型及测试音频未进入Git。模型发行许可、实时取消延迟、
  RK3576资源/性能和车控闭环未验证；见 `docs/architecture/ASR_BACKEND.md`。

## 11. 2026-10-01 ASR target file validation

- 独立分支 `agent/asr-rk3576-file-validation` 基于 `052a60e`。本节是新增实板证据，
  不改变上节当时仅完成 x86 Host 验证的历史事实。
- `ASR Board File Recognition: PASS`，**仅文件输入**。板端 Debian 12 / AArch64 原生构建
  主项目 `cockpit_asr_file_test`，官方 Sherpa-ONNX v1.11.3 CPU AArch64 运行库 +
  ONNX Runtime 1.17.1 均在 `/home/cat/cockpit/asr-target`。ELF启动和动态库解析通过。
- Host/Board 五个模型资产及 `test_wavs/0.wav` SHA256 逐项一致；板端经
  `SherpaAsrBackend → VoiceSessionController` 输出
  `ASR_FINAL 昨天是 MONDAY TODAY IS THEY AFTER TOMORROW是星期三`，与 Host 文本相同。
  板端模型加载约 7.62 秒，单次 decode 1.739 秒、RTF 0.173。
- 板端真实引擎 cancel、新 session、同进程三次识别和三类错误输入均正常退出；
  `bash scripts/dev/host_ci.sh` 9/9 CTest、6/6 Python，以及 x86 Sherpa integration 2/2 通过。
  资源采样、ABI、哈希及测试日志位置见 `docs/bringup/asr/`。
- **未验证**实时麦克风、ALSA采集、VAD/wake、组合视觉负载或模型/WAV发行许可；
  不能将本项写为实时 ASR 或完整语音助手 PASS。

## 12. 2026-10-01 RK3576 live microphone ASR

- 独立分支 `agent/asr-rk3576-live-mic` 基于 `2b151027`。本节增加实时麦克风证据，不回写第十一节文件 ASR 当时的边界。
- **`ASR_BOARD_LIVE_MIC_PASS`，仅麦克风 ASR。** `audio_srv` 的 `AlsaAudioCapture` 打开 `hw:0,0`，实际协商 16 kHz / mono / S16_LE、320-frame period、1280-frame buffer。PCM 经 100-chunk 有界队列进入 `voice_srv` 的 `LiveAsrPipeline → SherpaAsrBackend → VoiceSessionController`。T1 2.014 秒采到 32000 帧。
- T2 用户反复说“打开摄像头”，得到 6 次 partial 与 `ASR_FINAL 摄像头打开摄像头`。T3 约2秒取消后旧会话无FINAL；捕获停止13.06 ms、完整停止13.88 ms。T4取消后新会话识别用户反复说的“开始录像”，输出 `开始录像开始录像`。T5 同一进程模型只加载一次，三次会话各得到非空FINAL。全程记录XRUN 0、PCM队列溢出0，程序自然退出且无残留进程。
- T5模型加载7984 ms，峰值RSS/PSS约195332/192289 kB，最低MemAvailable约2890092 kB，峰值CPU约133%、最高热区约52.692°C。它是短测，不证明长期内存稳定或准确率。详细T0–T5、空语音诊断和测试日志位置见 `docs/bringup/asr-live/`。
- 板端文件 ASR 全套回归仍PASS；默认Host CI为10/10 CTest、6/6 Python，x86 Sherpa integration为2/2。模型与测试WAV未入Git，发行许可仍为`LICENSE_UNVERIFIED_FOR_DISTRIBUTION`。
- **未实现** VAD、wake word、intent/车控、播放、TTS、LLM、真实语音助手，也未做长稳或正式命令准确率认证。

## 13. 2026-10-01 RK3576 VAD automatic utterance segmentation

- 独立分支 `agent/asr-rk3576-vad` 从 `f2ddb76` 建立，保留上一节当时未实现VAD的历史事实。当前等级 **`VAD_FILE_PIPELINE_PASS`**：Host与RK3576同一主项目代码、固定Sherpa-ONNX v1.11.3、固定Silero v5.0外部模型，预录音WAV经VAD自动切分并通过 `SherpaAsrBackend → VoiceSessionController` 输出FINAL。单句、双句板端fixture通过；200句板端重复测试有200次start/end/FINAL，模型各加载一次，peak pre-roll 4800帧、sequence gap 0，程序自然退出。
- `audio_srv` ALSA `hw:0,0` 启停与实际16kHz/mono/S16_LE协商在此分支通过。5秒有界实时VAD运行接收80,000帧，queue peak 4/100、overflow 0、XRUN 0；出现speech start和partial，但安全上限前未见speech end/FINAL。不能据此宣布 `VAD_LIVE_PIPELINE_PASS`。没有要求用户新增真人语音准确率测试，也未保存新录音。
- 默认Host CI 11/11 CTest、6/6 Python；独立x86 Sherpa integration 4/4；VAD单元ASan/UBSan通过。RK3576最终文件/固定Live回归4/4，另有双句VAD fixture PASS；20秒真实麦克风静音链无误触发、XRUN或队列溢出，但没有控制语句供实时speech end/FINAL验证。ASR模型/测试WAV发行状态仍 `LICENSE_UNVERIFIED_FOR_DISTRIBUTION`；VAD v5.0模型在相同tag观察到MIT许可，产品打包通知待落实。模型/测试WAV只在忽略的本地构建目录和板端用户目录。详细证据和仍需验证的实时边界见 `docs/bringup/vad/`。
- VAD只检测语音边界并输出ASR文本；wake word、intent、vehicle_core命令、TTS、RKLLM、RKNN均未连接。

## 14. 2026-10-02 Deterministic Intent Router (Host only)

- 独立分支 `agent/deterministic-intent-router` 从 VAD commit `b6f8e9a` 建立。新增 `ASR_FINAL` 文本规范化、9条显式规则/20个alias、冲突检查、否定与多命令拒绝、最多3次相同完整短语折叠，以及typed `CandidateAction` 到记录型 `IVehicleCommandSink`。没有把路由器接入VAD回调或真实 `vehicle_core`。
- `bash scripts/dev/host_ci.sh`：13/13 CTest、6/6 Python通过；`ctest --test-dir build/intent-sanitizer -R '^intent_router_test$'` 在ASan/UBSan下1/1通过；外部固定Sherpa v1.11.3/VAD模型的 `ctest --test-dir build/intent-sherpa-integration -L sherpa-integration` 4/4通过。默认Host CI仍不需要ALSA/Sherpa。
- 白名单只生成候选，不证明任何摄像头、录像或模拟外设已执行。只读核对独立 `vehicle_core` 分支后，摄像头关闭语义暂无对应命令，因此“关闭摄像头”返回 `NO_MATCH`。真实Vehicle Core适配、重复FINAL幂等与VAD FINAL回调后的无重入交接仍待下一独立集成阶段。没有访问开发板或采集语音。

## 15. 2026-10-01 Vehicle Core foundation update

- 分支 `agent/vehicle-core-foundation` 从固定基线 `42c3c11` 建立；未合并 UI、ASR
  或 AMP 分支，未连接开发板。
- `apps/vehicle_core` 已实现结构化 command validation、ACK/RESULT、Mock domain
  routing、canonical state/revision、service registry、有限去重缓存、deadline 与
  late-result 栅栏、Voice CandidateAction bridge 及 Host loopback client。
- `bash scripts/dev/host_ci.sh` 退出0：CTest 11/11、Python unittest 6/6。
  四个 Vehicle Core 测试各连续运行20轮通过；ASan+UBSan 4/4通过。
- TSan 构建成功，但 WSL 运行时在测试启动前报
  `FATAL: ThreadSanitizer: unexpected memory mapping`；因此状态是
  `BLOCKED_BY_RUNTIME`，不是测试通过或发现代码数据竞争。
- 本轮等级仅为 `VEHICLE_CORE_HOST_PASS`。Camera、Recording、Voice、RTOS、Sensor、
  Qt integration 和任何实板业务状态均未因此提升。

## 16. 2026-10-01 cockpit_ui foundation update

- UI成果来自独立分支 `agent/cockpit-ui-foundation`（`58f69e5`），现已通过普通
  Git merge引入integration分支；Voice/Vehicle Core基础模块均保留。
- UI为Qt 5.15 Widgets单shell，包含Home、Camera、Media、Vehicle/Sensor、AI、
  Monitor、Settings、统一`UiState`和可保留的`MockUiBackend`。
- Host Qt构建和offscreen测试已有通过证据。LubanCat-3上的Qt 5.15.8、GNOME/X11、
  800x480全屏启动、页面导航和实体触摸由用户确认通过；运行需保持
  `QT_XCB_GL_INTEGRATION=none`，该workaround不证明GPU/GLX路径正常。
- 页面骨架与触摸PASS不等于Camera、Media、Voice、RTOS或Sensor真实业务PASS。
  本integration任务将在Host测试后重新生成本分支的AArch64与板端证据。

## 17. 2026-10-01 UI + Vehicle Core integration update

- 独立worktree `/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-ui-core` 和分支
  `agent/ui-vehicle-core-integration` 从Vehicle Core `8445677`建立，并以普通merge
  引入UI `58f69e5`；merge提交为`61b8507`，保留双方历史。
- `VehicleCoreUiBackend`通过`IVehicleCoreClient`发送命令，消费全量snapshot和严格递增
  revision；request id、boot epoch、session和deadline均不由Qt控件构造。ACK只产生
  pending，终态以RESULT和canonical state为准。
- Host最终`bash scripts/dev/host_ci.sh`退出0：CTest 16/16、Python 6/6、shell检查通过。
  纯C++ integration test连续50轮通过；独立ASan+UBSan 1/1通过。TSan没有新结论。
- 板端只部署到`/home/cat/cockpit/ui-core-integration-20261001-01/`。Qt 5.15.8 /
  GCC 12.2原生AArch64构建和target CTest 16/16通过，binary SHA256为
  `d5863b4fa763577bdf580d292148ca6046462eef192e1f5742ef9d19ca471632`。
- `normal`、`media-failure`、`media-timeout`、`rtos-offline`四个profile的有界X11
  windowed启动均退出0；`normal`全屏启动退出0；无残留进程。仍使用
  `QT_XCB_GL_INTEGRATION=none`，没有测试Mesa/GLX加速。
- 用户在800x480实体触摸屏完成integration二进制人工验证：`normal`下完整导航、
  Recording序列、Rear拒绝和SIMULATED控件正常；`rtos-offline`明确显示
  `UNAVAILABLE`；`media-timeout`最终显示`Error`且未误报Recording。
- 当前等级是`UI_VEHICLE_CORE_INTEGRATION_PASS`，板端子等级为
  `BOARD_TOUCH_TESTED_MOCK_INTEGRATION`。该结论只覆盖Qt→Vehicle Core→Mock adapter
  闭环，不提升Camera、Media、RTOS、Audio、Voice或AI真实服务状态。
- 本轮没有打开Camera/V4L2、ALSA、Sherpa、RKNN/RKLLM、RPMsg、RT-Thread、I2C、GPIO
  或MPU6050。所有Camera/Media/Voice/RTOS控制结果仅来自Mock adapter。

## 18. 2026-10-02 CAM0 Media/Core integration update

- 独立worktree `/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-media-cam0`、分支
  `agent/media-cam0-integration` 从`0d4c6a1`建立；ASR/VAD/AMP分支未合入。
- T0重新把OV8858 `3-0036`经`/dev/media0`、`/dev/media1`的rkisp mainpath解析到
  `/dev/video11`；当前alias `/dev/video-camera0`也解析到该节点，但产品CLI仍要求显式节点。
- 当前项目V4L2 backend在`cat`用户下实际协商1632x1224 NV12、MPLANE、一个memory
  plane、stride 1632、sizeimage 2996352及4个MMAP buffer。300目标测试实际完成301帧，
  10.041秒、29.8775 fps，sequence gap、poll timeout、DQBUF/QBUF error均为0。
- 20轮Start/Stop均完成，epoch 1至20严格增加，进程fd与thread计数前后相同。
  Snapshot生成1632x1224 P6 PPM并完成静态图像检查。
- T5真实链为`VehicleCore -> RealMediaServiceAdapter -> MediaService -> V4L2`；
  CAMERA_SELECT、PREVIEW_START、SNAPSHOT、PREVIEW_STOP均有独立ACK/RESULT，canonical
  Preview由RUNTIME结果更新。Recording/RTSP仍为明确未实现，Rear仍Unavailable。
- T6使用板端Qt 5.15.8、GNOME/X11和既有`QT_XCB_GL_INTEGRATION=none`运行20秒：
  capture 29.8767 fps、Qt delivered preview 13.0096 fps，正常退出并释放Camera。
- T7在HDMI-1实际800x480模式下由用户完成实体屏人工验收。用户确认实时画面、颜色/
  方向/比例/裁剪、触摸导航、Snapshot、Rear unavailable、Recording/RTSP unavailable及
  离开Camera后返回恢复预览全部通过；本次交互生成`cam0_e3_s281.ppm`。
- T8有界300秒运行完成8,960帧、29.8757 fps、Qt交付13.4174 fps，sequence gap、
  poll timeout、DQBUF/QBUF error、mailbox/UI coalescing drop均为0；退出后无残留进程/
  Camera owner，项目backend可重新打开设备。该结论不是长期稳定性或热认证。
- 原生AArch64 build通过，默认CTest 19/19通过。Host ASan+UBSan下MediaService与Core
  2/2（含LeakSanitizer）通过；Qt offscreen在关闭LSan时也通过，启用LSan只报告Qt
  offscreen/fontconfig退出时656字节框架分配，没有ASan/UBSan越界或UAF报告。
- 证据与边界见`docs/architecture/MEDIA_CAM0_PIPELINE.md`及
  `docs/bringup/media-cam0/`。T1至T8门均已关闭，当前等级为
  `MEDIA_CAM0_CORE_INTEGRATION_PASS`；这不提升Recording、RTSP或CAM1状态。
## 19. 2026-10-02 Voice Intent → Vehicle Core Host integration

- 分支 `agent/intent-vehicle-core-integration` 从 `97dbc29` 建立，普通 merge 保留
  `agent/vehicle-core-foundation` 的 `8445677` 历史。只使用 synthetic ASR FINAL 和
  MockMedia/MockRtos，未访问开发板、实时 VAD callback 或真实硬件服务。
- 有界 `VoiceIntentDispatcher` 将 FINAL 从 Controller 回调交给 joinable worker，
  有界去重缓存阻止重复 FINAL 再次提交。`VehicleCommandSinkAdapter` 严格映射 typed
  Camera Select、Recording Start/Stop 和模拟 LED/Buzzer；`OPEN_CAMERA` 因没有精确
  预览启动命令返回 `UNSUPPORTED_ACTION`，摄像头关闭仍是 `NO_MATCH`。
- Synthetic“开始录像”证明 ACK 仅表示 Core 受理、Recording 仍为 STARTING；
  MockMedia SUCCESS 的 RESULT 后才成为 RECORDING。FAILURE/TIMEOUT、迟到 SUCCESS、
  cancel、旧 session、过期、否定、未知文本、PARTIAL、重复 FINAL 均有集成测试。
  Mock RTOS RESULT 显式 simulated，不构成实际 RTOS 控制证据。
- `bash scripts/dev/host_ci.sh` 退出0：CTest 18/18、Python 6/6；新增集成测试
  连续50轮通过；独立 ASan/UBSan 构建与集成测试1/1通过。等级
  **`VOICE_INTENT_CORE_INTEGRATION_PASS`** 只表示 Host 文本意图到 Vehicle Core Mock
  服务的闭环，不表示 Camera/Recording/RTOS 实际执行或实时语音控制通过。

## 20. 2026-10-02 Voice Intent + Real CAM0 integration

- 分支 `agent/voice-intent-real-cam0` 以真实CAM0基线 `914b5a5` 为第一父提交，
  普通merge引入Intent/Core提交 `c50aa29`；merge提交 `36863cf` 保留双方历史。
- 路由表现在有10条规则/23个alias。新增typed `CLOSE_CAMERA`；固定映射为
  `OPEN_CAMERA → CAMERA_PREVIEW_START`、`CLOSE_CAMERA → CAMERA_PREVIEW_STOP`，两者
  均无参数，ASR原文不进入Core参数。
- Host FakeCamera套件证明开、关、重开、重复FINAL只调用一次service、Rear unavailable
  不破坏前摄状态、否定/PARTIAL/NO_MATCH无硬件动作，以及timeout后的迟到success不改
  canonical revision。最终Host CI为CTest 27/27、Python 6/6和shell检查通过。
- RK3576使用GCC 12.2、Qt5、V4L2 ON原生构建，CTest 27/27通过。实时图解析将OV8858
  CAM0经`/dev/media1`解析为该次运行的`/dev/video11`，实际完成STREAMON、3帧、
  STREAMOFF、重开和最终释放；duplicate FINAL在MediaService边界计数为1，Rear返回
  `UNAVAILABLE`且前摄仍STREAMING，negation产生0个Core命令和0个硬件动作。
- 首次板端调用因外层resolver误把`/dev/media0`的诊断文本当设备路径，在打开硬件前
  失败；修正为只接受`/dev/video*`后进行一次有限重试并通过。该失败与修正均保留在
  `docs/bringup/voice-intent-real-cam0/BOARD_RESULT.md`。
- 当前等级 **`VOICE_INTENT_REAL_CAM0_PASS`** 只覆盖synthetic `ASR_FINAL`到真实CAM0；
  不升级实时麦克风/VAD/wake、Recording、RTSP、CAM1、RKNN/RKLLM、AMP或RT-Thread状态。

## 21. 2026-10-02 CAM0 real recording integration

- 独立worktree `/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-recording`、分支
  `agent/media-cam0-recording`从`b8b27ec`建立；只增加CAM0 H.264 Annex-B录像，
  未实现MP4、RTSP、CAM1、RGA、零拷贝或实时VAD→Intent。
- 板端安装的`librockchip-mpp1`/`-dev`/demos为1.5.0-1 arm64，runtime自报commit
  `43a191ed`，pkg-config自报1.3.9；三种版本标识不一致但原生编译、synthetic NV12
  编码和真实CAM0编码均通过。动态库SHA256为
  `1aca0bed4ba184f5fef4841e381e8b9918983df02ebfd8c3983c6919acdc8bc5`。
- `MediaService`继续是唯一CAM0 owner。Preview和Recording消费同一个owned frame；
  Recording-only可启动capture而不伪造Preview。容量12的录像队列满时显式
  `RECORDING_BACKPRESSURE`并把canonical Recording置Error，不阻塞capture线程。
- START在ACK后为STARTING，只有首个真实MPP packet完成后RESULT SUCCESS才进入
  RECORDING。STOP停止新提交、排空队列、关闭文件后才RESULT SUCCESS/STOPPED；
  Preview活跃时不停止CAM0。重复START/STOP保持幂等。
- 真实10秒测试完成301 capture frame、300/300 encode frame，29.8772/29.9766 fps，
  文件9,757,751 bytes，SHA256
  `5469d757a17630c6759f460bdacf959fb42735998fd54387762f474ac8d7c79b`；
  SPS/PPS/IDR/slice与ffprobe High@4.0 1632x1224 30/1均通过。
- 20轮启停全部通过，无EBUSY、无线程增长且每轮设备可重开。MPP首次使用保留一个
  进程级FD（cold 8→first 9），cycle 1至20维持9，不存在逐轮增长；此常驻项不是
  “零常驻FD”主张。
- 五分钟Preview+Recording完成8,976 capture frame、8,965 preview delivery和
  8,965/8,965 encode frame，三条路径为29.8755/29.8775/29.8782 fps，queue peak 1；
  overflow、sequence gap、poll timeout、DQBUF/QBUF和encoder error均为0。稳定采样
  CPU均值18.9%、RSS/PSS为20,944/19,927 KiB、thermal zone峰值51.768°C；
  300,028,816-byte文件闭合后CAM0无owner。
- Synthetic ASR_FINAL“开始录像/停止录像”经Intent→Core→RealMediaServiceAdapter→
  同一MediaService完成，Recording来源为RUNTIME；duplicate FINAL只到service一次，
  RTSP和Rear仍UNAVAILABLE。UI录像路径由板端Qt fake-recorder CTest覆盖，未新增人工触摸验收。
- Host最终CI、sanitizer和板端native tests结果见
  `docs/bringup/media-recording/`。当前等级为 **`MEDIA_CAM0_RECORDING_PASS`**；
  不代表MP4、RTSP、CAM1、Long-term Recording或Live Voice Recording Control通过。

## 22. 2026-10-02 CAM0 RTSP integration

- 独立worktree `/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-rtsp`、分支
  `agent/media-cam0-rtsp`从Recording PASS提交`08b7a4b`建立。
- 原`MppH264Recorder`已拆为`MppH264Encoder`、`EncodedPacket`与独立
  `FileRecordingSink`。Recording/RTSP由同一`MediaService`共享单CAM0 capture和
  单encoder；停止任一consumer时，另一个仍可保留encoder。
- 首版为单CAM0、单客户端、无认证LAN服务：RTSP TCP支持OPTIONS/DESCRIBE/SETUP/
  PLAY/TEARDOWN，媒体为UDP unicast RTP/H.264；支持SPS/PPS、IDR join、单NAL、FU-A、
  90kHz timestamp、marker及默认1024 packet有界queue/drop/resync。
- Host最终CI为29/29 CTest、6/6 Python；Recording+RTSP ASan/UBSan 2/2通过。
  RK3576 Debug CTest 23/23通过，release probe、sanitizer probe及Qt5真实媒体target
  均原生构建。
- 旧地址`10.34.122.223`四次T0超时保留为历史证据。用户提供新地址
  `10.232.249.223`后，复用`lubancat`别名认证成功。实时graph重新确认
  OV8858 `3-0036`经rkisp0 mainpath到该次boot的`/dev/video11`。
- Board ffprobe/ffmpeg识别并持续接收H.264 High 1632x1224@30。断开/重连通过，
  第二次连接从新IDR解码；WSL板外客户端经wlan0收到OPTIONS/DESCRIBE 200及含
  SPS/PPS、1632x1224、`a=framerate:30`的SDP。WSL无媒体decoder，板外RTP解码
  未验证，实际解码证据来自板上既有客户端。
- 五分钟Preview+RTSP与五分钟Preview+Recording+RTSP均通过；后者只启动一个
  encoder，录像文件309,342,219字节且ffprobe可解析，recording overflow、RTP drop、
  sequence gap、poll timeout、DQBUF/QBUF及encoder error均为0。probe约43-44% CPU、
  PSS约23.7 MiB，最高采样温度53.615 C。
- 20轮RTSP START/STOP通过，40个ACK与40个独立RESULT，退出后8554和进程均释放。
  首次T3曾由probe线程按引用捕获局部mailbox导致UAF；板端ASan定位后改为按值捕获，
  重跑sanitizer及五分钟测试通过。详细证据见`docs/bringup/media-rtsp/`。
- 当前等级 **`MEDIA_CAM0_RTSP_PASS`** 仅覆盖上述单客户端台架范围；不代表CAM1、
  Audio RTSP、Internet streaming、multi-client、鉴权/TLS、H.265或长期稳定性通过。

## 23. 2026-10-02 Voice Runtime orchestration

- 分支`agent/voice-runtime-orchestration`从RTSP稳定提交`2af5303`建立；该历史已经包含
  Live ASR、VAD、Intent/Core、真实CAM0、MPP录像与RTSP，不从旧语音分支重做。
- 新`VoiceRuntime`拥有`VadLivePipeline`和`VoiceIntentDispatcher`生命周期；ALSA/VAD/
  ASR backend、Controller、VehicleCore和MediaService仍由上层application拥有。
  ASR callback只把FINAL复制到有界queue，worker侧才激活session、路由并提交Core。
- 修正VAD FINAL入队后立即Completed造成worker拒绝的竞态；session现在由Dispatcher
  report完成。PARTIAL不入队，duplicate/cancelled/stale/expired不会重复进入Core。
- Host CI为30/30 CTest、6/6 Python；ASan/UBSan下Runtime、VAD和Intent/Core 3项通过；
  同一Runtime对象50轮start/stop通过，VAD只configure一次，无detached thread。
- RK3576使用GCC 12.2原生构建，CTest 24/24。真实ALSA/VAD/ASR runtime协商16 kHz/
  mono/320-frame period并采集136,960 frames，XRUN/overflow为0，audio/dispatch queue
  peak均为1，退出前后thread为1/1。
- 同一application中四个明确synthetic FINAL经Runtime→Intent→Core→RealMedia分别完成
  CAM0 open/close和MPP recording start/stop。录像91 packets、2,905,668 bytes，ffprobe
  为H.264 High 1632x1224@30；Camera/ALSA重开通过，退出后无holder或8554监听。
- 当前等级 **`VOICE_RUNTIME_ORCHESTRATION_PASS`**。本次安静运行`utterances=0`，没有
  真人speech→VAD end→ASR FINAL→硬件链，因此不升级`VAD_LIVE_PIPELINE_PASS`、
  `LIVE_VOICE_CONTROL_PASS`、wake、TTS、RKLLM或完整语音助手状态。证据见
  `docs/architecture/VOICE_RUNTIME_ORCHESTRATION.md`和`docs/bringup/voice-runtime/`。

## 24. 2026-10-02 CAM0 RKNN Vision integration

- 分支`agent/rknn-vision-cam0`从Voice Runtime提交`3d95ac2`建立，保留
  `MEDIA_CAM0_RTSP_PASS`与`VOICE_RUNTIME_ORCHESTRATION_PASS`基线。
- 板端`librknnrt.so`为2.3.0、driver 0.9.8。系统MobileNetV1模型SHA256
  `bc66943ea85ec0dd8a04da22c4276bfc8a4c6fe24f5ea8be7a1e5c3c22c8259d`与
  官方Toolkit2 v2.3.0 Git artifact一致；模型/SDK头未进Git，Rockchip再分发权待审。
- `MediaService`仍是唯一CAM0 owner。Vision使用同一owned NV12 frame、容量2的
  latest-frame-wins队列、8 fps采样、CPU stride-aware RGB letterbox与单一RKNN context；
  frame/result保留camera/sequence/stream_epoch，旧epoch结果不发布。
- 官方固定图真实NPU推理通过。10秒CAM0测试完成75次推理，7.46996 fps；五分钟完成
  8,963 capture、2,241 inference，queue peak 1，queue/stale drop和V4L2错误均为0。
  五分钟CPU均值23.61%、PSS峰值33,738 KiB、最高温54.538 C。
- 60秒Preview+Vision通过。90秒Preview+Recording+RTSP+Vision只启动一个capture与
  一个MPP encoder；完成673次推理/2,692帧编码，recording overflow及RTP drop为0。
  ffprobe/ffmpeg实际识别并解码H.264 1632x1224@30，录像文件也可解析。
- AArch64 Qt5/X11真实运行报告Vision `RUNNING`、73次result、7.46982 fps；Host映射测试
  覆盖Runtime/Online、模型、rate、class及epoch/sequence。没有新增人工触摸或截图验收。
- RK3576使用GCC 12.2 Debug原生构建，启用V4L2/MPP/RKNN后的target CTest 31/31通过。
  本轮不把该结果写成Release构建认证。
- Host CI为31/31 CTest、6/6 Python；非Qt 27项在ASan/UBSan+LSan下通过，全31项在
  ASan/UBSan下通过。Qt offscreen启用LSan仍只见既有656-byte Qt/fontconfig退出缓存。
- 当前等级 **`VISION_RKNN_CAM0_PASS`**。不代表检测/跟踪、CAM1、RGA/zero-copy、
  第二模型、Voice+Vision同载、RKLLM vision或模型自由再分发通过。证据见
  `docs/architecture/RKNN_VISION_PIPELINE.md`与`docs/bringup/vision/`。

---

## AMP authority tip 的原始状态快照

以下“本轮/当前”指 AMP tip 整理当时；独立 AMP 分支限制不阻止此次经用户授权的源码收敛。

### AMP/RPMsg 最小链集成事实（原记录）

2026-10-04。**AMP/RPMsg 最小链 BOARD_TESTED，READY_TO_INTEGRATE；按用户要求冻结。** 当前集成入口是 [AMP_RPMSG_INTEGRATION_TIP](amp/AMP_RPMSG_INTEGRATION_TIP.md)，精确源码/产物/证据在配套 JSON，最终 Git SHA 由交付 tip 文件给出。

## 当前已证事实

| 项目 | 当前事实与证据 |
| --- | --- |
| 板/核 | LubanCat-3 v2 / RK3576 BUS Cortex-M0，RT-Thread 4.1.1；COM6真实运行 |
| Boot | proper149b1c5、当前8MiB SHA f9beef07…72c0b3；开发公钥conf签名AMP FIT、单次amp_m0load及C Linux启动均实测 |
| 配套系统 | 独立6.1.99-rk3576-m0echo-p026，Image/modules/initrd/DT/KO同源配套；保留默认#8入口 |
| 内存/通知 | 三段no-map；实际rings47d00000/47d08000、DMA base47d10000；link4、RX MBOX0/TX MBOX4 |
| 时基 | v5条件本地LOAD239998→326，ISR/tick各+54，首次RT延时返回 |
| 双向通信 | Linux收到HELLO_ACK/PONG；M0接收27d18010/len5、27d18210/len4并PONG sent；三处cache bypass1 |
| 冷恢复 | 用户完整断电再上电，默认6.1.99-rk3576 #8/root p3/boot p2/无stage身份 |
| 源码/Git | 当前纯AMP分支agent/amp-platform-closure；相对bootstrap基线没有UI/Voice/Media代码或提交增量 |

实际命令、来源附件 hash、重复第二次insmod/File exists偏离及范围限制见 [C执行记录](reviews/rk3576-amp-platform-closure/P030_RPMSG_C_V1_EXECUTION.md) / [JSON](reviews/rk3576-amp-platform-closure/P030_RPMSG_C_V1_EXECUTION.json)。原件保持和被动安装读回见C准备记录。

## 当前范围与未知项

用户明确不开发新RTOS业务，不合UI/Voice/Media。现有最小链无需重跑C。本轮仅整理源码/配置/提交/脱敏证据，无板访问、构建、签名或新测试。

原始CON16/17寄存器值、整个512MiB窗口、长期/缓存开启稳定性、热重连/业务心跳/用户态ABI/MPU6050仍未实测，不冒充本次完成项。原厂默认内核的逐字源码匹配未知；实际链使用独立paired kernel，此旧问题已不阻塞已测最小链集成。旧 full-deployment/D schema不作为本tip的集成门，也不据短测扩大生产验收。

## 历史与其他项目

此前STATUS全文和非AMP板端/相机/音频等事实保留于 [历史状态](reviews/rk3576-amp-platform-closure/STATUS_HISTORY_20261004.md)。这些模块不在本次集成范围；保留其原证据等级，不从AMP结果推断其完成。

旧AMP Markdown已明确标注历史或转向当前tip；旧JSON保留当时快照，[历史索引](amp/AMP_RPMSG_HISTORY_INDEX.json)登记范围。失败、撤回、旧包BLOCKED仍为真实历史；不能继续当作“当前最小链尚未启动/尚未通信”的结论。当前恢复后的系统处于默认Debian，M0启动/KO由用户已完成的测试记录支持，不代表默认上电自动运行。

### 2026-10-05 MPU I2C9 BSP 派生修复（Host）

四项源码问题已由派生补丁修复，实际 driver/board Fake 回归 ASan/UBSan 与原生 SCons 适配诊断构建通过；[结果与边界](bringup/mpu6050/I2C9_BSP_FIX.md)。仅 `I2C9_ADAPTER_HOST_TESTED`，不升为传感器 HOST_PASS。全量 Kconfiglib baseline/patched 均有原有 LED 依赖循环；M0 访问权限与 Linux clock/reset/pinctrl ownership 尚未实板证明，仍 `WIRING_NOT_READY / OWNERSHIP_NOT_CLOSED`。无 MPU 访问或板端部署。

主控独立复核 driver sanitizer/board fixture、5/5构建安全预检、native产物hash及source身份后通过；最新 reviewer-host-ci 为31/31 CTest、52/52 Python、5/5撤回，日志仅保留 artifacts/local/mpu-i2c9-fix/reviewer-host-ci.log。

### 2026-10-05 接线前资源配置续审（历史记录）

Linux I2C9 clock/pinctrl 派生 DT、M0 延迟两项 reset deassert 和 INTMUX 专用 gate 持有已完成 Host 实现；DT 编译/非目标资源逐项对比、失败回归、实际驱动 ASan/UBSan 与 fresh 原生诊断构建通过。最新 host_ci：31/31 CTest、59/59 Python、5/5撤回。详见 [接线前结果](bringup/mpu6050/PREWIRE_RESOURCE_RESULT.md) 及配套 JSON；前段52项为当时审核记录，保持不改写。

普通用户持锁 L1 盘点确认当前默认 Debian6.1.99-rk3576，I2C9 disabled/无 adapter，mcu-amp 未绑定/RPMsg为空；未做 MMIO/I2C/部署。静态 ownership 配置已闭合到可审查提案，实际冻结 BL31 对 BUS_MCU 的权限及 INTMUX reset/gate 运行状态尚未证明：`WIRING_NOT_READY / RUNTIME_ACCESS_PENDING`。不标传感器 HOST_PASS 或任何实板 PASS；有限无传感器访问验证方案只准备，仍需独立审批。

主控独立复测上述三补丁/DT负例/产物hash及host_ci全部通过；Host接线前配置完成，实际冻结BL31权限仍`BLOCKED`。无传感器权限诊断包未构建、不可批准部署。

### 2026-10-05 无传感器权限诊断包（历史记录）

用户最新消息授权下一步且不需额外审批；独立I2C_RESOURCE_PROBE_V1完成Host构建/配套KO/原信任链FIT验签/资源DT/八文件安装器与preflight/退出冷恢复脚本，主控复核host_ci31/67/5及actual-header sanitizer通过。主控已被动新增安装至`/boot/amp-p029/i2c-resource-probe-v1`并逐hash读回；尚未冷进入/加载KO/执行M0诊断。详见[精确包与执行恢复](bringup/mpu6050/RESOURCE_PROBE_HOST_PACKAGE.md)。无I2C事务，实际BL31权限仍待定向读证据，不升接线/传感器PASS。

### 2026-10-05 实板准备尝试结束（历史记录）

I2C_RESOURCE_PROBE_V1 v2被动安装/读回已通过；第二次双UART READY后主控持锁执行正常shutdown，COM5观察Power down。**当前板已关机，用户接回主电及默认启动核验待完成**，不再以先前L1快照表述默认系统正在运行。startup120秒采集自然结束（COM5 4702B/COM6 2B/source_once=False）；未见cold boot，未LOAD/INSPECT/SOURCE、未启动新M0/FIT、未加载KO或执行诊断MMIO/I2C。

关机SSH断开使持锁会话61397 exit1并释放锁，采集会话88023随后exit0；两者当前均结束，不能称整个cold窗口持续持锁。下一动作由主控重新持锁/采集并核实时环境。状态仅`NORMAL_SHUTDOWN_OBSERVED / USER_COLD_POWER_ACTION_PENDING`，诊断与恢复均未PASS；详见[最新板端记录](bringup/mpu6050/BOARD_RESULT.md)。

### 2026-10-05 默认系统冷恢复确认（历史记录）

用户已上电，主控只读盘点/实际恢复validator通过：默认6.1.99-rk3576/rootp3boot2、RPMsg/probe KO/项目进程为空、25个默认/冻结/SI文件大小/hash一致；日志resource-probe-root-after-attempt2.log。**当前已恢复运行默认系统**；前一条关机等待为历史。未执行新AMP诊断，不升权限/传感器PASS。

collector人工等待改300秒、首次cold后120秒、SOURCE后120秒，总上限540秒/每路256KiB，一次cold marker/一次SOURCE，不自动retry；实际PS六模式factory回归通过，v2生产包不改。主控最新host_ci31/31 CTest、72/72 Python、5/5撤回PASS（resource-probe-root-host-ci-final.log）。


### 2026-10-05 Sensor service Host子里程碑（历史记录）

公共BE codec、独立0x3005 sensor endpoint、单订阅/租约/latest背压与READY worker、同health transport双owner有界退出完成；实际fixture sanitizer/native SCons/FIT验签及host_ci31/77/5通过。详见[SERVICE_HOST_RESULT](bringup/mpu6050/SERVICE_HOST_RESULT.md)。用户断电接线USER_CONFIRMED；配套Linux sensor KO/rpmsg_srv/Core/Qt仍待实现，NOT_DEPLOYABLE，完整业务四等级与最终集成NOT_RUN，无本轮板端操作。下一步完成Linux桥及canonical/UI Host集成，不退回旧冻结tip或重复权限诊断。
