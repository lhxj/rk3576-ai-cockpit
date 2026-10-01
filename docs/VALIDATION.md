# 启动包检查记录

检查日期：2026-10-01。执行位置：生成包的Linux x86_64容器，不是用户WSL或鲁班猫。

## 实际通过

- CMake 3.31.6 configure/build（Presets v3，工程最低要求3.21）。
- GNU C++ 14.2.0，C++17，启用常见告警并作为错误处理。
- CTest：2/2通过（common metadata + scaffold smoke）。
- Python unittest：6/6通过（文档、配置、任务依赖、dry-run、SSH选项、presets）。
- 所有Bash脚本 `bash -n` 语法检查。
- JSON / TOML解析及Agent必填字段检查；YAML解析。
- Python源码语法解析。
- 本地Markdown相对链接检查。
- 模拟SSH测试：probe成功、认证失败退出码传播、inventory本地保存、独占锁拒绝并发。
- 包路径与敏感信息检查：不含.git、构建产物、SDK/模型、真实凭据、原始板日志。

## 没有执行 / 不能由上述检查替代

- 没有登录用户GitHub、创建仓库或Issue，也没有实际执行GitHub Actions。
- 没有连接用户WSL/鲁班猫，没有采集、播放、推理、刷机或修改任何boot文件。
- 没有验证用户Codex安装版本/账户模型权限；配置按官方文档准备，需WSL启动核验。
- 没有进行AArch64交叉构建、板端部署或Qt/MPP/RKNN/RPMsg实际运行。
- Hardware PASS来自用户此前提供的证据，不是本包的自动测试结果。

首次在用户WSL执行 `bash scripts/dev/host_ci.sh` 后，请将实际版本和结果追加到新记录，
不要覆盖此包检查记录或把它改成“板端通过”。
