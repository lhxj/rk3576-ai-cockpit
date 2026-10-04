> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# CON16/CON17：用户批准后的单次只读尝试

2026-10-02，用户明确修改此前“不得访问板端”的禁令，批准仅对 `0x26004060`、`0x26004064` 各作一次 32-bit 只读尝试；第一条异常立即停止。没有批准写寄存器、启动 M0 或修改启动链。每次板端连接前均通过 `scripts/board/_common.sh:board_lock()` 取得 WSL 用户级锁；本轮未改 SSH 配置或板端文件。

| 顺序 | 命令 / 检查 | 原始结果 | 结论 |
| --- | --- | --- | --- |
| 1 | 旧 SSH 别名 `10.34.122.223:22`，预检 `which devmem` | SSH 连接超时；远端命令未执行。Windows TCP 22 检查亦失败。 | 旧 IP 不可达 |
| 2 | 用户提供 `10.232.249.223`；临时 `HostName` override | 已保存的 ed25519 host key 与旧 IP 相同，SSH 连接成功；未改本地 SSH 配置。 | 新 IP 可达 |
| 3 | `which devmem` | stdout/stderr 空，exit 1 | 无独立 `devmem` 命令 |
| 4 | `uname -a` | `Linux lubancat 6.1.99-rk3576 #8 SMP Fri Apr 24 16:46:57 CST 2026 aarch64 GNU/Linux`，exit 0 | 内核身份核对 |
| 5 | `command -v busybox`、`busybox --list`、`ls -l /dev/mem`、`sudo -n -l /usr/bin/busybox` | `/usr/bin/busybox` 含 `devmem` applet；`/dev/mem` 为 `root:kmem crw-r-----`；现有 sudo 策略列出 `/usr/bin/busybox`。 | 未安装软件；选用现有 applet |
| 6 | `sudo -n /usr/bin/busybox devmem 0x26004060 32` | **只尝试一次；stdout 空，stderr 空，exit 1**。板端 sudo 日志确认该命令曾启动并结束。 | **CON16 值未取得；失败原因未证** |
| 7 | 随后 `uname -a`、有界 kernel journal | SSH 与 `uname` 正常；所见有限 kernel journal 无可归因于本次读取的错误。 | 板端仍响应；不能据此证明 MMIO 安全 |
| 8 | `devmem 0x26004064 32` | **未执行** | 第一条异常后停止 |

**证据等级：** 命令与退出码、板端仍可响应为 `BOARD_OBSERVED_READONLY`；寄存器值、是否到达 MMIO 总线、失败根因均为 `UNVERIFIED`。退出码 1 可能发生在用户态 `/dev/mem` 访问、内核限制或总线/安全防火墙等不同阶段，不能据此断言 CON16 具有读副作用，也不能推断 CON17 数值。没有重试、没有切换其它寄存器访问路径。

`CON16_RUNTIME_EVIDENCE=BLOCKED`；`CON17_RUNTIME_EVIDENCE=BLOCKED`；`M0_LINUX_ADDRESS_MAPPING=UNRESOLVED`；相对于当前 CON17 的 `0x47800000` 冲突仍 `UNRESOLVED`。静态 [访问语义裁决](CON16_CON17_ACCESS_SEMANTICS.md) 未因失败尝试而改变；整体 AMP 维持 **C. HOST_BUILD_PASS**。
