> **历史AMP阶段计划。** 此前计划的当时进度/阻塞保留；当前链已实板通过且用户要求冻结，不继续开发其中的后续业务建议。当前交接见 [AMP_RPMSG_INTEGRATION_TIP](../amp/AMP_RPMSG_INTEGRATION_TIP.md)，本轮仅整理既有源码与证据。

# P018：直接读取失败后的 MCU 映射证据替代路线

- 目标：在 Host 核对实际 BL31 镜像内 MCU 配置分支，为当前寄存器读取之外的证据路线提供依据。
- 基线：`agent/amp-platform-closure`，`d557948`；用户确认异常后重新上电 Linux 正常启动，CON16/17 数值仍未知。
- 范围：已复制的 U-Boot/BL31 文件、固定 U-Boot header；文档。没有板端访问，不调用 SMC、不改变 remap/reset/firewall，不修改 linker/ITS/DTS。
- 步骤：核镜像 hash → 追 MCU 配置 dispatcher 与参数 → 核机器码和地址算术 → 记录可行路径及动态验证缺口 → 更新恢复证据。
- 验证：Host 反汇编、文件 hash 与有限指令字检查、`git diff --check`。
- 边界：配置接口是写操作；当前运行时值不能由“二进制有该分支”证明。只有启动配置、调用时序、返回状态、有效 reset 和完整恢复条件闭合后，才可能作为将来映射的确定性证据。
- 交接：优先离线核对与厂商文档；不再建议直接 MMIO，不执行任何新的板端实验，等级仍 C。

## 实际结果

- 核实际 eMMC 提取文件 SHA256 和 FIT `atf-1` load；从 `0x4005eb24` dispatcher 追到 MCU 配置分支，参数与固定 U-Boot ABI 一致。
- 原始机器码的有限离线复核 exit 0：selector 1 写 CON14/15/16 等配置，selector 3 将参数写 CON17。没有得到当前值，没有 SMC 调用。
- 新增 `MCU_MAPPING_ALTERNATIVE_PATH.md`，按静态配置证据/未来确定配置/厂商只读接口区分路线与审批范围；更新用户确认的 Linux 恢复状态。
- 当前映射与关键 Gate 仍未闭合，等级 C；没有板端操作或固件/artifact 修改。
