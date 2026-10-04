# Merge provenance

2026-10-04 / WSL Ubuntu-22.04，x86_64；Windows PowerShell仅作为WSL调用入口。
独立worktree：`/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit-system`。
旧主仓未跟踪材料/应用worktree不改，未使用reset/rebase/squash/force push。

| 角色 | SHA / 分支 |
|---|---|
| First parent / Application | `8585c66d27fa65ef11a6531b95656acfa3dc9e8b` / `agent/application-integration-tip` |
| Second parent / AMP/RPMsg | `ec56833276d31df1e1ce8d36741a552042e2b6ca` / `agent/amp-platform-closure` |
| merge-base | `3b07df7ba05a535ba18dbc7d692ef961b9b748e5` |
| Output | `agent/system-integration` |

`git push -u origin agent/application-integration-tip`退出0，原tip不变。
`git merge --no-ff --no-commit ec56833276d31df1e1ce8d36741a552042e2b6ca`
仅出现三处文档冲突，没有冻结平台/RTOS/通信输入冲突。

| 冲突 | 语义解决 |
|---|---|
| `docs/STATUS.md` | 保留全部Application历史记录及AMP原始状态快照；顶层新汇总分开Application PASS、AMP BOARD_PASS、Full-system未测/阻塞。不能用AMP-only旧PLANNED覆盖应用进展 |
| `docs/THIRD_PARTY.md` | 保留IMX6ULL禁止复用/许可证未确认声明和AMP SDK/GPL/Apache/BSD及派生补丁来源；两段独立追加均保留 |
| `docs/tasks/ROADMAP.md` | 保留所有应用工作包和通过项，P002更新固定来源审查，P008只更新最小链BOARD_PASS/FROZEN；P010/P011完整业务目标未通过 |

自动合并后补充语义收敛：manifest与Feature Matrix同步P008范围；SYSTEM指向本轮门；
CMake提示明确“无VehicleCore/RPMsg业务backend”，AMP使用独立入口。
未删除应用测试或改硬件资源、未逐个merge历史分支。
两处Host测试维护：`tests/python/test_amp_host_proposal.py`跟进权威tip的最小链状态，
并增加生成前后完整contract不变断言；`scripts/amp/test_check_platform_contract.py`
使用显式临时静态fixture，摆脱只在旧worktree成立的外部SDK默认路径。
测试函数数量未减少，发布负例保留；真实checker/RTOS/transport未改。

最终merge SHA及双亲由本分支`git log --merges --format=fuller`核验；不在commit中
嵌入自身SHA。完整性检查结果在BUILD_MATRIX；原始命令/日志仅忽略的artifacts/local。
