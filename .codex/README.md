# Codex 项目配置

本包使用 `.codex/config.toml` 与独立 `.codex/agents/*.toml`。
Agent文件具备 `name`、`description`、`developer_instructions`；
并发上限为3个子Agent，含amp/media/ui/voice_ai/tooling/reviewer角色。
不会启用hooks、无限循环或额外MCP服务。

依据：OpenAI官方配置参考与Subagents文档，2026-10-01核对，见docs/REFERENCES.md [O1][O2][O3]。
本包做了TOML语法检查；并未在你的Codex版本或账户中实际生成子Agent。

## 第一次启动

1. `codex --version`，记录客户端版本。
2. 在仓库根目录启动Codex；信任项目之前先阅读AGENTS.md与本配置。
3. 主模型沿用客户端选择，不在项目里改认证/provider。
4. 按docs/prompts/PHASE0.md请求主控明确调用需要的子Agent。
5. 若客户端不识别这些字段，不直接绕过报错；先记录版本并核对官方文档。

当前官方配置以 `agents.max_concurrent_threads_per_session` 限定子Agent数量。
旧版本可能只支持旧字段/角色声明，本包不同时叠加两套可能冲突的配置。
需要暂时串行时，可人工备份后将config替换为下述最小配置；Agent规则仍由根AGENTS.md提供：

```toml
approval_policy = "on-request"
sandbox_mode = "workspace-write"
```

## 模型与成本

用户的意向是强模型主控、实现任务使用成本较低模型。
本包不假设账户拥有某个模型；默认不设置model，继承客户端的有效模型。
确认 `gpt-5.6` 在账户和CLI中可用之后，可取消config中对应注释：

```toml
[agents]
default_subagent_model = "gpt-5.6"
```

这是对现有 `[agents]` 表增加/取消注释一行，**不要重复新增第二个同名表**。
不要为了“自动化”静默切换到更昂贵模型。

## 权限与自动开发

默认workspace-write + on-request，沙箱网络默认关闭。
需要GitHub/官方资料/SSH时，按当前客户端机制批准具体操作。
GitHub push任务分支和Draft PR在用户建立远端并授权后可以推进；merge不自动执行。
不要永久放行任意 `ssh`、`sudo`、`bash` 前缀。

AGENTS.md是指令约束而非硬隔离；本地sandbox也无法控制所有远端SSH副作用。
因此第一轮只有只读板端盘点，之后再为受控设备测试设置独占与授权。
