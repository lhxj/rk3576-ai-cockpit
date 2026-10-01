# P000：接收启动包并验证实际环境

状态：PLANNED（本包host检查不代替用户WSL检查）。

目标：确认文件完整、配置可加载、host烟测通过、SSH只读可用、Git状态可追溯。
文件范围：根构建/规则、scripts/dev、docs/STATUS的新追加记录；禁止板端写入。

1. 阅读AGENTS与STATUS，检查git status/remote/history，不覆盖用户改动。
2. 执行环境检查，确认不在WLXray venv。
3. 运行host_ci并记录结果。
4. 核验Codex版本/项目配置；不把TOML语法通过当作客户端已加载。
5. 获得网络授权后运行只读probe/inventory；没有认证则记录阻塞。
6. 核验当前CAM0-only事实与桌面会话能力，不开始采流。
7. 在已有授权范围内准备初始提交/私有GitHub连接；否则给出待执行步骤。
8. 拆出AMP调查、Media接口、Qt迁移任务并独立审查。

停止条件：需sudo/boot更改、未知remote、缺依赖需安装、板端异常、工具版本不兼容。
验收：host测试输出、脱敏板端盘点摘要、Git状态、明确阻塞，不声称整机集成完成。
