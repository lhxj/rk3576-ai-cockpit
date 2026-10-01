# 当前状态：事实、约定和未知项

整理日期：2026-10-01。前十节主要来源是用户提供的实板输出与确认；
第十一节是本轮对实体板的实时文件 ASR 验证。历史日志日期可能受板端时钟影响。
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

## 16. 2026-10-02 Voice Intent → Vehicle Core Host integration

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
