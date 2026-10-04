> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 完整候选Kernel构建记录

2026-10-02，HOST_TESTED，**不是当前Image的逐字节重建或实板部署**。

- 来源：LubanCat 20260424 manifest pin `kernel@521833e2d28decbd6473d5717f1f96cc4108e208`；固定codeload tar SHA `49907db91253814952e42a441acbfc20ffbfeca4f55c0ef1e7dc3d99998697eb`。安全解包90,870个成员，源树只改RPMsg transport（0003）。
- 起点：本轮只读boot原件中的`config-6.1.99-rk3576`，保存`board-original.config`；新目录O=`artifacts/local/p023-kernel-full-clean-v3`，不复用旧build。
- 工具链：Ubuntu AArch64 GCC11.4.0/binutils2.38。缺少的flex/bison/libelf/m4标准包核Ubuntu APT索引SHA后解包任务目录；`PATH/M4/BISON_PKGDATADIR/CPATH/LIBRARY_PATH/LD_LIBRARY_PATH`只用于本次进程。没有Host全局安装和配置修改。
- 命令：`make O=... ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig`；`make -j4 Image modules rockchip/rk3576-lubancat-3-v2.dtb`；复制项目echo源码到新M目录，`make O=... M=.../echo ... modules`。三阶段均exit0；完整Kernel与新echo日志无`warning:`/`error:`。
- Image SHA `688561ba8f1bc4c201e21126a5907c8ab397c2d69ec0e7860401be32caa619bf`；新echo ko `3d06c2c0427cc66d0b7e9b7bc079b1129e8a8d50f840a16118eff87d6a18023c`。
- HOST_TESTED：新原版v2DTB SHA=`76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90`，与板原件相同；完整Kbuild Module.symvers SHA=`bebc886c6f4ef09ae0d38fc077cb25877f1d70c32e760736814bd032d596b19c`，与板headers副本相同。

## olddefconfig 的变化

13项由工具能力改变：AS/LD `23601→23800`、GCC `100301→110400`与CC_VERSION_TEXT；新增CC_HAS_ASM_GOTO_OUTPUT/TIED_OUTPUT、CC_HAS_KASAN_SW_TAGS、CC_HAS_ZERO_CALL_USED_REGS、GCC_ASM_GOTO_OUTPUT_WORKAROUND、HAVE_KCSAN_COMPILER；GCC_PLUGINS `y→n`（Host未提供plugin headers）、PAHOLE_HAS_SPLIT_BTF `y→n`与PAHOLE_VERSION `121→0`。本次没有为“看起来一致”手改配置，也没有启用I2C/相机/音频等额外业务选项。

因此这份Image是**新工具链/生成配置的完整未来候选**，不是原镜像。匹配DTB和Module.symvers提高了SDK/ABI对齐证据，但不能单独证明运行Image的完整source/build identity。当前headers编译的ko与新Kernel编译的ko有不同hash，manifest明确区分；尚未有任何ko上板或加载。
