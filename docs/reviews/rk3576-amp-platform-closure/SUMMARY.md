# RK3576 AMP platform closure — 2026-10-01

**结论：C. HOST_BUILD_PASS；D 未达到，禁止上板。** 本轮只做 Host 与板端只读盘点。固定 RTOS/HAL reference 未改；派生 worktree 位于 `worktrees/rk3576-amp-platform/rtos`，项目分支 `agent/amp-platform-closure` 基于 `c2b2a83`。之前的 BSP 身份、M0 架构、callback 与 I2C 配置结论保持有效。

| Gate | 结果 | 证据等级 |
| --- | --- | --- |
| 0x47800000 | 是实际 FIT payload 物理装载地址；CPU3 参考 DTS 又用作 vring0，二者原样合用确实冲突。参考 DTS 注释明确 CPU3 link3、MCU link4，故它不适用于 M0 原样部署。当前板无 AMP DT，不存在已发生的运行时冲突。 | SOURCE_VERIFIED / BOARD_OBSERVED_READONLY |
| CON16/CON17 | TRM 映射公式、寄存器位置有据；当前板值、CON17 写入者和时序未证。 | SOURCE_VERIFIED；板值 UNVERIFIED |
| RPMsg | 候选 M0 link4、MBOX0 TX/MBOX4 RX、NS service、两 ring 参数源码可追；Linux 仅 CPU3 link3/MBOX3 参考，当前板无最终 M0 DT。 | SOURCE_VERIFIED；端到端 BLOCKED |
| Cache | M0 侧 RPMsg cache hook 为空，U-Boot 源含 uncache window CON14/15 写入，但当前板执行及实际 shared DDR 属性未证。 | UNVERIFIED |
| Boot | 匹配版本 U-Boot 源的 `AMP_PART` 是 GPT 分区 `amp`；当前 eMMC 无该分区，实际 U-Boot `CONFIG_AMP`、BL31 MCU SMC、FIT 验签未知。 | SOURCE_VERIFIED / BOARD_OBSERVED_READONLY / BLOCKED |
| Host | 派生 M0 echo clean SCons + unsigned FIT 结构生成；Linux echo 模块对板上实际 headers/Module.symvers 构建通过，但 exact kernel source commit 未证。均是 **非部署候选产物**。 | HOST_TESTED |
| DTS / UART / recovery | 未选最终内存地址，未生成可部署 LubanCat AMP DTB；UART5 当前 DT disabled 但 v2 物理/互斥未闭合；无完整离线恢复链。 | BLOCKED |

唯一参数源 `docs/amp/AMP_PLATFORM_CONTRACT.yaml` 用 JSON 语法的 YAML 1.2；未知最终值为 `null`，检查器 `scripts/amp/check_platform_contract.py` 返回非零。报告标题中的 “FINAL” 表示最终 Gate 裁决，不表示已有可部署最终地址。

下一步需取得 **批准的只读寄存器/bootloader 证据**、当前 U-Boot/BL31 镜像与配置、CON17 和缓存属性，再依证据设计 M0 专用 reserved-memory/ITS/DTS 并完成离线恢复。当前不得把 Host FIT、KO、参考 DTS 或本报告用于部署。`BOARD_TEST_APPROVAL_PLAN.md` 只定义未来审批输入。
