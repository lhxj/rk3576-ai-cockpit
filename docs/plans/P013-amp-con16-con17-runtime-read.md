> **历史AMP阶段计划。** 此前计划的当时进度/阻塞保留；当前链已实板通过且用户要求冻结，不继续开发其中的后续业务建议。当前交接见 [AMP_RPMSG_INTEGRATION_TIP](../amp/AMP_RPMSG_INTEGRATION_TIP.md)，本轮仅整理既有源码与证据。

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

## 2026-10-02 后续授权与连接结果

用户随后明确修改禁令，批准仅各执行一次 CON16/CON17 的 32-bit 只读访问，第一条异常即停止。按 `scripts/board/_common.sh` 取得 WSL 用户级 board lock 后，先以既有 SSH 别名进行 `which devmem` 预检。到旧地址 `10.34.122.223:22` 的连接超时，远端命令没有执行；Windows TCP 22 检查也失败。用户随后给出新 IP `10.232.249.223`。

新 IP 的 SSH host key 与旧 IP 已保存的 ed25519 key 一致。板端 `which devmem` 返回 1；已安装的 `/usr/bin/busybox` 含 `devmem` applet。`uname -a` 为 `6.1.99-rk3576 #8`。在重新取得 board lock 后，用已安装 applet 对 CON16 **仅尝试一次** `sudo -n /usr/bin/busybox devmem 0x26004060 32`：命令退出码 1，stdout/stderr 均为空，没有寄存器值。板端之后仍可经 SSH 回应；sudo 日志确认命令曾启动，有限内核日志未提供失败原因。依约**未尝试 CON17，也未重试 CON16**。详细记录见 `docs/reviews/rk3576-amp-board-evidence/CON16_CON17_APPROVED_READ_ATTEMPT.md`。运行时映射仍未取得，AMP 保持 C。
