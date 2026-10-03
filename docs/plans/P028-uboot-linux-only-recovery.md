# P028：候选 U-Boot 启动失败、撤回与原件恢复

2026-10-03。基线 `agent/amp-platform-closure / ebb666a`。

## 目标与事实

用户手工写入 P026 后，串口显示 proper U-Boot `2314a3f`、SPL uboot payload hash 前缀 `6ea779087d`，随后 `FIT: No FIT image / No CLI available`，原 Linux 未启动。不是 Linux-only PASS；没有 M0 启动证据。当前用户截图确认 RKDevTool 发现一个 MASKROM 设备。

## 范围和权限

- 主控：撤回旧候选和写入脚本，新增失败记录、Host 安全回归，指导用户恢复原 U-Boot。
- 子代理：独立派生源码 worktree 中修复 CLI/default distro/legacy script 三项兼容，保留验签门，建立真实 parser 回归并 Host 构建。不得编辑主控文件或接触板端。
- Agent 只执行 Host 操作。MASKROM 插拔、RAM Loader、存储恢复和上电由用户执行；每一步先核目标、再给具体动作。没有自动闪存/重启/M0 启动。
- fixed RTOS/HAL reference、旧 P026 source/build/package 保留；旧候选不能继续使用。整体 C，D 准入关闭。

## 步骤与失败恢复

1. 阻止生产 `p027_manual_uboot.sh` 再执行旧 candidate check/write；桌面镜像改为明显不可刷写名称、保留 hash。
2. 记录三项 SOURCE_VERIFIED 根因；公开文档仅提交脱敏摘录，不提交原始板日志。
3. 核原 8MiB raw 和 factory MiniLoader 的 SHA；独立工具副本关闭自动重启。仅指导 `LoaderToDDR` RAM 下载，不选择普通 `Loader`（后者写 IDBlock）。
4. 用户返回 LOADER 日志与设备分区表后，核原 GPT、EMMC、uboot start/size；再形成仅恢复原 uboot 的具体 GUI 步骤。不要猜闭源 GUI 强制写地址单位。不写 Parameter/GPT/boot/rootfs。
5. 原 U-Boot 恢复需完整读回 hash 和用户冷启动原 Linux 证据；当前尚未完成。若 partial 恢复不可执行，停止并明确完整官方镜像会覆盖用户数据，不自动转整机刷写。
6. Host 修复经主控逐项审核和实际 parser/default environment/安全策略回归后才准备新的独立候选；本轮不发布为 D，不代替用户刷写。

## 验收与待办

撤回回归必须证明 `--check`、`--write` 在任何板端/文件访问前失败、无绕过环境变量。后续 Host 回归必须覆盖 required=0/1/TA失败、原 boot.scr、CLI/distro 及 control DT overrides。Host test → review → commit → push 现有 Draft PR；不 merge。

当前：用户正在执行 RAM Loader 前置步骤，原 U-Boot 恢复未观测；Host 修复进行中。没有 Agent 板端操作。

实际Host结果：撤回脚本5项无I/O回归PASS；全Host CI exit0（2 CTest+41 Python+上述5项，shell语法PASS）；`git diff --check` PASS。机器包hash完整性仍PASS，readiness/deployment均BLOCKED并明确包含candidate withdrawn。

工具副本首次未识别MASKROM，用户确认尚未执行RAM下载。发现副本INI误为UTF-8而原版为带BOM的UTF-16LE；已只修副本编码并保留原字节备份，Win32 profile读取Kinds/Selected/RESET_AFTER_DOWNLOAD均成功。需用户重开确认；未把编码修复等同为设备已识别。
