# P030 initdiag-v1 指南已暂停

2026-10-04：已确定 v1 脚本长度表结束项不兼容实际 U-Boot source parser。不要再执行 v1 的 stage-B.scr。

修正版已被动暂存并读回，默认 Debian 冷恢复通过。下一次人工冷启动 B 请只使用 [source-fix-v2 操作指南](P030_INITDIAG_SOURCE_FIX_V2_GUIDE.md)。

原 FIT 仍在 initdiag-v1 目录，由修正版脚本引用。M0/FIT 和 U-Boot 无需重刷；C/D 仍关闭。具体历史、根因及限制见 [执行记录](P030_INITDIAG_B_EXECUTION.md) 与 [修复机器记录](P030_INITDIAG_SOURCE_FIX_V2.json)。
