# P014：RK3576 CON16/CON17 访问语义静态取证

- 目标：分别裁决 CON16/17 读取副作用与当前 Linux EL1 直接访问权限；本轮不访问开发板。
- 基线：`agent/amp-platform-closure`，`599cde2`；此前 TRM/HAL 地址核对通过，但读访问属性缺失，板端读取已停止。
- 未知：CON16/17 的权威 read attribute、实际板端 BL31 防火墙配置、任何针对这两个寄存器的厂商读回校验。
- 文件范围：本计划及 `docs/reviews/rk3576-amp-board-evidence/CON16_CON17_ACCESS_SEMANTICS.md`；不修改 AMP 代码、配置或产物。
- 权限等级：L0，纯 Host 源码和文档取证；禁止 SSH、devmem、debugfs、U-Boot console 及任何板端操作。
- 步骤：核 TRM 中 SYS_SGRF 的定义和寄存器访问说明；追固定 HAL 的读取函数及其实际可达路径；查固定 U-Boot 和公开 TF-A 的读回与安全防火墙；分开裁决读取语义与 Linux 可达性。
- 验证：固定 commit 源码行号、TRM 章节、`git diff --check`；人工复核所有结论未将公开源码当成当前板二进制证据。
- 预期结果：形成 `READ_GATE_OPEN/PARTIAL/BLOCKED` 中唯一裁决，并列出缺失证据。
- 失败恢复与资源：仅新增文档，无板端或构建资源占用；无需板端恢复。

## 实际结果与交接

- TRM v1.2 part1 给出 SYS_SGRF 基址及 BUS MCU remap 字段，但未给 CON16/17 明确读访问属性。固定 HAL 的弱读取函数在默认 cache 地址配置下不作为运行时读回；固定 U-Boot 与所查上游 TF-A 路径均未读回这两个寄存器。
- 上游 TF-A 将 SYS_SGRF 放入 secure slave group 1，并对 non-secure 访问设拒绝；当前板实际 BL31 配置未证。
- 两个独立条件均未获充分证明，裁决 `READ_GATE_BLOCKED`。需要 Rockchip/板厂针对这两个寄存器的访问属性或等价的受支持只读接口说明，以及与当前 BL31 对应的 Linux 可达性证据。AMP 仍为 `C. HOST_BUILD_PASS`。
