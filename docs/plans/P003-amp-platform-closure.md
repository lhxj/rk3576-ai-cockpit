> **历史AMP阶段计划。** 此前计划的当时进度/阻塞保留；当前链已实板通过且用户要求冻结，不继续开发其中的后续业务建议。当前交接见 [AMP_RPMSG_INTEGRATION_TIP](../amp/AMP_RPMSG_INTEGRATION_TIP.md)，本轮仅整理既有源码与证据。

# P003：RK3576 AMP platform closure

- 目标：从 `C. HOST_BUILD_PASS` 追到唯一、可审查的 M0 echo 上板前方案；只有全部准入证据齐全才标 `D. READY_FOR_CONTROLLED_BOARD_TEST`。
- 固定基线：主项目 `c2b2a83b21837482e1fb15fb1ecab05101a52317`；RTOS 派生 `1d0de06c394f89be35a4b6966e766b56f035c19d`；reference RTOS `7c397f41751feb29b0b388dfda3d2c2225f1f87c`、HAL `277de3fd4b0e640654ee73bb3308be2ef01e3aad`。
- 工作区：`worktrees/rk3576-amp-platform/project` 与 `rtos`，均在独立 `agent/amp-platform-closure` 分支；HAL 只读 symlink。开始时已检查四项 Git 状态，现有其它分支和未提交工作未改。
- 范围：地址/CON16/CON17、RPMsg/mailbox/cache、当前 kernel/U-Boot/BL31 及 FIT/恢复；参数契约、Host 构建与报告。不重复 BSP 身份审查。
- 权限：Host L0；获准既有 SSH 的有界只读 L1。绝不写开发板、启动 M0、重启、读 `/dev/mem`、debugfs 寄存器或进入 U-Boot console。特殊寄存器读取若必要，仅列 `APPROVAL_REQUIRED_READ`。
- 未知项：CON17 真实值及写入者、当前 U-Boot 的 AMP 配置、BL31 M0 SMC 能力、准确 kernel 源 commit、共享 DDR cache 属性、加载 FIT 的可靠来源及不改分区方案。
- 顺序：①阅读既有审查和只读板端基线；②固定来源追 loader/寄存器/内核路径；③补充有界只读盘点；④形成机器可读 contract 与阻塞项检查；⑤在明确地址/配置后才改派生 linker/ITS/DTS 并做 clean builds；⑥生成 artifact manifest、报告并按 D 门复核。
- 验证：对 contract 运行静态一致性脚本；Host 的 `dtc`/FIT/SCons/内核模块构建仅在所需来源闭合时执行；`git diff --check`、reference SHA/clean 复核。缺关键来源即明确 BLOCKED，不填虚构地址、hash 或 PASS。
- 失败恢复：所有修改只在新 worktree；保留旧预板提交与补丁。板端无写入，故无本轮板端恢复动作。若需新分区、bootloader 或寄存器操作，停止该路径并形成下一轮审批材料。
- 资源：一次只读 SSH 任务占用共用 board lock；不进行采流、I2C、性能负载。日志限量存于被 Git 忽略的 `artifacts/local/`。

## 实际结果与交接

2026-10-01：基于 `c2b2a83` 建立新项目/RTOS worktree，旧 reference 与前轮 worktree 未改。固定源码和只读板端证据表明：CPU3 参考 DTS 与 M0 候选 FIT 的 0x47800000 确为物理冲突；CON17 当前值及写入者未证，故未设计新地址。U-Boot 公开 loader 只读 GPT `amp` 分区，当前板无该分区；实际 CONFIG_AMP、BL31 M0 SMC 与 FIT policy 未证。M0 cache hook 为空、实际属性未证。以上足以阻断 D。

完成 Host 候选 clean M0 SCons/FIT 结构生成、板上 headers/Module.symvers 只读复制及 Linux echo KO 全新 Host 构建。建立单一 contract 和 fail-closed 检查器；对 CPU3 参考 DTS 返回 BLOCKED。最终 LubanCat AMP DTS/签名 FIT/可部署 KO 没有生成，不把其 Host gate 记为 PASS。报告在 `docs/reviews/rk3576-amp-platform-closure/`，原始日志在忽略的 `artifacts/local/`。

结论保持 **C. HOST_BUILD_PASS**。等待另行批准的只读寄存器/bootloader 取证、确切 kernel manifest、cache 路径与离线恢复材料后，才可能设计最终布局并重开 D gate；本轮无板端写操作。
