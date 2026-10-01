# 发给 Codex：Phase 0 主控任务

你是本仓库主控。先读AGENTS.md、docs/STATUS.md、docs/architecture/SYSTEM.md、
docs/tasks/ROADMAP.md和.agent/PLANS.md，不复述整份架构后就结束。

当前目标：在现有WSL仓库上建立能继续开发的可靠基础，不是一次实现整个系统。

请执行：
1. 审计实际目录/git状态、已有改动、工具版本；保护用户文件，不重新初始化仓库。
2. 运行scripts/dev/check_environment.sh和host_ci.sh；失败时先定位，不自动大规模装包。
3. 核验Codex配置与可用角色。最多3个子Agent，最多1个写共享文件，实板任务串行。
4. 经网络授权后用scripts/board/probe.sh和inventory.sh只读核验，严禁采流/录音/推理负载。
5. 根据新增事实追加STATUS；历史USER_LOG不得伪装成本轮BOARD_TESTED。
6. 检查GitHub实际remote与lhxj认证；初始化仓库缺首commit时允许一次bootstrap commit。
   不擅自新建/公开仓库或force-push；用户按GITHUB_SETUP建好后再推任务分支/开Draft PR。
7. amp Agent只读盘点实际存在SDK，给出启动链证据与缺失交付；没有SDK则形成下载/授权清单。
8. media Agent设计CameraCapture/Frame/队列边界；本轮可写host接口与测试，不碰设备。
9. ui Agent调查板端Qt/桌面库清单与原Qt工程可获取性；保持既有桌面，不安装或停止服务。
10. reviewer独立审查真实性、资源生命周期、安全门与缺失测试，主控整合结论。

每项有ExecPlan、文件范围和验收证据。一次最多推进3个任务，遇到高风险门停下请求批准，
但可以继续独立的host/文档任务。不要创建无限后台循环，不自动部署boot/AMP固件。
最终给出：完成项、改动文件、测试、未完成与阻塞、建议下一批任务、是否需要用户行动。
