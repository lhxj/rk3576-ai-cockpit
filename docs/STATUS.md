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
验证；vehicle_core 的后续 Host foundation 见第10节。厂商例程、完整SDK、模型和运行库仍需逐项获取/核验。
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

## 10. 2026-10-01 Vehicle Core foundation update

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

## 11. 2026-10-01 cockpit_ui foundation update

- UI成果来自独立分支 `agent/cockpit-ui-foundation`（`58f69e5`），现已通过普通
  Git merge引入integration分支；Voice/Vehicle Core基础模块均保留。
- UI为Qt 5.15 Widgets单shell，包含Home、Camera、Media、Vehicle/Sensor、AI、
  Monitor、Settings、统一`UiState`和可保留的`MockUiBackend`。
- Host Qt构建和offscreen测试已有通过证据。LubanCat-3上的Qt 5.15.8、GNOME/X11、
  800x480全屏启动、页面导航和实体触摸由用户确认通过；运行需保持
  `QT_XCB_GL_INTEGRATION=none`，该workaround不证明GPU/GLX路径正常。
- 页面骨架与触摸PASS不等于Camera、Media、Voice、RTOS或Sensor真实业务PASS。
  本integration任务将在Host测试后重新生成本分支的AArch64与板端证据。

## 12. 2026-10-01 UI + Vehicle Core integration update

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

## 13. 2026-10-02 CAM0 Media/Core integration update

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
  自动运行不能代替当前二进制的实体触摸/视觉验收，因此T7仍待用户确认。
- T8有界300秒运行完成8,960帧、29.8757 fps、Qt交付13.4174 fps，sequence gap、
  poll timeout、DQBUF/QBUF error、mailbox/UI coalescing drop均为0；退出后无残留进程/
  Camera owner，项目backend可重新打开设备。该结论不是长期稳定性或热认证。
- 原生AArch64 build通过，默认CTest 19/19通过。Host ASan+UBSan下MediaService与Core
  2/2（含LeakSanitizer）通过；Qt offscreen在关闭LSan时也通过，启用LSan只报告Qt
  offscreen/fontconfig退出时656字节框架分配，没有ASan/UBSan越界或UAF报告。
- 证据与边界见`docs/architecture/MEDIA_CAM0_PIPELINE.md`及
  `docs/bringup/media-cam0/`。当前等级在T7人工确认前最多为`MEDIA_CAM0_CORE_PASS`。
