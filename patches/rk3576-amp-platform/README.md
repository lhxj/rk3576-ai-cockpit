# AMP 平台补丁历史与当前用途

当前集成事实：[AMP_RPMSG_INTEGRATION_TIP](../../docs/amp/AMP_RPMSG_INTEGRATION_TIP.md)。本目录保留按阶段拆分的原补丁，不能只按编号把所有文件应用到同一个组件。

- 当前 Linux transport 使用 `0003`，已包含早期 `0002` 草案；不重复应用。
- 当前 v5 RTOS 从派生3a39b0f开始，顺序是 `0010-m0-bounded-runtime-evidence`、`0011`、`0012`、`0013`、`0014`、`0015`。
- U-Boot分拆链为0001/0006/0007/0008/0009/0010-uboot；累计重建入口已在 `patches/rk3576-amp-integration`，包含实际149b1c5源码差异。不要与累计补丁双重应用。
- RTOS早期preboard echo/0004与HAL0005已包含在新的组件base累计export中；仍保留作来源历史。

两个0010文件分别属于M0应用和U-Boot最终FDT保护。所有base/derived commit、顺序和hash见 [series.json](../rk3576-amp-integration/series.json)。旧文件名中的draft/host-proposal反映当时阶段，当前实测验收由C执行记录给出。
