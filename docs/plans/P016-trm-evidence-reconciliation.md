# P016：RK3576 TRM 原文与既有 AMP 证据对照

- 目标：核验用户转交的网页聊天 TRM 报告，准确区分 TRM、固定 HAL 与运行时证据，补充 BUS M0 cache 的原文依据。
- 基线：`agent/amp-platform-closure`，`67ac1e0`；CON16/17 当前值仍未知，Linux MMIO 与 SiP 读取均失败，等级 C。
- 范围：Host 读取桌面的两份 TRM PDF、固定 HAL；更新 board-evidence 文档。不访问实体板，不修改 AMP linker/ITS/DTS 或固件。
- 步骤：提取并检索原 PDF → 检视相关完整页面 → 对照 HAL `soc.h` 偏移与 `hal_cache.c` getter → 更新证据归属、cache 与剩余缺口。
- 验证：PDF 文件 SHA256、页码/表号与视觉核查；固定 HAL SHA；`git diff --check`。PDF 和提取文本保留在忽略目录，不提交厂商资料。
- 交接：以实读原文为 `SOURCE_VERIFIED`；网页聊天的未独立验证结论不升级证据等级。静态文档无法给出 CON17 运行时值。

## 实际结果

- 两份原始 PDF 共 4381 页已全文提取；CON16/17 名称仅见 Part1 p762，目标 PA 字符串未见。Part1 p15/618/750/762/765/766/770/772 与 Part2 p1368 已视觉核对，p241/244 另核 clock/reset 属性。
- 固定 HAL HEAD 为 `277de3fd4b0e640654ee73bb3308be2ef01e3aad`，工作区无改动；`soc.h:651-652` 的 offset 与 TRM 外设映射经 Python 算术核对，得到 `0x26004060/64`。该偏移明确归属 HAL，未归为 TRM detail。
- 新增 `TRM_SOURCE_RECONCILIATION.md`，更新 CON16/17、coherency 和 SUMMARY；确认 16 KB cache/reset bypass/CON14-15 机制，但不把设计配置当作本板状态。
- 没有板端访问或 AMP 产物修改；CON17 runtime、最终 shared Linux PA 和 cache 方案仍未闭合，等级保持 C。
