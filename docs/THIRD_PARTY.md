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
