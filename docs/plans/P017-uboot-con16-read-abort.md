> **历史AMP阶段计划。** 此前计划的当时进度/阻塞保留；当前链已实板通过且用户要求冻结，不继续开发其中的后续业务建议。当前交接见 [AMP_RPMSG_INTEGRATION_TIP](../amp/AMP_RPMSG_INTEGRATION_TIP.md)，本轮仅整理既有源码与证据。

# P017：记录 U-Boot CON16 只读访问异常

- 目标：解析用户上传的 CON16 读取异常，记录实际访问结果并停止该直接 MMIO 路线。
- 基线：`agent/amp-platform-closure`，`e9c2a97`；CON16/17 当前值未知，AMP 等级 C。
- 授权与范围：用户在 MobaXterm 手动执行既有授权的 CON16 只读命令；本次 Agent 仅作 Host 源码核对、异常码解码及文档更新，不连接实体板。
- 文件范围：本计划与 `docs/reviews/rk3576-amp-board-evidence/`；不修改 linker、ITS、DTS、寄存器或固件。
- 步骤：区分错误后缀与有效命令 → 解码 ESR → 核固定 U-Boot handler 与 ARM64 header → 保留失败证据 → 更新汇总并停止。
- 验证：Host 脚本解码与字段断言；`git diff --check`；审查文档中的证据等级和历史时序。
- 恢复与占用：串口由用户持有。若仍停在 U-Boot panic，用户保存日志后手动重新上电、不打断默认启动；Linux 恢复须以后续实际输出确认。Agent 不发送复位或其它板端命令。

## 实际结果

- `md.1` 的无输出由固定源码的无效后缀返回路径解释，不能作为寄存器实读证据。
- 正确的 `md.l 26004060 1` 触发 `ESR_EL2=0x96000010`；Host 算术断言通过：`EC=0x25, IL=1, ISS=0x10, WnR=0, DFSC=0x10`。
- 当前 U-Boot 直接读 CON16 触发同步外部访问异常，进入 fatal handler；没有寄存器数值，CON17 未读。
- CON16/17、M0/Linux 地址映射仍 BLOCKED/UNRESOLVED；AMP 保持 `C. HOST_BUILD_PASS`。Linux 恢复状态待用户确认，不启动新的读取尝试。

后续用户确认：重新上电后 Linux 正常启动（`BOARD_OBSERVED_USER_REPORT`）。这确认本次 U-Boot 异常后的正常启动恢复，不证明完整离线恢复链已演练，也不改变寄存器取值结果。
