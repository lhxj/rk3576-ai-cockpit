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

# 当前系统集成状态（2026-10-04）

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

# 当前状态：事实、约定和未知项

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

### 2026-10-05 接线前资源配置续审（当前）

Linux I2C9 clock/pinctrl 派生 DT、M0 延迟两项 reset deassert 和 INTMUX 专用 gate 持有已完成 Host 实现；DT 编译/非目标资源逐项对比、失败回归、实际驱动 ASan/UBSan 与 fresh 原生诊断构建通过。最新 host_ci：31/31 CTest、59/59 Python、5/5撤回。详见 [接线前结果](bringup/mpu6050/PREWIRE_RESOURCE_RESULT.md) 及配套 JSON；前段52项为当时审核记录，保持不改写。

普通用户持锁 L1 盘点确认当前默认 Debian6.1.99-rk3576，I2C9 disabled/无 adapter，mcu-amp 未绑定/RPMsg为空；未做 MMIO/I2C/部署。静态 ownership 配置已闭合到可审查提案，实际冻结 BL31 对 BUS_MCU 的权限及 INTMUX reset/gate 运行状态尚未证明：`WIRING_NOT_READY / RUNTIME_ACCESS_PENDING`。不标传感器 HOST_PASS 或任何实板 PASS；有限无传感器访问验证方案只准备，仍需独立审批。

主控独立复测上述三补丁/DT负例/产物hash及host_ci全部通过；Host接线前配置完成，实际冻结BL31权限仍`BLOCKED`。无传感器权限诊断包未构建、不可批准部署。

### 2026-10-05 无传感器权限诊断包（当前）

用户最新消息授权下一步且不需额外审批；独立I2C_RESOURCE_PROBE_V1完成Host构建/配套KO/原信任链FIT验签/资源DT/八文件安装器与preflight/退出冷恢复脚本，主控复核host_ci31/67/5及actual-header sanitizer通过。主控已被动新增安装至`/boot/amp-p029/i2c-resource-probe-v1`并逐hash读回；尚未冷进入/加载KO/执行M0诊断。详见[精确包与执行恢复](bringup/mpu6050/RESOURCE_PROBE_HOST_PACKAGE.md)。无I2C事务，实际BL31权限仍待定向读证据，不升接线/传感器PASS。

### 2026-10-05 实板准备尝试结束（最新实时状态）

I2C_RESOURCE_PROBE_V1 v2被动安装/读回已通过；第二次双UART READY后主控持锁执行正常shutdown，COM5观察Power down。**当前板已关机，用户接回主电及默认启动核验待完成**，不再以先前L1快照表述默认系统正在运行。startup120秒采集自然结束（COM5 4702B/COM6 2B/source_once=False）；未见cold boot，未LOAD/INSPECT/SOURCE、未启动新M0/FIT、未加载KO或执行诊断MMIO/I2C。

关机SSH断开使持锁会话61397 exit1并释放锁，采集会话88023随后exit0；两者当前均结束，不能称整个cold窗口持续持锁。下一动作由主控重新持锁/采集并核实时环境。状态仅`NORMAL_SHUTDOWN_OBSERVED / USER_COLD_POWER_ACTION_PENDING`，诊断与恢复均未PASS；详见[最新板端记录](bringup/mpu6050/BOARD_RESULT.md)。
