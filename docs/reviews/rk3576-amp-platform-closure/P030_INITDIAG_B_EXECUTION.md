# P030 initdiag-v1 执行记录与静默根因

后续更新：用户已执行一次 v2 B，source/FIT/M0 entry/配套 Linux 通过，首次 mdelay 返回未观察到；当前暂停重跑。见 [最新实板记录](P030_SOURCE_FIX_V2_B_EXECUTION.md)。以下保留 v1 诊断与 v2 暂存时的事实。

日期：2026-10-04。**v1 脚本格式被实际 U-Boot parser 拒绝；默认 Debian 冷恢复已确认；修正版已被动暂存，B 尚未验收。**

## 1. 最初用户报告（保留原始观察）

```text
=> load mmc 0:2 0x4c000000 /amp-p029/initdiag-v1/stage-B.scr
3062 bytes read in 8 ms (373 KiB/s)
=> source 0x4c000000
## Executing script at 4c000000
=>
```

用户报告 COM5 10 秒没有变化、COM6 没有输出。当时只有摘录，未记录该次内存内容或散列，因此当时写为结果不明。后续已查明固定 v1 文件的确定性拒绝原因，见第 3 节。

## 2. 最新 COM5 全文：另一组命令次序与恢复证据

附件 SHA256：`ef3da91dc0cf43f8f5869082a640547d6c39893ec9e3aa700ab579acea044464`。原文含登录凭据，未复制进项目或桌面交付物。行号按附件文本计：

| 行号 | 实际观察 |
| --- | --- |
| 157–161 | policy=0；先执行 source；明确打印 `PROJECT: source closed by policy or loaded-image bounds`，这次调用在解析脚本前被拒绝 |
| 162–164 | 随后 load v1，读入 3062 字节；下一行开始新启动记录，全文中该次 load 后未出现 source |
| 219–220、233、254、320 | SPL U-Boot `7d8fe670d9`/control DT `43164981ef` 均 OK；soc cold boot；proper g149b1c5；policy=0 |
| 327、380、424、1257、1357 | 原 boot.scr → kernel → 默认 root p3、无 amp_test_stage → Debian 登录 → 6.1.99-rk3576 #8 |

这份全文与最初“load 后 source”摘录的次序不同，不能当作同一条命令链。它证明全文中的 source 调用没有分派脚本，并证明随后默认冷恢复成功；不能把该次仅 load 记作 B 执行。

## 3. 静默原因：v1 的脚本长度表结束项不兼容

实际固件源码 `cmd/source.c` 的 `amp_project_source_legacy_script` 要求 `table[1] == 0`，否则在 `run_command_list` 前返回失败，且该分支不打印原因。v1 的 CRC、长度、文本均正确，但表为 `(0xbae, 0xffffffff)`。因此固定 SHA 的 v1 文件即使先正确 load，也会在脚本正文前被拒绝，不会到达首行 banner 或 amp_m0load。

原检查只核 CRC/长度/文本，遗漏了实际 bounded parser 对结束项的条件；这是主机准备检查的遗漏。修正版把结束项改为 0 并重算两处 CRC，正文、签名 FIT 与 U-Boot 保持原字节。

真实 source 函数编译到 Host 回归：190 项检查通过，旧 v1 拒绝且无正文分派，修正版接收并捕获完整正文。硬件命令以 stub 捕获，没有启动固件。这证明文件格式修复，不能代替板上 B、验签、mapping/cache 或 RPMsg 实测。

## 4. 修复与板端暂存

只新增 `/boot/amp-p029/initdiag-source-fix-v2`，修正版脚本 3062 字节，SHA256 `2f9bcdf69de2e8b72cdbcd8453b72122720a207529bdff3f2db1d97d36aff0d8`。安装器读回、安装后独立三项 SHA256SUMS 和回执散列核对通过；原 factory 六文件/链接、既有 P029/v1 文件与 metadata 保持。默认 root p3，无 amp_test_stage。RAM 包及安装源已清理。

Agent 未执行 source、启动 M0、加载 KO、访问 MMIO 或重启。COM6 完整日志仍未收到，修正版 B 尚未执行，C/D 保持关闭。

下一次人工操作只按 [修正版指南](P030_INITDIAG_SOURCE_FIX_V2_GUIDE.md)，保存双路完整日志。详细机器证据见 [JSON](P030_INITDIAG_SOURCE_FIX_V2.json)。
