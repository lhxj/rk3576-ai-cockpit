# 参考代码与许可证登记

本包没有复制以下工程的源代码/模型/SDK，也不宣布这些工程已在本项目中运行。
路径、入口、模型可得性和许可证待任务P002/P004/P005实际审查。

| 来源 | 预期用途 | 当前状态 |
|---|---|---|
| CSDN black_sneak 的IMX6ULL Linux+Qt车机文章及本地RAR | 页面组织、大触控交互、媒体控制项、传感器展示思路 | 本地RAR已审查：`SHADOW_BUILD`、源码`INCOMPLETE`、`UI_REFERENCE_ONLY`、迁移`REIMPLEMENT`、`LICENSE_UNVERIFIED` |
| superxiaobai-1/LLM_Voice_Flow | 模块化C++语音流程 | 用户指定参考；具体入口/模型/依赖/授权待盘点 |
| EmbedFire / Rockchip SDK | 板级配置、ISP/MPP/RGA/NPU/AMP等 | 用户板端已有BSP，主机完整源码版本待获取 |

每次引入记录：来源URL、获取日期、commit/tag、许可证文件、拷贝范围、修改说明、
是否含再分发受限的模型/固件、允许的发布范围。
不要从“公开能下载”推断MIT/Apache许可，也不要将整个新仓库擅自声明覆盖第三方的MIT授权。
本包不替用户选择开源许可证；公开前由用户确定自有代码许可并完成第三方审查。
链接见docs/REFERENCES.md。

IMX6ULL RAR 不纳入主仓。禁止复制其中的 ARM32 ELF、object、旧 Makefile、
FSL sysroot配置、Qt生成文件、AP3216C实现、图标、歌曲和视频。文章页面可能
标注的许可与Qt自身许可不能替代RAR内容逐项授权；未来即使取得源码，也必须
分别确认代码和资源的修改、使用及再分发范围。
