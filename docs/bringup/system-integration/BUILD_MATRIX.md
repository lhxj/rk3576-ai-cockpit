# System integration build results

2026-10-04；**SYSTEM_INTEGRATION_HOST_PASS**。
环境：WSL Ubuntu22.04 x86_64，GCC11.4/CMake3.22.1、Qt5.15.3（qmake3.1）。
构建入口见[系统构建矩阵](../../architecture/SYSTEM_BUILD_MATRIX.md)。

| 验证 | 本轮实际结果 |
|---|---|
| Application Host configure/build/CTest | `bash scripts/dev/host_ci.sh`退出0；31/31 CTest，Qt5 shell构建；原31项均保留 |
| Python/Shell/AMP checks | 原Application 6项保留，合并后41/41 Python；host_ci另跑撤回保护5/5；所有scripts中的`.sh`经bash -n；均退出0 |
| AMP纯Host额外单测 | `python3 -m unittest discover -s scripts/amp -p 'test_*.py' -v`：8/8，退出0；包含旧完整contract负例与撤回保护 |
| ASan/UBSan | 全31/31 CTest通过，退出0；`detect_leaks=0`，`halt_on_error=1`；无删测试 |
| 安全LSan子集 | `detect_leaks=1`，排除四个Qt进程测试的27/27通过，退出0；排除不改变普通CI或全31项ASan/UBSan |
| Application源码/测试完整性 | Application tip的apps/libs/tests/cmake/config/CMakePresets共187个文件与当前树Git blob完全一致，输入两树文件联合无缺失 |
| 冻结AMP源码/配置及现有产物hash | rtos/platform/patches/docs/amp/AMP工具及echo输入100个文件与AMP tip逐字一致（单独列出的Host测试修复除外）；JSON指定28项SHA256全部PASS，包含全部10项既有产物 |
| U-Boot Host函数harness | `p030_test_final_fdt.py`：实际vendor函数/真实libfdt，235 checks；`test_uboot_factory_boot.py`：181 checks；编译/运行均退出0；硬件边界stubs，无固件执行 |
| 旧完整contract检查 | 对真实外部固定RTOS source运行`check_platform_contract.py --rtos ... --linux-dts scripts/fixtures/rk3576-amp-reference.dtsi`，退出1/30 failures（预期旧完整发布门BLOCKED）；不是最小链失效或完整contract发布PASS |
| Application AArch64重建 | NOT_RUN；实板为默认#8、无AMP/RPMsg，按用户要求停止Board后续 |
| M0/U-Boot/paired kernel/KO重建或部署 | NOT_RUN；冻结产物不自动重建、签名、写boot |

既有Application RK3576 35/35 CTest和AMP实板一次收发为历史证据；不等于本轮构建
或全系统同载测试。旧完整deployment checker负例测试继续保留旧BLOCKED语义。

## 首次失败与最小修正

- 首轮Host CI的C++31/31通过，但Python40/41：`test_host_proposal_does_not_open_release_gate`
  仍断言旧`BLOCKED_PREBOARD`，AMP权威tip已经改为`BOARD_TESTED_MINIMAL_CHAIN_FROZEN`。
  在原AMP worktree直接运行同测试也失败（退出1），不是此次merge修改了资源。
  修正测试为生成前后完整contract不变，同时保留原始缺失final字段/board-ready false
  和Host proposal DRAFT_NOT_DEPLOYABLE检查。未删除或减弱发布门。
- 额外AMP测试首次3项失败，原因是隐式`ROOT.parent/rtos`仅在AMP旧worktree成立。
  修正负例测试为显式临时静态parser fixture，常量不跟着被修改contract变化；
  不依赖外部SDK、不运行firmware。checker自身未改，另跑真实固定source检查。
- 首次LSan筛选只排除了两个Qt测试，剩余两个startup测试各报告既有Qt/fontconfig
  656-byte/9-allocation退出缓存（exit8）。保留日志；最终正确限定非Qt27项，全部
  LSan通过。全31项ASan/UBSan通过，不将Qt LSan写成PASS。

命令、退出码和原始日志位于忽略的`artifacts/local/system-integration/`。
AMP外部输入只读来自既有`worktrees/rk3576-amp-platform/project/artifacts/local`；
未复制固件、SDK、私钥或媒体至Git。签名/固件重建/部署均NOT_RUN。

合并diff格式检查：普通编辑无新增whitespace错误；整体cached diff检查仍报告
`patches/rk3576-amp-preboard/0001-Add-host-only-RK3576-M0-RPMsg-echo-candidate.patch`
的历史patch上下文尾空格/format-patch尾标记。该文件与AMP tip逐字一致，保留其hash，
不清理patch内容以免改变冻结来源。该格式告警不是运行测试失败。
