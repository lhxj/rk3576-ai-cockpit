# 当前状态：事实、约定和未知项

整理日期：2026-10-01。来源是本次对话中用户提供的实板输出与确认；
不是本包生成过程对实体板的实时读取。历史日志日期可能受板端时钟影响。
新增记录注明时间、命令、实际输出、版本与证据位置。

**2026-10-01 AMP 更新（新证据）：** 固定 RK3576 RT-Thread/HAL 候选已完成 Host 构建；派生 M0 最小 RPMsg echo clean build 通过。上板前等级仍为 **C. HOST_BUILD_PASS**：候选 FIT `0x47800000` 与参考 RPMsg vring 同址，BUS M0 shared DDR remap 未证明；当前板无 `amp` 分区与 AMP/RPMsg DT 节点。只读 boot/DTB/分区盘点和所有 blocker 详见 [preboard SUMMARY](reviews/rk3576-amp-preboard/SUMMARY.md)。以下历史段落保留其当时证据层级。

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

本启动包仅提供规则、任务、README、配置样例、只读脚本和host构建烟测。
Qt、media_srv、audio_srv、voice_srv、infer_srv、vehicle_core、rpmsg_srv、
RTOS业务都**尚未由本包实现或验证**。厂商例程、完整SDK与参考工程尚需获取/核验。
所有PASS指此前用户的具体测试，不代表当前Codex可以跳过盘点。

## 8. AMP Host 候选包（2026-10-02）

**C. HOST_BUILD_PASS；尚未上板。** RK3576 BUS M0最小echo的独立冷启动proposal已clean Host构建（122,696B、无warning），包含cache bypass和有界payload地址转换；Linux pool/barrier、U-Boot reset/SiP错误传播patch与生成式contract已形成。完整候选Kernel Image/modules/v2DTB、echo ko、U-Boot bin、Host overlay/FIT构建通过，Host一致性/CI通过。

普通只读SSH已复制boot原件和运行DT到Host，已有Image/uEnv/DTB/hash；不是完整eMMC备份。当前CON值、板上AMP加载/签名入口、动态SMC、实际UART5和完整恢复介质仍需证据，**无D、无部署授权**。详见 [P023交接](reviews/rk3576-amp-platform-closure/HOST_PREBOARD_PACKAGE.md)。本包没有新增sensor/vehicle业务或验证实板RPMsg。

## 9. AMP恢复文件/Windows工具（2026-10-03，Host）

完整官方Debian12 GNOME 20260424 `update.img`已取得，整文件发布MD5、内部content MD5和SHA256校验通过。解析器exit0、标准dumpimage/debugfs只读提取后逐字节比较：恢复包U-Boot/BL31/Kernel/DTB/boot脚本与前轮板端原件一致。原件同时复制到桌面并核hash。Windows RKDevTool3.32已解压，Rockusb5.14目录签名Valid，PnPUtil预装exit0并枚举为oem71.inf；没有请求重启。

本轮未访问开发板或运行镜像代码。恢复介质文件缺口已关闭；USB/MR实际识别、用户数据备份、实际AMP boot入口/验签/动态SMC及最终可部署changeset仍未闭合，**C. HOST_BUILD_PASS**。详见 [P024恢复核验](reviews/rk3576-amp-platform-closure/RECOVERY_IMAGE_ANALYSIS.md)、[Host工具](reviews/rk3576-amp-platform-closure/HOST_RECOVERY_TOOLS.md)。Host CI：2项CTest、22项Python PASS。
