# 参考代码与许可证登记

主Git仓库没有复制以下工程的源码、模型或二进制。Sherpa官方外部运行库已用于
Host和RK3576用户目录下的文件ASR测试；这不表示其进入产品发行包。
参考审查结论不等于第三方依赖已经进入产品。

| 来源 | 预期用途 | 当前状态 |
|---|---|---|
| CSDN black_sneak 的IMX6ULL Linux+Qt车机文章及本地RAR | 页面组织、大触控交互、媒体控制项、传感器展示思路 | 本地RAR已审查：`SHADOW_BUILD`、源码`INCOMPLETE`、`UI_REFERENCE_ONLY`、迁移`REIMPLEMENT`、`LICENSE_UNVERIFIED` |
| superxiaobai-1/LLM_Voice_Flow | Sherpa/RKLLM调用顺序与模块化语音流程参考 | `SOURCE_REFERENCE` / `REFERENCE_ONLY`；审查commit `be82e87cc334ae6e222f83f7555531d1ddebaa8b`；`ROOT_LICENSE_UNVERIFIED`；模型`PARTIAL`；产品决策`BUILD_OWN_VOICE_AI_STACK` |
| [k2-fsa/sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx/releases/tag/v1.11.3) | ASR-01 外部 Host / RK3576 文件ASR C API依赖 | v1.11.3；源码 Apache-2.0；官方x64 no-TTS archive SHA256 `84ce8e14e4d4aa692f8ecd9745b5a73a4b9103809cadbb7e645c0b5c9ea853bb`；官方AArch64 shared CPU archive 19,003,169 bytes，SHA256 `8dc62c081c90d30b15e0912c98863117ec61bd8b1729a3019a6ec52826a90733`。仅在忽略的Host build目录及板端 `/home/cat/cockpit/asr-target`；无源码/二进制进Git。发行包NOTICE与实际binary来源仍需发布前核验。 |
| [ONNX Runtime](https://github.com/microsoft/onnxruntime/blob/main/LICENSE) | Sherpa CPU runtime | Host与AArch64包均为 `1.17.1`，上游MIT；本轮Host/板端文件测试使用，未纳入产品发行。 |
| [Sherpa Zipformer bilingual ASR model](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-transducer/zipformer-transducer-models.html#sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16-bilingual-chinese-english) | 外部本地文件识别研究样本 | 中文/英语，约60.4 MB所选文件；来源指向[模型卡](https://huggingface.co/csukuangfj/k2fsa-zipformer-bilingual-zh-en-t)标Apache-2.0，但本地转换权重包及测试WAV没有单独许可通知：`LICENSE_UNVERIFIED_FOR_DISTRIBUTION`。不提交/不打包。 |
| [Silero VAD v5.0 ONNX](https://github.com/snakers4/silero-vad/raw/refs/tags/v5.0/files/silero_vad.onnx) | Sherpa v1.11.3 VAD 文件/实时链外部模型 | 2,313,101 bytes，SHA256 `6b99cbfd39246b6706f98ec13c7c50c6b299181f2474fa05cbc8046acc274396`；[同一v5.0 tag的LICENSE](https://raw.githubusercontent.com/snakers4/silero-vad/refs/tags/v5.0/LICENSE)为MIT，状态`MIT_LICENSE_OBSERVED_NOTICE_PENDING`，产品打包仍须保留通知并复核。只在忽略的Host build与板端用户目录，未进Git。 |
| EmbedFire / Rockchip SDK | 板级配置、ISP/MPP/RGA/NPU/AMP等 | 用户板端已有BSP，主机完整源码版本待获取 |
| Rockchip MPP (board Debian delivery) | CAM0 H.264 hardware encode | `librockchip-mpp1`/`-dev`/demos 1.5.0-1 arm64；runtime自报commit `43a191ed`，pkg-config自报1.3.9；动态库SHA256 `1aca0bed4ba184f5fef4841e381e8b9918983df02ebfd8c3983c6919acdc8bc5`。上游Apache-2.0、Debian packaging GPL-2+；系统包未复制进Git。版本元数据差异及依赖见`docs/bringup/media-recording/MPP_ENVIRONMENT.md`。 |
| [Rockchip RKNN Toolkit2 v2.3.0](https://github.com/airockchip/rknn-toolkit2/tree/v2.3.0) | RK3576 MobileNetV1 runtime/header/model sample | 板端`librknnrt.so` 2.3.0、driver 0.9.8；模型`mobilenet_v1.rknn` SHA256 `bc66943ea85ec0dd8a04da22c4276bfc8a4c6fe24f5ea8be7a1e5c3c22c8259d`与官方v2.3.0 Git blob一致。Toolkit LICENSE/header为Rockchip proprietary/all-rights-reserved；外部系统依赖，不进入Git，`REDISTRIBUTION_RESTRICTED_REVIEW_REQUIRED`。不要由model zoo的Apache-2.0推导本样本模型已重许可。 |

每次引入记录：来源URL、获取日期、commit/tag、许可证文件、拷贝范围、修改说明、
是否含再分发受限的模型/固件、允许的发布范围。
不要从“公开能下载”推断MIT/Apache许可，也不要将整个新仓库擅自声明覆盖第三方的MIT授权。

LLM_Voice_Flow 的作者集成胶水、麦克风循环、ZMQ协议、TTS队列/服务、ALSA播放器、RKLLM demo包装及RK3588导出配置均不直接复用；其根目录无统一LICENSE。ASR资产与单个TTS模型在参考仓，RKLLM权重不在；代码、权重、运行库和数据集许可分别核验。证据见 `docs/reviews/reference-audit/llm-voice-flow/`。
本包不替用户选择开源许可证；公开前由用户确定自有代码许可并完成第三方审查。
链接见docs/REFERENCES.md。

IMX6ULL RAR 不纳入主仓。禁止复制其中的 ARM32 ELF、object、旧 Makefile、
FSL sysroot配置、Qt生成文件、AP3216C实现、图标、歌曲和视频。文章页面可能
标注的许可与Qt自身许可不能替代RAR内容逐项授权；未来即使取得源码，也必须
分别确认代码和资源的修改、使用及再分发范围。
## 2026-10-02：LubanCat SDK 静态取证

- 来源：[LubanCat/manifests](https://github.com/LubanCat/manifests)，固定 `db55f9658b2460b40e6e873d391e86d1b29e2916`，选择历史 `lubancat_linux/lubancat_linux_generic_20260424.xml`。
- 发布 pin：U-Boot `8f53f800da2c25d0c6ba414fb45902a01675703a`、rkbin `58a39b47f77a26a1e110fa1a1ce80bcfcb0b3505`、device_rockchip `ea5af0b2e5a48cc3d225c42c717923f70b1ac03d`、kernel-6.1 `521833e2d28decbd6473d5717f1f96cc4108e208`。
- 取得范围：manifest/include、关键板型/AMP 构建配置片段、RK3576 trust 配置及预编译 BL31 ELF，均存于忽略的 `artifacts/local/`。用途限 XML/ELF 静态解析、hash/字节比较和来源调查；没有修改或执行厂商文件，没有将其 vendor 或提交到主仓。
- 许可证：本轮没有完成这些 SDK 项目和预编译固件的许可证/再分发条款审查，状态 **UNVERIFIED**；不授予再分发或修改固件许可。后续引入代码或发布固件前须核各项目许可证与厂商条款。
- 结果与范围限制见 [SDK_MANIFEST_EVIDENCE.md](reviews/rk3576-amp-board-evidence/SDK_MANIFEST_EVIDENCE.md)。

## 2026-10-02：U-Boot 启动修复草案

- 来源：LubanCat/uboot 固定 `8f53f800da2c25d0c6ba414fb45902a01675703a`；独立派生 `7daeb0fc8ad0818a833b162405a35d0511d767bb`。
- 复用范围：主仓只保存四个文件的 format-patch 和审查/Host 检查记录，不 vendor 完整 SDK，不提交厂商预编译固件。对象文件仅留忽略目录。
- 文件原声明：`drivers/cpu/rockchip_amp.c` 为 GPL-2.0；`arch/arm/mach-rockchip/{board.c,rk3576/rk3576.c}`、`include/amp.h` 为 GPL-2.0+；Rockchip copyright 与声明保留。总说明 `Licenses/README`、许可正文 `Licenses/gpl-2.0.txt`。
- 修改：自定义 FIT window-base 参数、SMC 返回值检查、standalone 失败传播；不是厂商发布补丁。供私有审查，发布或分发修改后 U-Boot 时须处理对应 GPL 源码/声明要求，不能用主项目其他许可证覆盖这些文件。
- 对固定 device_rockchip 的 `Config.in.loader`、`mk-loader.sh` 仅静态读取并核 Git blob；未运行，未将源码复制到主仓。其整体许可证审查仍 UNVERIFIED。

## 2026-10-02：Linux RPMsg 屏障草案 / cache 静态证据

- 来源：LubanCat/kernel 固定 `521833e2d28decbd6473d5717f1f96cc4108e208`；15 个 mapping/virtio/barrier 文件固定 URL 与 SHA256 见 [SHARED_MEMORY_CACHE_RESULT.json](reviews/rk3576-amp-board-evidence/SHARED_MEMORY_CACHE_RESULT.json)。这些文件只保存在忽略的 Host 分析目录；没有 vendor 完整源码。
- 主仓复用：`patches/rk3576-amp-platform/0002-linux-rpmsg-use-device-barriers-draft.patch` 仅包含 `drivers/rpmsg/rockchip_rpmsg_mbox.c` 的最小上下文和 weak-barriers 参数修改；原文件 `SPDX-License-Identifier: GPL-2.0` / Rockchip 2022 copyright 保持。该 patch 是本项目审查草案，不是厂商发布版本。
- Host 编译使用该固定文件及 `drivers/rpmsg/rpmsg_internal.h`，对象只保留在忽略目录，不生成 ko、kernel 或可部署包。运行 Image 对应完整源码仍未闭合；不能据对象编译 PASS 声称有实板加载授权。
- 分发修改后的 kernel/模块时须处理 GPL 对应源码和声明等要求。原始 TRM / BL31 文件只静态解析，不提交或授予再分发许可；本轮不作法律保证。

## 2026-10-02：P023 Host proposal

- 固定RTOS/HAL reference不改；主仓保存RTOS/HAL/U-Boot format-patch、Linux单文件patch、配置/审查/自写检查器，不vendor完整SDK。派生SHA和复用文件范围见 [HOST_PREBOARD_PACKAGE.md](reviews/rk3576-amp-platform-closure/HOST_PREBOARD_PACKAGE.md)。
- RTOS/应用/linker Apache-2.0、RK3576 RPMsg platform和HAL相关文件的BSD-3-Clause原声明保持；以此前 [LICENSE_AUDIT.md](reviews/rk3576-amp-bsp-audit/LICENSE_AUDIT.md)的逐项范围为准，不为整厂商demo统一改许可证。Linux GPL-2.0、U-Boot GPL-2.0/GPL-2.0+原声明保留。
- LubanCat/kernel完整固定源码tar仅在忽略目录；SHA256 `49907db91253814952e42a441acbfc20ffbfeca4f55c0ef1e7dc3d99998697eb`。只修改transport，Host构建产物不提交；发布时处理相应GPL要求。
- 用户硬件PDF、TRM、恢复archive及厂商预编译firmware不提交；仅登记hash和取证结论，没有授予再分发许可。Ubuntu官方flex/bison/libelf/m4标准包核APT hash后仅任务目录解包使用，无全局安装。工具package/hash见本轮manifest。

## 2026-10-03：P024恢复镜像与PC工具

- 官方野火Debian12 GNOME 20260424 update.img由用户提供；发布MD5匹配。原件和提取payload只保存在用户桌面/忽略目录；主仓只保存hash、格式/版本取证和自写解析器，不再分发预编译固件。[镜像/来源与范围](reviews/rk3576-amp-platform-closure/RECOVERY_IMAGE_ANALYSIS.md)。
- RKFW/LDR格式参考Rockchip rkdeveloptool固定`304f073752fd25c854e1bcf05d8e7f925b1f4e14`，RKImage/RKBoot.cpp声明GPL-2.0+；仅阅读其packed协议和边界，不复制/运行厂商工具实现。RKAF固定表另参考独立neo-technologies/rockchip-mkbootimg固定`2348690523faee6ce3cea9eb9ff47e8b8d5e1df6`的rkafp.h，不当作官方SDK或安全依据；该项目整体许可尚未闭合，不vendor源文件。自写解析器不支持其未证明的64位item扩展，不提取rootfs。
- SDK rkbin固定DDR v1.09 bin与loader USB entry前缀匹配；只静态比较，文件未执行/提交。预编译固件修改和再分发条款仍UNVERIFIED。
- 用户要求Host安装后，下载Radxa官方HTTPS目录分发的Rockchip RKDevTool3.32和DriverAssitant5.14；保留zip/版本/hash。RKDevTool免安装，未运行；微软签名Rockusb CAT Valid，Windows原生PnPUtil仅预装x64 driver，exit0。没有使用ADB、卸载驱动、关闭验证或重启。许可证/再分发范围不作保证，工具没有进入Git。[工具与签名记录](reviews/rk3576-amp-platform-closure/HOST_RECOVERY_TOOLS.md)。

## 2026-10-03：P025 FIT-policy派生及Host package

- 固定LubanCat/uboot `8f53f800da2c25d0c6ba414fb45902a01675703a` → P023 `87f467be568f1189dce4b6eb65ab138279311984` → 本轮派生 `96c9a009eed997c318cd247943e5fc88e8e1bcc6`。主仓仅保存`0007`两commit format-patch、审查/自写检查/封装脚本；原`drivers/cpu/rockchip_amp.c` GPL-2.0/Rockchip声明保留。修改不是厂商发布补丁；分发修改后bootloader时须处理对应源码/许可要求。
- 原恢复固件ATF/OPTEE/控制DT用于字节保留的Host package，全部只留忽略目录；未签名/执行/上传，未授予预编译固件再分发或修改许可。完整hash/source/命令见P025 manifest。
- 本轮旧boot/运行DT的只读副本/日志只存本地忽略目录；主仓只保存去内容的identity/hash与结论，不提交账号凭据、完整板端日志或firmware blob。

## 2026-10-03：P026 preload、只读取证及paired kernel

- 0008项目补丁基于P025固定派生96c9a009，最终2314a3f9；涉及sysmem、FS/FIT/booti/initrd/FDT及AMP命令，原U-Boot GPL/Rockchip声明保留。自写contract生成器和Host harness只在Host执行提取出的C函数；没有把厂商firmware作为可执行测试。
- Linux继续固定521833e2及0003 transport patch，独立LOCALVERSION官方Kconfig生成；fresh Image/255 modules/initrd只在忽略目录。原initrd脚本和厂商boot payload不进入Git或授予再分发许可；未来分发对应kernel/模块/bootloader需处理完整对应源码及声明。
- 自写只读TA诊断使用固定U-Boot厂商TA UUID/command5和Linux tee UAPI，源码可审核；板端只读调用经用户授权一次，临时文件清理。主仓记录ABI核验/hash与脱敏结果，不提交原日志/firmware/账号。
- 随RKDevTool3.32提供的Rockchip公开《开发工具用户手册V1.0》仅静态取证；文档始于2.88，不能外推其generic步骤已在RK3576单分区实测。手册不提交，只登记SHA/页/章节。第三方license/来源原审查保留，本轮不作法律保证。

## 2026-10-03：P028 factory Linux启动兼容

- 基于LubanCat U-Boot 8f53f800da、P026派生2314a3f，独立派生f8b4554584dd475ce783c605850c5e883b0a0fd4。0009统一diff保留GPL-2.0+声明；新内部policy与专用SCRIPT parser是项目补丁，不是厂商发布。
- 自写Host harness提取固定vendor C/image CRC/libfdt与实际ELF默认env；TA/command/hardware边界是stubs。不vendor完整源码，不提交firmware或原板日志。
- Host封装保留实际原板BL31/OPTEE/control DT payload；新候选仅在忽略Host目录与用户桌面，等待单独Linux-only测试审批。分发修改后的U-Boot仍须处理相应GPL源码义务。


## 2026-10-03：P029分阶段Host测试包

- 继续使用上述固定RTOS/HAL、Linux、P028 U-Boot来源；0010只修改派生应用amp_echo.c的有界诊断，保留该文件Apache-2.0声明。主仓只保存patch/自写脚本/脱敏hash与结论，不vendor完整SDK或二进制。
- TRM §8.3.3缓存RW定义和固定HAL普通RMW仅用于M0本地CACHE_CTRL取证，不外推SYS_SGRF/Linux访问。TRM PDF不提交。
- paired Image/255modules/initrd、官方CAM0 overlay、新M0/FIT只在忽略目录及用户桌面私有测试包。未授予厂商预编译文件再分发许可；未来发布kernel/模块/U-Boot仍处理对应GPL源代码/声明。RTOS/HAL既有Apache/BSD文件声明与范围保留，本轮未增加第三方库或全局安装。

## 2026-10-04：P030最终设备树保护修复

- 固定上述P028派生f8b4554584dd475ce783c605850c5e883b0a0fd4 → 本轮149b1c53e368a0d77e542cfdf3bed6db3374682a。新 `0010-uboot-final-memory-bank-padding-and-tuples.patch` 只改项目guard，保留Rockchip/GPL声明；与前一RTOS诊断0010不同文件。完整SDK/firmware不进入Git。
- 自写Host回归提取实际vendor arch/fdt packing/final prep/guard和真实libfdt，硬件/LMB/board边界明确stub。封装仅保留已实测公钥DT/签名AMP/原BL31与TEE，proper为对应新源码clean build；私钥不读取/不复制，所有二进制只在忽略目录和用户桌面。既有许可与后续对应源码义务继续适用。

## 2026-10-04：P030 M0 初始化诊断

- 0011-rtthread-mbox-client-pointer.patch 与 0012-m0-init-checkpoints.patch 仅修改固定 RT-Thread 派生源码：前者将栈上 mailbox client 指针数组改成逐次设置的标量指针；后者在既有 echo app 添加初始化、首个 link probe、首个 mdelay 与 RT tick 观测。保留上游各文件 SPDX/copyright 声明；完整 SDK、ELF/BIN、FIT 与本地开发私钥不进入 Git。
- 新 FIT 使用现有 P029 开发测试签名链并由匹配的固定 vendor verifier 检查；私钥留在本地忽略目录。验签只证明本地容器与 payload 完整性，不代表目标板硬件验签、M0 运行、有效 mapping 或 RPMsg 已验证。后续分发 RT-Thread/HAL 修改、U-Boot/内核或厂商二进制时，仍须遵循对应来源与许可要求。
- source-fix-v2 只修正项目生成的 legacy SCRIPT 长度表结束项及 CRC；实际 U-Boot bounded parser、FIT、M0 源码与二进制保持。Host 回归复用固定 vendor source/image/libfdt 函数并明确以 stub 捕获命令，保留原许可与来源；不将 Host parser 通过表述为板端执行通过。

## 2026-10-04：P030 SysTick 局部诊断

- 0013/0014 仅修改固定 RT-Thread evb board/app，保留原版权与 SPDX；记录 HAL 返回值及 MCU-local SysTick/SCB/handler/ISR/tick，有次数上限并失败停止，不改 vendor HAL/共享时钟/映射。新签名 FIT 使用现有本地开发测试 key，私钥不交付。Host 编译/验签/被动读回不代替实板 tick 或完整 B 通过。

- 0015继续基于同固定RTOS/HAL，保留board/hal_conf版权和Apache-2.0声明；SDK同SoC vehicle-evb的RT_USING_32K_TICK_SRC与hal_conf 32768为候选配置依据。新UART5物理TX参照及条件SysTick本地reload是项目实现，没有复用外部库或修改固定HAL。Rockchip TRM V1.2 Part1官方作者文档镜像仅尝试下载（超时且不完整），不提交、不作为实际输入频率证据。完整SDK/二进制/开发私钥仍只在忽略目录或用户Windows私有交付目录。

## 2026-10-04 P030新版C验证准备

- 新C复用已实测v5 signed FIT以及既有P026配套Linux/C DT/GPL-2.0 echo KO字节，未重建或修改这些第三方/派生产物。依据固定kernel521833e2和既有0003 patch的实际RPMsg buffer/driver日志与模块源码，生成项目C脚本、manifest和仅只读运行检查；不vendor SDK，不交付私钥，不把准备或设备注册表述为实际通信通过。
- 被动安装器由既有已审查项目helper派生，仅改变C目录/文件白名单/manifest/id/scope/pin/CLI；保留原default/U-Boot/factory/旧tree/metadata/原子no-replace/cleanup保护。二进制仍仅在忽略目录及用户Windows私有目录，沿用此前来源与许可、后续分发对应源码义务。

## 2026-10-04：AMP_RPMSG_INTEGRATION_TIP 源码整理

- 本轮仅导出已实际使用的固定派生Git diff：LubanCat U-Boot vendor8f53f800→149b1c5；本地RT-Thread SDK import8541f7a→3a39b0f，再叠加原0010-m0至0015；本地HAL import277de3f→bc99978。import SHA是本地导入对象，不冒充厂商公开提交。原声明与既有Apache-2.0/BSD-3-Clause/GPL文件范围保持；累计export排除生成BIN，不vendor全SDK。
- DTS transport overlay、v5 signing ITS输入和C command是既有项目配置的精确复制；源码基线、顺序、hash在patches/rk3576-amp-integration/series.json。没有新第三方库、固件重建、私钥读取/复制或全局安装。旧分拆patch与新累计patch不双重应用。
- 既有GPL Linux/KO/U-Boot配套产物保持私有本地/Windows交付，原二进制与预编译BL31/TEE不进入Git；此前对应源码/声明与预编译文件范围约束不变。真实C收发证据已取得，旧“未上板”段落仅是当时历史。没有新增RTOS业务或UI/Voice/Media代码。

## 2026-10-05 MPU I2C9 BSP 最小派生补丁

`patches/mpu6050/0001-i2c9-deferred-held-clock.patch` 仅修改既有本地 RT-Thread/Rockchip BSP 四文件（drv_i2c.c/h、evb/iomux.c、MCU drivers/Kconfig），RTOS3a39b0f+0010..0015/v5、HALbc99978，保留原 Apache-2.0 版权/SPDX。测试直接编译临时应用补丁后的驱动并用项目 Fake RTOS/HAL hooks，不 vendor SDK。完整 SDK 副本、ELF/BIN/map 只在忽略目录；无二进制再分发、新第三方库或板端安装。完整来源与 hash 见 docs/bringup/mpu6050/I2C9_ADAPTER_BUILD.json。
