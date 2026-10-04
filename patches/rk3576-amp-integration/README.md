# 已实测 AMP/RPMsg 最小链源码索引

本目录是现有已用源码的累计 diff 导出，未改变运行代码。当前事实和身份见 [AMP_RPMSG_INTEGRATION_TIP](../../docs/amp/AMP_RPMSG_INTEGRATION_TIP.md)，精确 base/derived SHA、顺序和文件 hash 见 [series.json](series.json)。

## 组件顺序

1. U-Boot：在固定 vendor8f53f800 上应用累计 `uboot-8f53f800-to-149b1c5.patch`。等价于旧分拆 U-Boot 补丁链，不再叠加旧0001/0006/0007/0008/0009/0010。
2. HAL：固定 SDK import277de3f + `hal-277de3f-to-bc99978.patch`。
3. RTOS：固定 SDK import8541f7a + `rtos-import-to-3a39b0f-source.patch`，再按 series.json 顺序应用原 `0010-m0` 到 `0015`；HAL symlink 显式指向步骤2源码。累计 base 包含早期 UART5 M0 pinmux 与 preboard echo/0004，不能重复应用。
4. Linux：固定 kernel521833e2 + 原0003 transport补丁；0002草案不重复应用。专用 echo 源码在 `scripts/board/amp_echo_linux`，使用同次独立 kernel 构建。

RTOS base diff 排除了生成 BIN 的删除内容；没有固件二进制、私钥或全量 SDK。旧 build 输入与新 source export 是源码整理，不保证新的编译字节自动等于冻结实测产物。无需重跑板端验证或重建当前包。

累计diff保留源树空白与unified diff上下文前缀，不能用普通源码格式化器改写。仓库.gitattributes仅为本目录patch沿用既有text/LF/-whitespace规则，未修改全局Git设置或运行代码。

许可证按文件原声明：U-Boot GPL-2.0/GPL-2.0+，RT-Thread应用/链接配置 Apache-2.0，相关 Rockchip port/HAL BSD-3-Clause；见 docs/THIRD_PARTY.md 和既有逐文件审查。SDK import SHA 是本地导入提交，不冒充厂商公开 commit。
