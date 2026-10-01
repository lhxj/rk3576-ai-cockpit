# Git / GitHub 初始化

目标仓库：`lhxj/rk3576-ai-cockpit`，默认私有。
用户最后提供：gh以lhxj登录、HTTPS协议；本地main无提交。
本启动包没有连接用户GitHub，也没有创建仓库、Issue、PR或分支保护。

## 1. 首次提交

在本仓库WSL目录运行，先检查：

```bash
pwd
git status
git remote -v
git config --get user.name
git config --get user.email
```

身份缺失时只设置本仓库：`git config user.name "lhxj"`，邮箱用用户自己确认的GitHub邮箱/隐私邮箱。
不要虚构邮箱，不执行全局身份覆盖。

```bash
bash scripts/dev/host_ci.sh
git add --dry-run .
git add .
git diff --cached --stat
git commit -m "chore: bootstrap RK3576 AI cockpit starter"
```

初始提交是main上的唯一初始化例外。之后使用agent任务分支。
Git不跟踪空目录，本包在模块目录中放了职责README，不需要批量.gitkeep。

## 2. 远端

先检查是否已有origin和仓库：

```bash
git remote -v
gh auth status
gh repo view lhxj/rk3576-ai-cockpit
```

“无法访问”可能是网络/认证失败，不能直接当成仓库不存在。
确认尚未创建、网络认证正常且origin不存在，才执行官方CLI创建命令 [G1]：

```bash
gh repo create lhxj/rk3576-ai-cockpit --private --source=. --remote=origin --push
```

如果仓库已有，不重新创建。先确认owner、visibility、远端历史与本地兼容，
再由用户选择添加origin或clone/整合；不强推、不覆盖已有远端。
如HTTPS git push的认证尚未接好，核验后可人工执行`gh auth setup-git`；不把Token嵌入URL。

## 3. 初始Issue（可选）

本地任务已经在docs/tasks中，无GitHub也能开发。
默认命令只打印待建列表，不联网：

```bash
python3 scripts/dev/seed_issues.py
```

明确批准创建后：

```bash
python3 scripts/dev/seed_issues.py --apply
```

脚本核对账户和私有仓库，按稳定Pxxx标记跳过已有任务。
不会假设任务P001恰好对应GitHub #1，也不会重复创建同名任务。
它不创建仓库、不push源码、不自动关闭Issue。

## 4. 后续任务

```bash
git switch -c agent/P001-board-inventory
```

改动/测试/审查后提交并推送任务分支，使用`gh pr create --draft`提PR。
主控维护公共文件；审查Agent不自动合并。

CI只做host smoke、Python结构测试和shell语法检查。
合并保护、Actions权限和计费额度仍需在用户实际仓库核对，不能称为已配置完成。
不要配置公网可访问板子、反向隧道、无人审批self-hosted runner或上传实体板私钥。

参考：[G1][G2][G3]见docs/REFERENCES.md。
