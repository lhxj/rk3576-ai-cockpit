# 当前状态：事实、约定和未知项

整理日期：2026-10-01。来源是本次对话中用户提供的实板输出与确认；
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
ZeroMQ 或业务服务进程。Qt、media_srv、vehicle_core、rpmsg_srv、RTOS业务
仍未由本轮实现或验证。厂商例程、完整SDK、模型和运行库仍需逐项获取/核验。
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
