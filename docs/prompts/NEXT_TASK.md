# 后续每轮任务入口

读取AGENTS.md、docs/STATUS.md与docs/tasks/ROADMAP.md。
选取依赖已满足的最高优先级未完成任务，先核验GitHub与本地任务记录是否一致。
创建或更新ExecPlan，最多3个子Agent；独立文件范围，实板串行。
依据授权仅做允许的步骤；不把Mock/旧日志当新实测。
完成host测试与独立review后准备任务分支和Draft PR，禁止自动merge。
最多完成3个任务后汇总一次；任何boot/高风险变更须请求用户确认。
