> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# P028 新 U-Boot：Linux-only 实板结果

记录时间：2026-10-03 16:18:54 +08:00（客户端 Asia/Shanghai；不是独立测得的上电时间）。

**P028_LINUX_ONLY_TEST = PASS；AMP = C. HOST_BUILD_PASS；D 仍关闭。**

## 对象与证据

用户已批准仅更换 P028 U-Boot并正常启动原 Linux，M0保持关闭。此前完整8MiB读回与获批镜像逐字节相同，SHA256 `9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e`。本次串口日志确认正在运行新的 proper U-Boot `f8b4554`，SPL 检查的新payload前缀 `2e1b965bfb` 通过。

原始附件共 72874 bytes，SHA256 `5d467df0502dea1681004f1b6444b459171d90276d9cffc695cf4a0b32f34629`；原字节已保存在忽略目录 `artifacts/local/p028-linux-only-cold-boot-20261003T080230Z/user-serial.log`，不提交原始日志或其中的登录/网络信息。Agent只在Host核验、归档及解读，未访问板端。机器记录：[P028_LINUX_ONLY_EXECUTION.json](P028_LINUX_ONLY_EXECUTION.json)。

| 检查 | 本次实际观测 | 证据等级 |
| --- | --- | --- |
| proper U-Boot | `U-Boot 2017.09-gf8b4554` | BOARD_OBSERVED_USER_REPORT |
| proper U-Boot策略调用 | banner之后 `Verified-boot: 0`，继而 `PROJECT: factory boot/CLI allowed by runtime policy=0` | BOARD_OBSERVED_USER_REPORT；getter调用链 SOURCE_VERIFIED |
| 原厂启动脚本 | `/boot.scr` 被发现，在 `0x40500000` 执行 | BOARD_OBSERVED_USER_REPORT |
| Linux handoff | `booti 0x40400000 0x4a200000:0x63bcef 0x48300000`，`Starting kernel ...` | BOARD_OBSERVED_USER_REPORT |
| 原 Linux | `6.1.99-rk3576 #8`、`EmbedFire LubanCat-3-v2`、Debian12/ttyFIQ0登录提示 | BOARD_OBSERVED_USER_REPORT |
| 原 firmware | BL31 `c481e5368/v1.14`、OP-TEE `9f2aca7d1/v1.05`；SPL五项保留payload检查均OK | BOARD_OBSERVED_USER_REPORT；完整payload身份见此前 Host封装/读回核验 |
| 镜像/日志身份 | 候选、此前完整读回、附件及本地归档SHA与字节核验PASS；启动标记顺序PASS | HOST_TESTED |

SPL的 `Verified-boot:0` 与 proper U-Boot的运行时策略结果是两个独立观测。新proper U-Boot已实际获得策略0并通过原厂脚本启动路径；这不证明OTP/全系统secure boot均关闭，也不证明AMP FIT加载已成功。第一份冷启动日志只打印CLI允许策略；下述补充日志进一步验证Ctrl+C、命令帮助与手动boot。

日志开头包含此前USB/RAM恢复Loader的扫描过程，不混入后续正常冷启动裁决。后续Linux出现原有外围驱动warning和journal非干净关机提示；到登录界面是本次验收终点，不据此声称所有外设/应用回归通过。

## 补充：Ctrl+C、命令帮助与 Linux 身份

用户明确提供操作范围和第二份串口原文：自动启动时Ctrl+C → `help amp_m0load` → `boot` → Linux登录后 `uname -a` / `cat /proc/cmdline`。

第二份附件 70253 bytes，SHA256 `09ff1699c3a2a29b99cd3b006dcb0597d32ed6f0c9c1df5483f45866b034c322`；原字节仅保存忽略目录 `artifacts/local/p028-linux-only-cold-boot-20261003T080230Z/user-cli-and-linux.log`。本次只按终端返回记录结果，没有额外执行板端命令，也未把未捕获的exit code写成0。

| 检查 | 原始返回摘要 | 结论 |
| --- | --- | --- |
| Ctrl+C和CLI | `<INTERRUPT>`，进入 `=>`，可执行help/boot | PASS / BOARD_OBSERVED_USER_REPORT |
| 命令注册 | `help amp_m0load` 返回项目命令说明及 `<file> <linux_fdt_addr>` 用法 | PASS，**只证明帮助/注册** |
| 手动boot | `=> boot` → 原 `/boot.scr` → `Starting kernel` → Debian登录 | PASS / BOARD_OBSERVED_USER_REPORT |
| uname | `Linux lubancat 6.1.99-rk3576 #8 ... aarch64 GNU/Linux` | 原内核身份符合预期 |
| cmdline | root=`/dev/mmcblk0p3`、boot_part=2、ttyFIQ0、BL31 v1.14/BL32 v1.05、uboot-f8b4554 | 新U-Boot引导原Linux路径符合预期 |

`help amp_m0load` 没有执行 `amp_m0load`；命令说明中的“verified”是程序帮助文本，不能用作实际加载/MCU启动成功证据。交互CLI的已测范围仅为Ctrl+C、help和boot。补充日志从策略阶段开始，候选身份由cmdline与之前完整读回/启动日志共同支撑，不单独当作完整DDR冷启动日志。板端欢迎信息的2025时间不用于本次取证日期。

## 历史与当前剩余门

P026 `2314a3f` 启动失败与撤回、原件恢复PASS，以及14:43读回原件/15:09修正后读回P028的历史均保留。旧P026/P027 writer与canonical旧候选保持撤回。

| 下一门 | 当前状态 | 本次没有覆盖的事实 |
| --- | --- | --- |
| M0有效映射与缓存 | UNVERIFIED | 冷reset后实际code/shared映射、cache bypass及共享可见性 |
| AMP文件入口与BL31 MCU setter | UNVERIFIED | `amp_m0load`真实文件读取/复制/两阶段setter/reset release；它是启动命令 |
| 配套Linux/RPMsg | UNVERIFIED | 候选kernel/DT/modules/initrd实际部署及端到端echo |

源代码自动AMP分支已关闭；本次无M0启动命令/日志，不能把日志缺失当作M0硬件reset状态测量。新M0测试需另行形成确切物料、步骤、停止条件及回滚范围，再按AGENTS.md L3取得批准；本次Linux-only审批没有扩大。
