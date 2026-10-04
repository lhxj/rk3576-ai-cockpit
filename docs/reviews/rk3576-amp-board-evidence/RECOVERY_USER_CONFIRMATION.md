> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 整机恢复实测：用户确认通过

- 记录日期：2026-10-03；来源：本会话用户反馈。
- 用户先反馈“刷机功能正常”；在确认是否实际刷写并正常启动 Linux 后，用户明确回复：**“‘整机恢复实测’通过”**。
- `WHOLE_BOARD_RECOVERY_TEST = PASS`，证据等级 **BOARD_OBSERVED_USER_REPORT / USER_CONFIRMED**。这是用户执行并确认的结果；Agent 本次没有访问或操作开发板。

## 证据范围

接受用户对整机刷写恢复并正常启动 Linux 的实测确认，无需重复恢复演练。此前 Host 已核验的官方恢复镜像、桌面工具和原 boot 备份见 [RECOVERY_IMAGE_ANALYSIS.md](../rk3576-amp-platform-closure/RECOVERY_IMAGE_ANALYSIS.md) 和 [ROLLBACK_FINAL.md](../rk3576-amp-platform-closure/ROLLBACK_FINAL.md)。

用户本次没有提供实际使用的镜像文件名/hash、MASKROM 或 LOADER 模式、工具日志和恢复后 boot hash。因此不把 Host 已核镜像标成此次实际刷入镜像，也不声称两种模式均已实测。既有 boot/U-Boot/DT hash 是恢复前取证和备份身份，不能自动当作恢复后当前状态。

## 对 AMP 准入的影响

整机恢复实测缺口按用户确认关闭。用户数据备份、拟部署 AMP 的逐文件 rollback 清单和阻止早期 M0 重复加载的措施，仍须与最终 changeset 一同确定。当前 M0 映射、实际 AMP loader/加载源、SMC 动态能力、验签和 coherency 不能由整机恢复成功推导。

整体仍为 **C. HOST_BUILD_PASS**，`APPROVAL_GATE_BOARD_TEST` 未开放。本次只更新文档，不部署、重启、读取 MMIO 或启动 M0。下一次已授权的普通只读盘点应重新核恢复后的运行版本、DT、分区和 boot hash，保留恢复前原件。

## 本次 Host 检查

`git diff --check` exit 0；`bash scripts/dev/host_ci.sh` exit 0（2 CTest、22 Python PASS）。这些检查只验证仓库，无新增实板恢复证据；恢复实测 PASS 的来源始终是用户确认。
