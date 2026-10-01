# P013：RK3576 CON16/CON17 只读前置核验

- 目标：在不修改板端的前提下取得 CON16/17 当前值，计算 M0 shared window 对应 Linux PA；若源码安全门无法确认则立即停止，不进行 MMIO 读取。
- 基线：`agent/amp-platform-closure` 本地 `1ee7d0e`；此前板端普通只读盘点未给出 CON16/17。范围仅报告与 `SUMMARY.md`，不修改 linker、ITS、DTS 或固件。
- 权限：用户本轮列出两个目标 MMIO 纯读命令，但明确规定地址、bitfield 或读取副作用无法确认时停止。`AGENTS.md` L3 对特权/AMP 板端操作要求逐次明确批准；无论审批如何，安全前置核验优先。
- 步骤：①核 TRM v1.2 part1 地址表及 BUS_MCU remap 表；②核固定 HAL/U-Boot 地址和读函数；③寻找 CON16/17 寄存器读访问属性；④仅全部确认后才进入板端 `which devmem`、`uname` 与单次读取；⑤若前置核验失败，记录 BLOCKED 和审批/资料需求。
- 验证：TRM PDF 文本搜索及页面可视检查、固定 HAL/U-Boot 行号比对；`git diff --check`。板端不得尝试替代寄存器访问路径。
- 失败恢复：本轮只做 Host 文档分析，没有板端写入或寄存器访问；无需板端 rollback。

## 实际结果

- TRM §1.1 给 `SYS_SGRF=0x26004000`；§8.6.2 表 8-6 给 BUS_MCU code/shared `CON16/17[31:10]` 映射公式。固定 HAL `soc.h:651-652` 给偏移 `0x60/0x64`（M0 alias 加 `0x20000000`），与系统地址 `0x26004060/64` 一致。
- 对 TRM part1 1412 页全文扫描，`SYS_SGRF_SOC_CON16/17` 只出现在 PDF 第 762 页的 remap 表；未找到两寄存器的 `Attr`、read-clear/side-effect 定义。HAL 的读取函数仅说明源码预期可读，不能证明当前板从 Linux MMIO 读取无副作用。
- 按用户的“无法确认即停止”规则，没有连接板端、没有执行 `which devmem`、`uname`、`devmem`、`/dev/mem` 或任何 MMIO。运行时读数与地址计算仍 BLOCKED，整体维持 C。
