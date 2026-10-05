# 2026-10-05 manual-v6：音频冷启动缺失二次复现，停止同样重测（当前）

主控已结束失败现场只读取证并正常关机：关机前paired kernel、无taskKO/node/actors/8554确认；UART1124.089925 reboot: Power down（Windows manual6-result-shutdown/linux-com5.log），collector40688 CANCEL exit0、sharedlock76404 RELEASE exit0。当前等待用户默认冷恢复确认；没有继续同组合boots或reprobe。以上“保留供电”描述是此前采证阶段，不是当前状态。

manual-run-v6/logs-20261005T152226203Z再次仅PA auto_null.monitor、ALSA HDMI/DP；Linux启动3.395811秒ES8323 3-0011 i2c recv Failed。unique exact source硬门拒绝，无PA USER setter、entry、两KO、Qt或共存启动；音频冷启动BLOCKED，RTP pacing实板验证NOT_RUN，最终集成未PASS。主控失败现场实读仍paired6.1.99-rk3576-m0echo-p026，3-0011 DT节点存在但无driver link，es8388-sound无driver link且deferred，cards持续缺失，两KO/node不存在。原始证据manual6-pa-source-stop-logs、manual6-failed-live.log。保留供电进行有限只读取证，不再同样重测/盲目冷启动，不改冻结核/DT或弱化audio guard。

本地冻结kernel es8323_i2c_probe在i2c_master_recv一字节返回负值时打印该日志并原样return，发生在ASoC component注册及其mclk enable之前；不是chip-ID验证。日志未输出底层ret，无法区分NACK/timeout，也不能据此修delay/重试/clock。下一诊断仅sysfs binding/deferred、运行DT及现存clock/pinctrl/regulator摘要，不触发I2C事务/PCM/MMIO读取或force reprobe；缺失项记录缺失，root独占实板。


失败现场补证manual6-failed-resource-live.log：实际2ac60000.i2c/rk3x-i2c绑定，GPIO4-12/13(pin140/141)归I2C3；GPIO1-12/13(pin44/45)归AMP I2C9；SAI1 pad131/133/137/139正常归SAI1。3-0011 waiting_for_supplier=0且mclkout-sai1 supplier link存在，但codec driver/component未注册；link存在不等于provider有效时钟证明。clock summary读入INPUT_CAP_EXCEEDED是未取得结果，不能当empty/zero；错误候选2ac70000不是I2C3，猜测根层mclk节点为空也不能作为provider不存在证据，应沿实际sysfs of_node解析。已保全Linux UART只见ES8323读取失败、无rk3x timeout/ipd日志，但不能据日志缺失断言具体errno。此时尚无可定位的安全kernel/DT修复。


manual6-failed-clock-live.log后续bounded streamfilter实际完成（输入166285B）：mclk provider driver=rockchip-clk-out，of_node实际/clocks/mclkout-sai1@26046400，运行DT provider及codec phandle链、12.288MHz assigned-rate完整；I2C9保持24MHz/enable1与PCLK100MHz/enable1，I2C3当前198MHz/PCLK100MHz enable0 prepare1。MCLK链当前12.288MHz/enable0/gate N、provider runtime suspended，GPIO4-2 MCLK pad UNCLAIMED。这是失败后空闲现场；rk3x xfer会临时enable并在结束disable两clock，codec driver失败后也可释放pinctrl，因此这些0/UNCLAIMED不能证明probe时clock/pad错误。当前没有已定位的安全repair，尤其不得直接启clock、跳初次read、force bind/reprobe或加blind retry。若后续独立kernel诊断需保持原一次事务/负ret失败并记录其errno和启动时序，另目录独立构建kernel/headers/KO/hash，不能替换冻结组合；本轮停止同组合boots。

---

# 2026-10-05 manual-v5：音频卡启动probe失败，pacing实板验证未运行（历史）

manual-run-v5/logs-20261005T145352237Z已在paired kernel 6.1.99-rk3576-m0echo-p026执行SOURCE，但Linux UART启动3.295553秒报告ES8323 3-0011 i2c recv Failed；3.671137秒ALSA仅列HDMI/DP，20.310718秒es8388-sound deferred probe pending。PA wrapper在唯一exact capture source检查拒绝；无PA_BASELINE/USER setter、entry、两KO、Qt或共存启动，本轮T5/T6及20Mbps pacing硬件验证均NOT_RUN，最终集成未PASS。完整原始日志及hash：artifacts/local/mpu-root-review/manual5-pa-source-stop-logs。该失败不能以当前默认PA正常或parser正常覆盖。

用户明确失败后自行断电再上电。主控随后实际默认uname6.1.99-rk3576、两KO/node不存在，ALSA card0 ES8323和exact PA source index1正常；同v2 parser离线解析state=SUSPENDED、suspend cause=IDLE、USER=false成功（manual5-pa-missing-current.log、manual5-default-source-parser.json）。这是用户自主冷恢复后的当前实读，不是本轮正常shutdown/Power down流程PASS；本轮未重复35项保护hash。manual capture CANCEL/ports closing和shared lock RELEASE有日志，原失败证据保留。


## manual-v5音频启动失败的本地ownership复核

本轮未进入sensor KO加载，M0原始UART无WHO/CONFIG/resource记录。sensor_service_request只有收到SV_HELLO且flags!=1才调用initialize；query不触发，initialize经mpu_sensor_resource_ready进入rockchip_i2c9_resource_ready。PREV分支跳过I2C9初始化；最终native-v8配置仅I2C9/UART5，rt_hw_iomux_config仅调用UART5 mux，ownership排除通用I2C7 mux。因此本轮流程不支持“MPU业务已初始化I2C9并干扰codec”的解释，但不能外推M0 BSP全无clock写。

厂商p023 HAL核验：I2C3 mux=CLKSEL_CON57[5:4]，I2C9 mux=CON58[1:0]。I2C9 held路径只读24MHz、不set-rate或enable/disable I2C gates；resource_ready后开独立INTMUX2BUS gate(0xbc)，deassert I2C9 PRESET(SOFTRST12 bit8)/RESET(SOFTRST13 bit4)，随后HAL_I2C_Init只写传入I2C9的CLKDIV/CON。I2C3 PRESET/reset为SOFTRST12 bit2/bit14；reset写宏CRU_WRITE=((mask)<<16)|(value<<shift)，或VAL_MASK_WE，同为high-word bit write-enable，不是覆盖共享寄存器或RMW。HAL API成功不是实际MMIO/readback证明。

实际冻结sensor.dtb SHA33dc67a8a0199d4d0b313e980657982297155f364509552ecc3a92d650c3064e与原冻结SI stage-C.dtb SHAdd68818b7fd27e9abc56522d7c4782adb0b1dd49d274fe1daa75a32ad3366fbf重新逐节点/属性解析对照：仅/mcu-amp五属性clocks/assigned-clocks/assigned-clock-parents/assigned-clock-rates/pinctrl-0变化；I2C3完整子树（含ES8323与CAM0）及sound节点字节相同，其余所有属性相同（manual5-local-dt-ownership-audit.json）。Linux mcu-amp probe持有I2C9时钟及选择GPIO1B4/B5 function10；M0不选择该pad。该源码/DT证据排除本次改动直接覆写I2C3音频/CAM0配置及上述I2C9 reset误mask，但不能排除冷启动电气/时钟时序或未采集的其它运行状态；codec注册失败根因仍unknown。

下一增量仅独立PA wrapper v3/manual-v6失败出口诊断：unique exact source拒绝时有界输出source名称/身份、ALSA cards及指定ES8323/es8388/deferred kernel行，各JSON<=16KiB总<64KiB；无setter、wait/retry或PCM操作，诊断失败保留原guard异常。原entry/coex/APP/KO/FIT/M0/音频独占及zero-error门不改。新窗口如再次cold codec缺失即停止并保全启动证据，不自动重试，不凭当前默认正常修kernel/DT。



当前PA v3用户目录部署读回PASS，实板业务/诊断新窗口NOT_RUN，音频probe因果UNKNOWN。DT审核仅SOURCE_VERIFIED，不能倒推失败窗口运行寄存器或时序。下一次有界startup诊断若身份及音频守卫健康才继续原Qt/300秒共存；失败保留供电及原日志供主控采集live证据，无自动重试。

---

# 2026-10-05 manual-v4：应用301.301秒通过，解码器接收UDP溢出，默认已恢复（历史）

本次真实T5 Qt观察后用户明确确认“已看到变化，此前固定静止”，记UI_SENSOR_PASS/USER_CONFIRMED；原应用日志human_confirmation=USER_CONFIRMATION_PENDING保持原文，人工确认独立映射。91组SENSOR_UI seq18..1820、max age12ms；64组资源全窗口（含启动）max CPU per-core scale298.75%、RSS283788KiB/PSS271032KiB/threads27/fds52、min MemAvailable2847108KiB，非稳态/精度或延迟校准。T6全组件实际301301ms、APPLICATION_EXIT0与APPLICATION_PROBE_PASS；RTSP解码9362帧但1245B真实H264错误，controller FAIL，最终集成未PASS。

v5父观察时间线证明decoder owned RTP socket inode41186 drops累计371，RTCP inode41187 drops0；global UDP InErrors/RcvbufErrors baseline0→371。十批解码错误父观察时刻191.088..420.005均在运行阶段、早于shutdown439.902及clientstop440.158，与随后有界UDP快照drop增长对应，不能归因仅启动或关停。接收socket溢出已证明，具体每AU/RTP序号映射未采集；sender无节奏连续sendto是候选机制，不宣称packetizer字节损坏或修复已实板通过。证据t5t6-v5-board-logs五文件及t5t6-v5-udp-causal-analysis.json。

PA实际IDLE/无USER→SUSPENDED/USER→结束SUSPENDED/IDLE/无USER，恢复成功，entry原失败保留。主控fresh post-health PONG620→623、error0，正常rmmod/node/actors/fuser/8554释放PASS；正常shutdown UART697.547536 Power down、collector/锁exit0。用户默认冷上电后实际uname6.1.99-rk3576且无两KO/node确认PASS（t5t6-v5-default-identity.log）；本轮未重复35项保护hash。

L0待实板验证修复：单active UDP client sender逐包20Mbps pacing，高于原codec8..8.5Mbps预算；按完整RTP datagram含12B header计算，默认1212B包间隔484800ns。steady_clock实际发送完成锚定下一时刻，不做延迟追赶burst；只worker在正常运行修改时刻，start安全复位，PLAY不复位。采集/录像submit仍仅入原有界队列1024；stop拒绝新enqueue并保留受限drain，不改变FU-A/codec/接收buffer/ffmpeg/zero-error门。默认满队列理论发送间隔预算约0.497秒，不含scheduler/transport开销；大payload配置按字节更久，不外推默认上限。Host完整35 CTest/176 Python/5 withdrawal PASS（rtp-pacing-host-ci-final.log）；旧未pacing发送实现配新无delaytest实际rc1在包间隔断言失败（rtp-pacing-old-negative/result.json），主控独立ASan/UBSan media_rtsp PASS（rtp-pacing-root-sanitizer.log）。独立app-v5原生builder及301成员reviewed archive已准备，主控逐成员与current源字节核对PASS并在默认系统启动原生构建；清单PACED_V5_BUILD.json。默认系统独立native build实际exit0，四ELF实际size/SHA/AArch64及ldd全resolved读回PASS（rtp-pacing-native-readback.log）；v6/PA2四控制器仅fresh用户目录部署并size/SHA/compile读回PASS（rtp-pacing-controller-stage-readback.log），manual-run-v5发布PS5 HostCheckOnly PASS。最终Host CI35 CTest/209 Python/5 withdrawal PASS，33 focused无skip，主控final ASan/UBSan media_rtsp PASS。清单manual-run-v5/ASSETS.json保留全部实际pins；实板修复runtime仍NOT_RUN，最终集成未PASS，等待主控正常关机后独立手动测试。

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

# MPU6050真实Qt与五分钟共存：loaded下一窗口（T5/T6尚未执行）

最新用户已授权继续且不需要再次审批。具体产物仍由主控审核并唯一持共享锁操作板；本文件不将Host构建算作实板T5/T6。接线USER_CONFIRMED；用户刚重新固定模块，不能据此补写上一轮静止。冻结AMP/Application/SI_HEALTH及已部署正式包v3保持原样。当前默认6.1.99/rootp3/bootp2，首guard停止后主控冷恢复25+旧sensor十文件共35 hash/metadata PASS，该首窗口锁/双UART已关闭。下一retry主控已重新持锁78175：最新只读before25与旧十文件PASS（t5t6-retry-default-before.log、t5t6-retry-previous-v3-before2.log）；仅部署新用户目录loaded脚本并核SHA及冻结包hash读回PASS，未启动新固件/KO/应用（t5t6-retry-loaded-stage.log）。

## 精确组合与Host证据

清单：[FORMAL_T5_T6_BUILD.json](FORMAL_T5_T6_BUILD.json)。Host新包为`artifacts/local/mpu-sensor-coexistence-package-v2/packet`，11成员，独立目标`/boot/amp-p029/mpu-sensor-coexistence-v1`；原v3路径`/boot/amp-p029/mpu-sensor-v1`不覆盖。新包v1因UART预算不足被审查否决，保留历史，不能安装使用。

应用由主控实际AArch64原生编译四ELF、核hash/readelf/ldd依赖resolved；源码归档v2 SHA454342eb36a6fd1f27255aeeae163e9fd518fdc86a05df7cd3a37308a5b1647f含288成员，e1b29b81为导出基点，包含工作树增量。新目录`/home/cat/cockpit/mpu-sensor-app-v3`。构建只编译，未运行测试/应用或访问设备。M0原生v8仍MPU_SENSOR_V1服务协议，配置诊断拆成MPU_CONFIG与MPU_RATE短行，最坏格式<=126字节且newline完整；raw限100成功样本。FIT/map/签名验证通过；entry0x141，heap364348B，sensor/control/main栈2048/4096/1024B，高水位未测，owner窗口900秒不延长。传感器KO-v4、health KO-v2、owner DT和配套kernel/initrd复用已验证组合。

最新Host CI34/111/5 PASS（t5t6-resumed-host-ci.log）；此前34/97/5为历史；主控实际sampler八模式ASan/UBSan/LSan1 PASS。新collector生产factory八模式覆盖占用非零退出、cold首次/重复、SOURCE一次、deadline、错prompt与CANCEL；Windows实际.NET验证live reader可见Flush内容且第二writer拒绝。新packet exact11与篡改/缺失/附加/符号链接回归通过。以上不是板端结果。

## 窗口、停止与正常退出

双UART COM5=1500000/COM6=115200、8N1/no flow/DTR/RTS=false，每路总256KiB。两端口成功打开才READY；人工等待300秒，首次真实cold marker开始startup120秒，SOURCE一次后采集900秒，总绝对1320秒。UART只是诊断/控制，产品数据仍真实I2C→M0→独立RPMsg→Linux→VehicleCore→Qt。当前loaded controller总480秒、T5 UI90秒（进程守卫110秒）、T6真实全组件共存至少300秒；冻结child controller420秒（父进程守卫450秒）。Linux health720000ms、M0 900000ms，起点不同分别核验；SOURCE真实age<=390秒，已加载health elapsed<=200000ms。旧冻结root runner630秒只作历史且禁止再次运行。主控SOURCE后立即核身份并启动，不能耗到窗口末尾。采集期限到达不会自动恢复板；任何失败停止扩大测试、保留日志，主控执行正常停机与用户cold恢复，不自动warm reboot/重试。

启动前root fuser对CAM0 `/dev/video11`及ALSA capture `/dev/snd/pcmC0D0c`检查全部UID持有者；应用再核实际CAM0 graph。录像合计512MiB、空闲盘>=1GiB；每日志2MiB，4KiB读取/16KiB完整行，禁止分片插入health。T6共享一个MediaService/Camera/MPP实例，Qt preview/recording/RTSP+真实RKNN/Voice持续工作；capture/encode/record packets/vision inferred/audio captured计数每5秒严格增长，Qt preview最终>0，sensor序号每5秒增长且RUNTIME/VALID、协议/采样/发送错误零，health计数/年龄持续过门。RTSP loopback真实UDP decode>=8000帧且无decoder错误；录像ffprobe；CPU/RSS/PSS/MemAvailable/CmaFree/温度/线程FD在resources.jsonl。UI合并更新单独计数，不计传输丢包。

T5固定静止窗：首次有效UI观测后10秒稳定，取[10,25)秒至少10组1Hz实际换算值。每组加速度模长0.85..1.15g，各轴陀螺均值绝对值<=10°/s；这是预定义功能容差，不称精度校准/零偏校准，不减去上一轮X偏置。窗结束输出SENSOR_STATIC_WINDOW与T5_DIRECTION_CHANGE_ALLOWED后，主控才提示用户轻轻改变固定模块方向，确认六轴数值/芯片温度/状态/年龄真实变化；不移动整板/MIPI/风扇。自动记录永远保留USER_CONFIRMATION_PENDING，人工确认独立登记。

M0错误发布STATUS、连续三次停止；Host controller看到错误状态/health失败立即停止推进，缺样本由有界期限失败，不声称每个I2C错误都立即停M0。T6结束先停止媒体消费者，再runtime.stop内部UNSUB/有界worker join，最后Core/Qt应用退出；只有matching UNSUB RESULT才确认退订，不能称退订先于媒体停止。退出后health继续真实PONG，正常卸载sensor/health KO，确认node/模块/媒体设备释放。M0在无订阅状态不发SAMPLE，采样仍可继续至其900秒窗口；正常关机后用户冷恢复，不做额外故障注入。

## I2C与codec依据

固定kernel源码commit521833e2d28decbd6473d5717f1f96cc4108e208：`artifacts/local/p023-kernel-source/kernel-521833e2d28decbd6473d5717f1f96cc4108e208`位于AMP worktree。drivers/soc/rockchip/rockchip_amp.c SHA91e9d19e0517527b991fe7f1f0dec2665cb65c7b136c0b1795fc2c3f300e05ad，L671..677 probe bulk_get_all/prepare_enable永久保留DT七clock引用，L734仅remove释放，driver无runtime PM回调。sensor KO卸载仅释放其额外两clock和rate-exclusive引用，不移除rockchip-amp基础引用；窗口不unbind AMP、不系统suspend。preflight核DT七clock字节/owner绑定，KO owner-ready核两clock enabled/24MHz；没有读取debugfs实际reference count，不能这样声称。上轮卸载后M0 seq3401/error0支持该生命周期，但不是唯一依据。

同kernel arch/arm64/boot/dts/rockchip/rk3576-lubancat-3.dts L146..161 soundcard rockchip-es8388引用SAI1和es8388；L391..400 I2C3 codec@11 compatible包含everest,es8388及everest,es8323。sound/soc/codecs/es8323.c L873 regmap_i2c与L880 probe recv可产生真实事务错误；卡名ES8388与驱动ES8323不是两块codec。owner DT保留I2C3/CAM0节点原样。默认启动也有ES8323 3-0011 -6，7条与上轮12条覆盖范围不同，不能归因新sensor或直接比总数。T6启动进程前记录kernel时间cursor及有限原有错误；同一运行窗任何新ES8323 -6即停止、留证。仅ALSA枚举不能通过音频验收；还需Voice frames持续增长/XRUN/overflow零等实际门。

## 主控执行与恢复

1. 审核exact清单/installer pin/启动cmd文件hash及当前25+旧十保护记录；被动安装只新目录，读回全部11文件，不自动启动。
2. 使用新包external collector（hash在清单），确认双UART READY后持锁正常shutdown；用户拔主电等10秒后上电。检查真实SPL版本8f53f800da-241224、checked Uboot image hash前缀7d8fe670、controlDT43164981、Uboot149b1c5、cold/policy0。
3. 向ControlFile依次写LOAD/INSPECT；确认load3225字节/filesize=c99/fileaddr4c000000及日志身份后写SOURCE一次。stage脚本逐一核Image/initrd/DT/FIT大小，原load/entry/mailbox/CON16/17不变。任何loader异常冷恢复，不盲retry。
4. 原冻结boot runner因实板psmisc不接受fuser `--`而guard停止，不能再次用它执行测试。下一组合先由主控执行固定包readonly preflight，核两KO尚未加载，再定向加载相同health KO并有界等待HELLO_ACK/PONG>=3、timeout/error=0，再加载同sensor KO；不重载、无强卸载。独立Host已审源码`run_sensor_t5_t6_loaded.py`复制到新用户目录（不写原11文件），核SHA2b957e2cf88360ee694f780275fa0c3fc81a6d98d276be73a8a95b484d9240ba，root执行`python3 /home/cat/cockpit/mpu-sensor-runtime-loaded-v2/run_sensor_t5_t6_loaded.py --source-age-seconds FRESH_ROOT_MEASURED_AGE`。SOURCE实际age<=390秒、已加载health PONG>=3/elapsed<=200秒、总480秒；live GNU build-id、56字节ABI1/owner ioctl与相同应用hash核验通过才启动UI。时间不足直接停止扩大、正常冷恢复，不延长M0900秒或绕过child300秒门。其root fuser已实测不带`--`时rc1且stdout/stderr空为FREE，仍拒绝usage/持有者。25秒静止窗完成后人工方向确认，记录真实观察；保存应用/RTSP/resources/M0日志。
5. 正常模块退出/媒体释放/health后续响应确认后，仍持锁正常shutdown，COM5 Power down后CANCEL释放串口。用户主电冷恢复默认，不由warm reboot替代。默认恢复生产reader与validator：`bash scripts/board/mpu6050/read_resource_boot_baseline.sh`保存after；`python3 scripts/board/mpu6050/verify_resource_cold_recovery.py artifacts/local/mpu-root-review/t5t6-default-before.log AFTER.log`核默认rootp3/boot2、无项目/RPMsg模块及25bytes/hash/resolved/symlink。同时root执行`python3 scripts/board/mpu6050/read_sensor_protected_v3.py --read-only-protected-v3`保存AFTER-V3.log；Host `python3 scripts/board/mpu6050/verify_sensor_protected_v3.py artifacts/local/mpu-root-review/t5t6-previous-v3-before.log AFTER-V3.log`核十hash/metadata。恢复确认才释放锁。

当前T5/T6 NOT_RUN、UI_SENSOR_PASS NOT_RUN、最终等级NOT_RUN。凡尚无板端计数（独立I2C NACK/timeout分类、全窗interval完整分布、stack高水位等）明确UNKNOWN，不能用Host Fake或目标20Hz代填实测。

实际准备记录：主控已被动安装v2并核11hash PASS，双UART READY后正常shutdown/Power down，该历史记录时等待用户cold动作；后续首guard停止及默认恢复见下文，T5/T6仍未执行。证据t5t6-passive-install.log，源构建manifest的board_deployed=false未改写。

事务统计边界：实际native-v8的mpu6050 rd/wr每次一次IO，RTThread i2c_core.c master_xfer调用一次，派生drv_i2c.c只处理组合消息/等待IRQ，无内部retry；HAL I2C Transfer单次Start，NAK未IGNORE时Stop(HAL_NODEV)，adapter仅WR/RD不启用IGNORE。因此aggregate sample_errors=0不会隐藏内部失败后重试成功；但未独立导出NAK/timeout计数，初始化逐笔错误分类/物理未上报异常不可从该计数推导。实际HAL_NODEV及其他非成功结果经adapter泛化UNAVAILABLE，不伪造具体NACK原因。

准备历史：首collector44583在人工准备阶段300秒自然结束，SOURCE=false/COM5=4223B/COM6=2B，仅Power down，无新固件/MPU访问；不是固件失败/额外M0重启。用户准备后主控同hash新collector95611 READY启动attempt2，复用原窗口锁，待真实cold日志。

## 首个正式T5/T6窗口guard停止（不算MPU硬件失败）

主控记录original runner约4秒在Qt/WHO之前退出：设备节点检查后psmisc实际`fuser -- /dev/video11 /dev/snd/pcmC0D0c`返回1、stdout空、stderr中文usage“未指定进程”，触发fail-closed。定向兼容验证`fuser /dev/video11 /dev/snd/pcmC0D0c`实际rc1/stdout/stderr空，确认FREE。证据t5t6-runtime-live.log与t5t6-fuser-compatible-diagnostic.log。两KO正常保留clock直到主控退出；修正controller第一版虽已准备，但SOURCE/health起点预算已错过，未执行，不延长原窗口。主控随后health407→409、timeout/error0，正常rmmod两KO/nodeFREE，shutdown255且实际Power down，CANCEL关闭collector。用户随后冷上电默认Debian；主控生产validator已确认默认/冻结/SI 25及旧sensor-v3十文件共35 hash/metadata PASS（t5t6-guard-stop-default-after.log、t5t6-guard-stop-previous-v3-after.log），锁及双UART关闭。未Qt/WHO/T6，不伪造Sensor或人工PASS。冻结11文件/四ELF/FIT不改；上文独立loaded-controller只改必要用户态兼容与剩余时限，live GNU-note/ABI须下一真实窗口确认。
