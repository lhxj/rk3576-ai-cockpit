# 当前状态：事实、约定和未知项

**P029 Host准备完成（2026-10-03）：** 已交付A/B/C独立DT/脚本、配套Linux/initrd/模块、有界M0诊断FIT和回滚指南；21文件桌面SHA复核PASS。fresh M0 build零warning，67脚本/包 +14实际M0 C +6snapshot故障项、实际P028 preload/libfdt及full Host CI PASS。只Host，未访问实板；A暂存/配套Linux、B/C有效mapping/cache/RPMsg均UNVERIFIED，未取得本轮部署批准。用户只有一个Debug USB-TTL，A足够，B/C UART5采集待落实。**C. HOST_BUILD_PASS，D关闭**。 见 [审核与下一步](reviews/rk3576-amp-platform-closure/P029_HOST_PREPARATION_REVIEW.md)。以下保留历史证据及其当时状态。

**P028新Linux启动PASS（2026-10-03）：** 完整8MiB新P028此前读回与获批SHA9bd8cc03...71c6d5e逐字节相同；用户新串口确认 proper U-Boot `f8b4554`、banner后运行时policy=0、原 `/boot.scr` 与 `booti`，随后原 `6.1.99-rk3576 #8` 到Debian12登录。新proper策略调用和原厂Linux启动兼容缺口闭合；补充日志确认Ctrl+C进入CLI、help显示amp_m0load用法、手动boot及uname/cmdline身份PASS；help没有加载/启动M0。不是AMP FIT/M0或所有外设测试。M0有效映射/reset/cache、BL31 MCU setter与配套RPMsg仍UNVERIFIED；**C. HOST_BUILD_PASS，D关闭**。Agent只Host解读与归档。见 [P028实板结果](reviews/rk3576-amp-platform-closure/P028_LINUX_ONLY_BOARD_RESULT.md)。以下保留此前各阶段的待审批/失败/读回历史。

**2026-10-03 15:09 P028完整读回PASS：** 用户截图Uboot已改选新P028，导出成功；Host实读新ExportImage.img=8388608B，与获批候选逐字节相同，SHA=`9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e`，已独立保存忽略目录。**新candidate readback=PASS；新Linux冷启动尚UNVERIFIED**，可按既有有限批准由用户正常冷启动，M0关闭。此前14:43原件读回及随后原件路径截图保留历史，不将失败记录改写为PASS。Agent只Host校验，整体C。见 [执行历史和当前读回](reviews/rk3576-amp-platform-closure/P028_LINUX_ONLY_EXECUTION.json)。

**2026-10-03 P028读回未匹配新候选：** 用户导出成功，Host实读14:43更新的ExportImage.img=8388608B，SHA=`ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`，逐字节匹配原件；预期新P028为9bd8cc03...71c6d5e，未匹配。**新candidate写入未证明、读回BLOCKED、Linux冷启动未验证**。停止新候选冷启动/重复写入，等待下载镜像页所选文件名及完整写入日志，原因尚UNVERIFIED。不能归因用户或认定其他分区状态。Agent只Host分析，整体C不变。见 [实际读回](reviews/rk3576-amp-platform-closure/P028_LINUX_ONLY_EXECUTION.json)。

**2026-10-03 P028测试批准：** 用户明确批准仅更换新P028 U-Boot并验证原Linux、M0保持关闭；候选8MiB/hash9bd8cc03...71c6d5e与原件/MiniLoader在Host复核一致。仅指导用户手工MASKROM/RAM Loader→现有EMMC uboot→读回→原Linux冷启动。**新写入/读回/启动未观测**，C不变、D与AMP部署门关闭，不扩大到boot/GPT/IDBlock/M0。见 [审批与执行状态](reviews/rk3576-amp-platform-closure/P028_LINUX_ONLY_EXECUTION.json)。

**2026-10-03 P028 Host修复交付：** 原Linux恢复PASS后，主控集成0009/Host build/test并独立clean构建，181项factory script/boot policy检查、FIT/preload及full CI PASS。新8MiB候选仅用于另行审批的Linux-only兼容，SHA9bd8cc03...71c6d5e；BL31/TEE/control DT与实际原件相同。不把Host成功当作新镜像实板启动成功，D关闭、C保持。见 [审核与待审批物料](reviews/rk3576-amp-platform-closure/P028_HOST_REPAIR_REVIEW.md)。本轮只Host操作，未访问实板。

**2026-10-03 P028恢复进展：** 用户经MASKROM RAM Loader读取GPT、仅写EMMC的uboot原8MiB后，按起始/数量均0x4000扇区只读导出。Host实际读取新导出文件，size=8388608、SHA256=`ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`，与原备份完全一致，已另存忽略的Host证据目录。**ORIGINAL_UBOOT_READBACK=PASS；ORIGINAL_LINUX_COLD_BOOT=PASS。** 用户最新串口原始文本与明确确认：proper U-Boot `8f53f800da` → 原 `/boot.scr` → `6.1.99-rk3576 #8` → Debian 12登录。原始日志仅在忽略目录保留；不把登录成功外推为所有外设功能已回归。 旧P026候选保持撤回，D门关闭，C不变。Agent未写板/重启/M0操作；记录 [P028读回身份](reviews/rk3576-amp-platform-closure/P028_RECOVERY_READBACK.json)。下面“当前原Linux不可用”是失败时刻的历史状态；现在原Linux已恢复至登录界面。

**2026-10-03 P028紧急更新：** 用户手工写入 P026 候选后，串口确认 proper U-Boot `2314a3f` 和候选 payload hash 前缀 `6ea779087d`，但原 Linux 启动失败：`FIT: No FIT image` 后 `No CLI available`。P026/P027候选与写入指导已撤回，生产脚本在所有 I/O 前无条件停止。根因是 FIT_SIGNATURE 触发 CLI 禁用、默认 boot_fit 与 legacy boot.scr 禁用三项兼容问题。用户截图确认 MASKROM 可识别，正按 RAM-only LoaderToDDR 前置步骤恢复；**原 U-Boot 恢复未观测、当前原 Linux 不可用**。子代理仅在独立 Host 源码修复。整体仍 **C. HOST_BUILD_PASS**，D门关闭，无 M0 启动证据；Agent没有写板/重启。见 [P028失败与恢复](reviews/rk3576-amp-platform-closure/P028_LINUX_ONLY_FAILURE.md)。以下历史记录保留其当时范围。

**2026-10-03 P026最新AMP证据：** 用户批准中午12:00前受控取证；原厂OP-TEE只读command5返回4B flag0，原8MiB U-Boot已复制并核六个payload hash，UART5两脚MUX/GPIO UNCLAIMED。原firmware/boot/modules及MiniLoader另存Windows恢复目录并核hash。独立子代理修复复制前完整code/shared预留、Linux Image/initrd/DT防覆盖与no-map检查，主控阅读/复测；无GPT改动的显式boot文件入口、fresh独立Linux release/255modules/配套initrd和精确rollback已形成。整体仍 **C. HOST_BUILD_PASS**：实际映射/reset/cache及新Ub/MCU setter动态能力未证明，不启动M0。未修改boot/GPT/OTP/MMIO、未加载KO/重启；唯一新板端程序是获批RAM临时只读诊断，已清理。见 [P026结论](reviews/rk3576-amp-platform-closure/P026_CLOSURE_RESULT.md)。以下为历史，不把旧“缺备份/分区方案/flag”当当前完成事实。

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

## 10. 整机恢复实测用户确认（2026-10-03）

用户明确回复 **“整机恢复实测通过”**。记录 `WHOLE_BOARD_RECOVERY_TEST=PASS`，证据等级 **BOARD_OBSERVED_USER_REPORT / USER_CONFIRMED**；无实际刷写命令、工具日志或恢复后 hash 提供，不能写作 Agent 执行/观测。无需重复整机恢复演练。具体模式和实际刷入镜像未提供，旧 boot 备份 hash 不自动当作恢复后当前状态。

本次 Agent 仅更新文档，无板端访问、部署或 M0 启动。后续只读盘点应重新核恢复后的版本/DT/分区/boot hash。AMP 专用精确 rollback、用户数据备份及 boot/映射/coherency 仍需闭合，整体 **C. HOST_BUILD_PASS**。见 [恢复实测证据](reviews/rk3576-amp-board-evidence/RECOVERY_USER_CONFIRMATION.md)。

## 11. P025：恢复后只读基线、AMP验签与changeset（2026-10-03）

普通SSH exit0：恢复后Kernel`6.1.99-rk3576 #8`、U-Boot`8f53f800da-04/24/2026`/BL31`v1.14`、v2 model及六项boot hash与原件一致，重新复制原boot/运行DT；仍无amp GPT/AMP DT。已有完整恢复包和用户整机恢复PASS无需重复。

Host无重叠地址/cache bypass+Linux uncached pool方案复核PASS（9672检查）；新增required-conf-key/FIT policy和partition边界U-Boot派生补丁，signature-enabled完整fresh build、原BL31/OPTEE/DT保留封装及46+3条件测试PASS。产物未签名、未上板。机器changeset核文件身份PASS、准入检查BLOCKED/exit2；真实验签policy、amp GPT/数据计划、复制前RAM保护、动态SMC/mapping/cache等仍需闭合，整体 **C. HOST_BUILD_PASS**。

见 [P025结果](reviews/rk3576-amp-platform-closure/P025_CLOSURE_RESULT.md)、[部署回滚清单](reviews/rk3576-amp-platform-closure/P025_DEPLOYMENT_AND_ROLLBACK.md)。本轮仅Host/普通只读SSH，无MMIO/SMC重试、上传候选、写板、reboot或M0启动。

用户后续完整串口日志显示SPL `Verified-boot:0`与六个原厂payload SHA检查PASS：记录的SPL FIT未要求签名（BOARD_OBSERVED_USER_REPORT）。proper U-Boot AMP验签走独立OP-TEE flag路径，日志未触发，仍UNVERIFIED；不能据SPL的0或缺security分区推定AMP允许unsigned。完整日志证据已取得，无需重复请求。见 [分阶段验签证据](reviews/rk3576-amp-board-evidence/SPL_VERIFIED_BOOT_SERIAL_EVIDENCE.md)。整体仍C，本补充只做Host分析/文档。
