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
