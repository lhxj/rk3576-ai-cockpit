# 从这里开始

## 1. 解压位置

目标是 WSL 的：

```text
/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit/
```

ZIP 内就是项目根目录文件，不再套一层 `rk3576-ai-cockpit/`。
包中没有 `.git`，无需重新 `git init`。解压会更新同名文件，因此先备份。
假设下载文件位于 Windows 的默认下载目录：

```bash
cd ~/rk3576-work/cockpit/rk3576-ai-cockpit
backup="../rk3576-ai-cockpit.before-starter.$(date +%Y%m%d-%H%M%S).tar.gz"
tar -czf "$backup" .
unzip -o /mnt/c/Users/27432/Downloads/rk3576-ai-cockpit-starter-v1.0.zip -d .
```

若下载位置不同，只修改 ZIP 路径。以上需逐条执行；备份失败就不要覆盖。
`unzip -o` 只覆盖包内同名文件，不清除目录中其他文件。
以前自行添加的 `.codex` 文件也不会被删除，启动时如有角色重复，先比较备份。

## 2. 检查环境与最小构建

确保当前提示符是 WSL 的 `ywx@ywx:...`。
如果还激活着 WLXray 的 Python 环境，先在当前交互 shell 执行 `deactivate`。
本包脚本不会修改你的 shell 配置或其他项目。

```bash
bash scripts/dev/check_environment.sh
bash scripts/dev/host_ci.sh
```

最低依赖是 CMake >= 3.21、支持 C++17 的 g++、make、Python >= 3.10。
缺少时先查看输出，再决定是否安装。不要为了本包先装完整 SDK 依赖。
本包默认没有 Qt / MPP / RKNN / ALSA 开发库依赖。

## 3. 可选：检查 SSH（不采集、不改板）

```bash
bash scripts/board/probe.sh
```

使用 `BatchMode=yes` 与已有主机指纹，不弹密码，不关闭主机指纹检查。
认证失败时在 WSL 交互 shell 修复 SSH / ssh-agent，不把密码写进脚本。
`ssh lubancat` 在普通终端可用，不代表 Codex 沙箱自动获得网络权限。
首次由 Codex 执行时，批准具体的只读任务，不授予任意 SSH / sudo 通行权限。

## 4. 首次 Git 提交与 GitHub

最后已知本地 `.git` 存在、main 分支没有提交；远端是否创建尚未确认。
先执行：

```bash
git status --short
git remote -v
git config --get user.name
git config --get user.email
```

必要时只为当前仓库设置姓名邮箱；不要覆盖已有全局 Git 身份。
确认没有密钥、SDK、模型或原始日志后：

```bash
git add --dry-run .
git add .
git diff --cached --stat
git commit -m "chore: bootstrap RK3576 AI cockpit starter"
```

初始化提交允许留在 main。之后开发使用 `agent/*` 分支。

新建远端、连接已有远端、发布 Issue 的具体分支步骤在
[docs/dev/GITHUB_SETUP.md](docs/dev/GITHUB_SETUP.md)。
不要重复创建或覆盖已存在的 origin。

## 5. 启动 Codex

```bash
codex --version
codex
```

首次信任项目时先阅读本包规则；不要关闭全局安全限制。
将以下内容发给它即可：

```text
读取 AGENTS.md、docs/STATUS.md、.agent/PLANS.md 和 docs/prompts/PHASE0.md。
按照 PHASE0 执行。先验证实际仓库、环境和硬件状态，再推进任务。
使用不超过 3 个子 Agent；同一时间只允许一个板端任务。
本轮只允许 host 构建与板端只读盘点，不采流、不刷机、不改 /boot、不重启。
先做可自主完成的工作；遇到真实阻塞时保存证据，不伪造成功。
```

主模型沿用你客户端的选择；默认没有写死任何模型名。
需要“强模型主控、较便宜模型执行”时，按 `.codex/README.md` 选择账户实际可用模型。

## 6. 你应当看到的第一轮成果

不是“整套座舱已完成”，而是：

- host 构建和测试可重复运行；
- 已有事实与新盘点明确分开；
- GitHub 连接状态被核验，准备可审核提交或 Draft PR；
- AMP 调查列出真实 SDK 路径 / 缺失材料；
- 单摄接口与 Qt 集成任务形成小而明确的计划。

本包没有自动驻留程序或无人值守循环。Codex 在当前会话和授权范围内推进，
会话关闭、额度用完、网络失败或安全门阻塞时不保证继续运行。
