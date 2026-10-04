# RK3576 AI Cockpit · 项目启动包

**目标：基于鲁班猫3 RK3576 的 AMP AI 车载多媒体座舱。**

这是项目的规则、状态、任务和构建基础，不是已完成的座舱软件。
先阅读 [START_HERE.md](START_HERE.md)，再让 Codex 读取 [AGENTS.md](AGENTS.md)。

| 入口 | 用途 |
|---|---|
| [AMP_RPMSG_INTEGRATION_TIP](docs/amp/AMP_RPMSG_INTEGRATION_TIP.md) | 已实测最小AMP/RPMsg链的集成提交、源码与证据 |
| [docs/STATUS.md](docs/STATUS.md) | 用户已验证的硬件事实、待核验项、阻塞项 |
| [docs/architecture/SYSTEM.md](docs/architecture/SYSTEM.md) | 已确定的功能范围与模块边界 |
| [docs/tasks/ROADMAP.md](docs/tasks/ROADMAP.md) | 有依赖与验收条件的开发队列 |
| [docs/prompts/PHASE0.md](docs/prompts/PHASE0.md) | 可直接发给 Codex 的首轮任务 |
| [.codex/README.md](.codex/README.md) | Agent 配置、模型选择、版本兼容说明 |
| [docs/dev/GITHUB_SETUP.md](docs/dev/GITHUB_SETUP.md) | Git 首次提交、GitHub 私有仓库与 Issue |
| [docs/dev/BOARD_WORKFLOW.md](docs/dev/BOARD_WORKFLOW.md) | SSH、设备占用、测试恢复与安全边界 |
| [docs/VALIDATION.md](docs/VALIDATION.md) | 本启动包实际完成的检查；不冒充板端验证 |

## 最小检查

在 WSL 的项目根目录运行，不是在 `cat@lubancat` 远端：

```bash
bash scripts/dev/check_environment.sh
bash scripts/dev/host_ci.sh
```

第二条只构建无硬件依赖的 `cockpit_host_smoke`、运行 CTest 和仓库结构测试；
不会 SSH、采集、刷机、安装软件或提交 Git。

## 边界

- 主仓库：`~/rk3576-work/cockpit/rk3576-ai-cockpit/`。
- SDK、参考项目、模型放在 `~/rk3576-work/` 的其他子目录，不入本仓库。
- 板端通过已有 SSH 别名 `lubancat` 访问；不存储 IP、Wi-Fi 密码或 SSH 私钥。
- CAM0 是当前开发基线；CAM1 新排线待到货，双摄并发尚未验证。
- HDMI 与触摸已由用户实测可用；Qt 应用仍待开发。
- MPU6050 已有、未集成；LED / 按键 / 蜂鸣器按用户决定用软件模拟。
- AMP / RPMsg 最小链已实板双向收发并冷恢复通过；[当前集成tip](docs/amp/AMP_RPMSG_INTEGRATION_TIP.md)整理源码、配套产物与证据，业务扩展未开发。
- 压缩包不包含 `.git/`，不会创建 GitHub 仓库或自动运行 Codex。

本仓库保存必要的派生源码补丁及来源声明，不vendor完整厂商SDK、模型或固件，不授予额外许可证授权。
参考代码引入前按 [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md) 登记来源与授权。
