# RK3576 Full System Integration ExecPlan

日期：2026-10-04。负责人：主控。无子Agent、无新功能开发。

## 目标与已有证据

从 Application `8585c66d27fa65ef11a6531b95656acfa3dc9e8b` 建立
`agent/system-integration`，普通双亲 merge
`ec56833276d31df1e1ce8d36741a552042e2b6ca`。
Application 固定分支已推送，不改变原 tip；Application 31/31 Host CTest、6/6 Python、
35/35 RK3576 CTest为既有基线。AMP最小链有真实一次收发及冷恢复记录，冻结。

## 范围、权限与未知项

L0：独立worktree合并、文档/任务语义收敛、Host/静态/一致性/sanitizer测试。
L1：既有SSH认证、HostName覆盖为用户当前地址，持板锁有限只读盘点。
L2：仅在已有冻结AMP部署实际运行且无人占用时，按用户授权做逐级/300秒共存测试。
L3不执行：不得改boot/FIT/DT/内存/transport、重刷、重启或自行热启动M0。

冻结最小echo在PONG后销毁endpoint并deinit，没有业务重复请求接口。
需判断该既有能力能否满足用户提出的持续/退出后再次echo；不能以新协议补齐。
AMP冷恢复记录表明当时默认Debian未自动启动M0，需只读确认当前部署。

## 操作与验证

1. 核验两个tip、merge-base、工作树及origin；保护旧工作树未跟踪材料。
2. 创建独立worktree，`git merge --no-ff --no-commit <AMP tip>`。
3. 仅三处文档冲突：STATUS合并两套事实，THIRD_PARTY保留双方声明，ROADMAP保留
   应用进展并更新AMP最小链。manifest/Feature Matrix明确业务未实现。
4. `bash scripts/dev/host_ci.sh`；AMP纯Host单测与已有vendor函数harness；精确hash
   核验冻结源码/配置/本地产物；确认应用源码/全部测试未减少。
5. 安全ASan/UBSan测试；Qt退出缓存LSan限制单列，非Qt启用LSan。
6. 先完成Host门，再只读确认板身份/paired kernel/RPMsg endpoint。缺前提则停止
   实板后续，不通过默认系统应用负载掩盖AMP缺失。
7. 如前提齐全：T1-T7逐级、300秒共存、资源/错误/echo计数及退出后重复echo。
8. 文档登记真实结果，提交双亲merge并推送独立分支，不合main。

## 构建资源与失败恢复

Host构建parallel2、输出在忽略的build与artifacts/local，SDK/模型/原日志不入Git。
各自构建入口独立，不从应用CMake自动构建/签名/部署AMP。
有运行负载才管理本次PID，不杀无关进程；出现boot部署需要即停止。
合并失败只保留新worktree供审查，不reset旧分支。冻结输入必须与AMP tip逐字一致。

## 实际结果与交接

完成到 **SYSTEM_INTEGRATION_HOST_PASS**。Host31/31 CTest、41/41 Python、撤回保护5/5、
额外AMP8/8、shell检查、全31项ASan/UBSan及非Qt27项LSan通过。初始失败和最小
测试维护均保留记录。100个冻结AMP文件及28项来源/产物hash一致，应用187个文件一致。
Board只读退出0：默认kernel#8、无stage、RPMsg设备0。按用户要求停止T2–T7及300秒负载，
不改启动配置、不重建签名固件；没有新功能。持续echo仍受冻结服务一次性能力阻塞。
完整结果见[共存记录](../bringup/system-integration/BOARD_COEXISTENCE_RESULT.md)、
[构建矩阵](../bringup/system-integration/BUILD_MATRIX.md)及[资源记录](../bringup/system-integration/RESOURCE_RESULT.md)。
