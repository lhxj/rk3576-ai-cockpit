# P004：IMX6ULL Qt 参考审查同步与 cockpit_ui 路线修订

状态：HOST_TESTED

## 目标与验收

把 2026-10-01 对 `build-QTMenu-IMX6U_rsync-Debug.rar` 的静态审查结论
同步到主项目，并把 P004 从“旧项目迁移”改为基于当前接口重新实现
RK3576 `cockpit_ui`。验收条件是参考角色、迁移策略、许可状态、功能映射、
UI 工作包和未实现状态在文档与任务清单中一致，且 Host CI 通过。

## 已有证据及未知项

- 仓库外 `reference/imx6ull-qt/analysis/` 已完成静态盘点；归档是 qmake
  Shadow Build，原始 Qt 源码不完整，内含 ARM32 ELF/object、生成文件与
  许可未核验资源。
- 可恢复五入口菜单和音乐/视频/传感器交互轮廓；不能作为可编译源码基础。
- 板端 Qt 版本、桌面会话、平台插件与实际显示 mode 仍未盘点。
- `cockpit_ui` 业务代码仍为 NOT_IMPLEMENTED；本任务不改变该状态。

## 文件范围 / 负责人 / Agent

主控 Agent 单独维护：`docs/STATUS.md`、`docs/REFERENCES.md`、
`docs/THIRD_PARTY.md`、`docs/architecture/`、`docs/tasks/`、
`apps/cockpit_ui/README.md`、`docs/reviews/reference-audit/imx6ull-qt/` 与本计划。
不修改 `docs/reviews/architecture-audit/` 历史报告，不修改业务代码。

## 依赖 / 权限等级 / 板端独占

仅 L0：本地文档、JSON 和 Host 测试。不访问开发板，不运行归档 ELF，
不安装或下载 Qt，不占用相机、声卡、NPU 或 AMP 资源。

## 分步操作

1. 保存当前 Git 状态和历史架构审查报告哈希，创建任务分支。
2. 阅读仓库外静态审查结果和仓库内架构、状态、任务、UI 文档。
3. 新增正式参考审查记录与旧功能到新模块映射。
4. 修订 cockpit_ui 边界、单 shell/页面路由、线程约束和服务数据流。
5. 将 P004 改为重新实现路线，并定义 UI-01 至 UI-10 工作包。
6. 校验 JSON、文档关键字段、历史报告哈希与 Git diff。
7. 运行 `bash scripts/dev/host_ci.sh`。

## 测试：命令、预期结果、实际结果、退出码

- `python3 -m json.tool docs/tasks/manifest.json`：JSON解析成功；P004包含
  UI-01至UI-10共10个工作包；退出码0。
- 关键字段/禁用旧路线文本检查：`SHADOW_BUILD`、`INCOMPLETE`、
  `UI_REFERENCE_ONLY`、`REIMPLEMENT`、`LICENSE_UNVERIFIED` 已在状态、参考、
  架构、任务与正式审查记录中一致；没有未修正的直接移植任务。
- 历史审查报告 SHA256：`REVIEW.md`、`REFERENCE_MATRIX.md`、
  `NEXT_ACTIONS.md` 分别保持
  `33c77248...b0779`、`cd04c4d0...53cd`、`190a6ee4...d7dc`，与修改前一致。
- `bash scripts/dev/host_ci.sh`：退出码0；2/2 CTest、6/6 Python测试通过，
  输出 `HOST_SCAFFOLD_CHECKS_PASSED (not hardware validation)`。

## 失败恢复与停止条件

若 manifest 校验失败，只修正本轮新增字段或依赖，不删除测试。若 Host CI 暴露
既有失败，记录真实输出并停止扩大范围。禁止通过执行旧 ELF、改板端或安装 Qt
来补充本任务证据。

## 风险：资源、隐私、版本、第三方许可

旧包整体保持 `LICENSE_UNVERIFIED`。不复制其图标、歌曲、视频、生成文件、
ELF、object、Makefile 或 sysroot 配置。仓库外归档和分析目录不加入 Git。

## 交接：提交、文档、待办

已完成文档同步、功能映射、P004/UI-01至UI-10路线修订和Host验证。未访问
开发板，未运行参考ELF，未复制参考内容。代码实现留给后续P004工作包；
启动实现前仍需确认首版页面范围、板端Qt/session/platform plugin和实际显示mode。
