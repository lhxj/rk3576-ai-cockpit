> **历史AMP阶段计划。** 此前计划的当时进度/阻塞保留；当前链已实板通过且用户要求冻结，不继续开发其中的后续业务建议。当前交接见 [AMP_RPMSG_INTEGRATION_TIP](../amp/AMP_RPMSG_INTEGRATION_TIP.md)，本轮仅整理既有源码与证据。

# P002：RK3576 M0 AMP 上板前闭合

- 目标：仅在证据满足全部门槛时将 `C. HOST_BUILD_PASS` 提升到 `D. READY_FOR_CONTROLLED_BOARD_TEST`；本轮绝不部署或修改板端。
- 基线：RTOS `7c397f41751feb29b0b388dfda3d2c2225f1f87c`、HAL `277de3fd4b0e640654ee73bb3308be2ef01e3aad`；前轮八份报告已复制到本项目分支，历史状态仍为 C。
- 工作区：主项目独立 worktree `project`，分支 `agent/amp-preboard-closure`，基于 `main` `3b07df7ba05a535ba18dbc7d692ef961b9b748e5`；RTOS 派生 worktree `rtos`，同名独立分支，HAL 通过同级 `hal` 链接使用固定仓库。原仓库保持不变。
- 权限：Host 编辑/构建与静态分析 L0；如既有 SSH 可用，仅按 AGENTS L1 只读、有界收集当前 boot/DT/config。无任何 L2/L3 或板端写入。
- 未知项：`0x47800000` 的 FIT 装载语义与 M0 alias、实际 U-Boot/BL31、M0 MBOX 路由、cache 属性、UART5 40Pin、当前板 kernel/DTB。缺源码或只读证据即记 BLOCKED，不猜物理地址。
- 顺序：①固定工作区和当前板只读基线；②追 memory/FIT/loader/alias；③追 RPMsg/mailbox/cache/config 与最小 echo；④修必修 warning、clean Host build 和静态一致性脚本；⑤形成十六份报告、精确 changeset/rollback，逐项评 D 门。
- 测试：Arm GNU/SCons clean build；`file/readelf/nm/size/objdump`；DTS/RTOS 参数静态测试；`git diff --check`；核对 reference `rev-parse` 和 clean 状态。
- 失败恢复：修改仅派生 worktree；保留 patch/diff，构建失败记录命令、退出码、warning；不在固定 reference 上回滚。任何板端不可达不影响 Host 调查。
- 资源：已有 Arm GNU 13.2.1 与 SCons；不安装未知工具、不运行候选二进制；构建日志保存在忽略目录并把必要摘要写报告。
- 实绩：两个 reference SHA/clean 复核；派生 M0 echo、mailbox4 client、旧 NS callback、M0 汇编 `.cpu` 和最小 defconfig 已实现。派生 RTOS commit `1d0de06c394f89be35a4b6966e766b56f035c19d`，patch SHA256 `4d02455ad53407414c9eaf832b9245b2c54e9da2ed909f25a4cf500ee5fba1fa` 保存在本项目。最终 clean SCons exit 0、ELF ARMv6S-M、bin 135,256 B；系统 mkimage Host FIT 已成但 unsigned。只读 SSH 取得当前 boot hashes、DTB、分区，确认无 `amp` 分区、MBOX0/4 与 UART5 disabled。参数测试 exit 1，准确暴露 6 项不一致/未知。16 份新报告齐全；原历史审查未改。没有任何板端写操作。
- 结论/交接：维持 **C. HOST_BUILD_PASS**。关键 blocker 为 FIT/vring `0x47800000` 物理冲突、CON17 alias 未证、Linux link3/MBOX3 与 M0 link4/MBOX4 不符、shared cache 属性、FIT 签名与缺 `amp` 分区、当前板 DT 缺 AMP/RPMsg、U-Boot/BL31 配置和离线 rollback 未闭合。下一步只在 Host 获取 exact kernel/U-Boot/BL31、制定并静态验收全新不重叠板级布局；达到 D 后仍须等用户明确批准上板。
