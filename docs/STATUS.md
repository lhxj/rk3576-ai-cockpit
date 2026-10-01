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
| Git | `.git`存在，main，无commit；可能已按之前指引操作，须重新检查 | LAST_OBSERVED |
| GitHub CLI | 用户lhxj已登录，Git走HTTPS | USER_LOG |
| GitHub仓库 | 期望lhxj/rk3576-ai-cockpit，是否已创建/推送尚未核验 | UNVERIFIED |
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
| HDMI/触摸交互 | 历史显示/触摸PASS；2026-10-01只读盘点见X11活动会话与800x480 framebuffer | Qt应用尚未构建/显示；刷新率、平台插件实际加载与本UI触摸仍未验证 |
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

仓库已有host scaffold。2026-10-01新增`cockpit_ui` foundation：纯C++状态模型、
Mock backend和路由映射已Host `MOCK_TESTED`；Qt Widgets单shell与七个页面的
源码已建立，但因当前Host和板端均缺Qt开发包，Qt target未编译，状态为
`PARTIAL`。media_srv、真实audio/voice/infer/vehicle_core/rpmsg_srv、RTOS业务仍
未由这项UI工作实现或验证。所有历史PASS只对应此前具体测试条件。

## 8. 2026-10-01 IMX6ULL archive audit update

- 本地已取得并静态审查 `build-QTMenu-IMX6U_rsync-Debug.rar`，SHA256 为
  `92e571eaeb171be6dcd73c2db8e895223f2fdabd3ee863b8dc66de9f5e39462c`。
- 归档是 Qt Creator/qmake `SHADOW_BUILD`，原始 Qt 工程源码 `INCOMPLETE`，
  包内主要是 ARM32 ELF/object、Qt 生成文件、旧 Makefile 和演示媒体。
- 正式定位：`Reference Role = UI_REFERENCE_ONLY`，
  `Migration Strategy = REIMPLEMENT`，`License = LICENSE_UNVERIFIED`。
- 在该参考审查完成时，`cockpit_ui` 为`NOT_IMPLEMENTED / CONTRACT_ONLY`。
  后续foundation实现只增加自研Mock/skeleton，不使音乐、视频、传感器参考
  功能成为RK3576真实业务功能。
- 仓库记录见 `docs/reviews/reference-audit/imx6ull-qt/`。更早的架构审查在当时
  记录“RAR尚未取得”仍是有效历史事实，不回写或覆盖。

## 9. 2026-10-01 cockpit_ui foundation initial update

- 隔离分支：`agent/cockpit-ui-foundation`；未混入并行Voice/AI工作区改动。
- Host Qt盘点：`qmake`、`qtpaths`、Qt5/Qt6 pkg-config与开发头均不存在。
- 板端只读盘点：活动GNOME/X11会话，HDMI-A-1 connected/enabled，framebuffer
  800x480；Qt 5.15.8 arm64运行库存在，但无`qmake`、`qtpaths`或Qt开发元数据。
- `bash scripts/dev/host_ci.sh`退出0：3/3 CTest、6/6 Python测试通过；CMake
  明确输出`UI_HOST_BUILD_SKIPPED_QT_NOT_FOUND`。
- 已测试范围只含`UiState`默认真值、Mock请求/失败结果、导航映射、SIMULATED
  标识和preview metadata。未生成AArch64 UI，未在LubanCat显示，未触摸验收。
- 详细边界见`docs/architecture/COCKPIT_UI.md`与
  `docs/bringup/ui/qt_target_inventory.md`。

## 10. 2026-10-01 Qt development environment and target build update

- 用户随后授权安装开发环境。Host安装Ubuntu Qt 5.15.3开发包；板端安装与
  运行库匹配的Debian Qt 5.15.8 arm64开发包，均来自标准发行版仓库。
- Host实际构建Qt shell；`host_ci.sh`退出0，4/4 CTest与6/6 Python测试通过，
  包含offscreen事件循环启动测试。
- 源码只部署到`/home/cat/cockpit/ui-foundation-20261001-qtdev/`。板端使用
  GCC 12.2/Qt 5.15.8原生生成ARM aarch64 ELF，4/4 CTest通过。
- 从GNOME Shell进程读取到`DISPLAY=:0`和实际Xauthority后，Mock shell通过
  X11/xcb全屏启动并自动退出，退出码0。使用`QT_XCB_GL_INTEGRATION=none`
  避免当前Widgets界面不需要的GLX探测。
- 默认GLX探测仍打印Rockchip Mesa DRI2/DRI3驱动错误，不能宣称GPU路径正常。
  无截图工具，未安装额外工具；视觉裁剪、文字可读性和实体触摸仍待人工确认。
- 详细证据见`docs/bringup/ui/development_environment.md`。
