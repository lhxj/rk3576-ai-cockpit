# P020：对应板端固件的 CON17 配置调用链

- 目标：沿已匹配板端 load payload 的 RK3576 BL31，以及固定 U-Boot 源码，确认 CON17 写入的接口、参数、调用时序和默认值依据；区分源代码能力、文件指令证据与运行时事实。
- 基线：`agent/amp-platform-closure` / `c82057d`；U-Boot `8f53f800da2c25d0c6ba414fb45902a01675703a`；rkbin `58a39b47f77a26a1e110fa1a1ce80bcfcb0b3505`。CON17 当前值未知，AMP 等级 C。
- 范围与权限：L0，Host 静态源码/ELF/bin/反汇编分析。无 SSH、MMIO、SMC、板端写入、启动 M0；不修改 linker/ITS/DTS，也不设计新地址。第三方文件仅在忽略的 `artifacts/local/`。
- 文件范围：本计划、board-evidence 新调用链证据与 SUMMARY、必要的 Host 验证脚本；不改历史失败记录。
- 步骤：核源码的 MCU SiP caller 与 FIT 启动入口 → 核实际 BL31 对 CON17 的引用和写入分支 → 查初始化常量、覆盖和参数来源 → 形成可证明部分和剩余缺口。
- 验证：原文件 SHA、原始指令字/地址算术、完整源码调用点搜索、`git diff --check`；只运行自写的 Host 解析脚本，绝不执行固件。
- 验收：回答“公开启动路径是否设置 CON17、用什么值、何时生效”，不能把没有找到调用者解释为寄存器默认零；证据不足即 BLOCKED，不升级 D。
- 失败恢复：分析不接触板端；保持原固件/reference 不变。网络或公开源码缺口只记录，不盲试寄存器或其它 SMC。

## 实际结果

- 固定 U-Boot 源码调用链闭合到 FIT load → CODE selector → gate/reset，未找到 shared selector caller；发现两层错误返回被忽略，记录为未来 release 前必须解决项。
- 对应板端 BL31 的参数型 CON17 setter 与含 CON16/17 的 EL3 保存/恢复表均核原始指令；表的零是 buffer pointer，占位值不能用来推断默认映射。
- 实际 U-Boot payload 不含公开 AMP loader 的关键字符串；仅标 SOURCE_INFERRED，实际 CONFIG_AMP 仍 UNVERIFIED。
- 新增 Host verifier 正向 exit 0、错误固件 hash 负向 exit 1、py_compile exit 0；新增调用链文档/机器结果，更新 SUMMARY。
- 当前 CON17/有效 shared PA 没有取得；不改 AMP artifact、不访问板、不升级 D。下一项是确定配置的 Host changeset 与真实构建/资源/恢复证据，不能先猜地址。
