# 参考代码与许可证登记

本包没有复制以下工程的源代码/模型/SDK，也不宣布这些工程已在本项目中运行。
路径、入口、模型可得性和许可证待任务P002/P004/P005实际审查。

| 来源 | 预期用途 | 当前状态 |
|---|---|---|
| CSDN black_sneak 的IMX6ULL Linux+Qt车机文章 | 应用页面与组织思路 | 用户指定参考；完整源码获取与授权待核验 |
| superxiaobai-1/LLM_Voice_Flow | 模块化C++语音流程 | 用户指定参考；具体入口/模型/依赖/授权待盘点 |
| EmbedFire / Rockchip SDK | 板级配置、ISP/MPP/RGA/NPU/AMP等 | 用户板端已有BSP，主机完整源码版本待获取 |

每次引入记录：来源URL、获取日期、commit/tag、许可证文件、拷贝范围、修改说明、
是否含再分发受限的模型/固件、允许的发布范围。
不要从“公开能下载”推断MIT/Apache许可，也不要将整个新仓库擅自声明覆盖第三方的MIT授权。
本包不替用户选择开源许可证；公开前由用户确定自有代码许可并完成第三方审查。
链接见docs/REFERENCES.md。

## 2026-10-02：LubanCat SDK 静态取证

- 来源：[LubanCat/manifests](https://github.com/LubanCat/manifests)，固定 `db55f9658b2460b40e6e873d391e86d1b29e2916`，选择历史 `lubancat_linux/lubancat_linux_generic_20260424.xml`。
- 发布 pin：U-Boot `8f53f800da2c25d0c6ba414fb45902a01675703a`、rkbin `58a39b47f77a26a1e110fa1a1ce80bcfcb0b3505`、device_rockchip `ea5af0b2e5a48cc3d225c42c717923f70b1ac03d`、kernel-6.1 `521833e2d28decbd6473d5717f1f96cc4108e208`。
- 取得范围：manifest/include、关键板型/AMP 构建配置片段、RK3576 trust 配置及预编译 BL31 ELF，均存于忽略的 `artifacts/local/`。用途限 XML/ELF 静态解析、hash/字节比较和来源调查；没有修改或执行厂商文件，没有将其 vendor 或提交到主仓。
- 许可证：本轮没有完成这些 SDK 项目和预编译固件的许可证/再分发条款审查，状态 **UNVERIFIED**；不授予再分发或修改固件许可。后续引入代码或发布固件前须核各项目许可证与厂商条款。
- 结果与范围限制见 [SDK_MANIFEST_EVIDENCE.md](reviews/rk3576-amp-board-evidence/SDK_MANIFEST_EVIDENCE.md)。
