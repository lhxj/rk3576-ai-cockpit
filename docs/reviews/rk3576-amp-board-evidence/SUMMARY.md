# RK3576 AMP board evidence closure

结论：**C. HOST_BUILD_PASS**。截至 2026-10-02，尚无足够真实板端证据设计唯一可信的 LubanCat-3 v2 BUS M0 AMP 内存、启动和 RPMsg 配置。派生 echo Host 构建通过的历史结论保留；本轮没有构建、修改或启动板端固件。

**最新 U-Boot 实读（2026-10-02，用户串口日志）：** 实机版本、`bootcmd/bootdelay/base` 已确认，见 [UBOOT_SERIAL_RUNTIME.md](UBOOT_SERIAL_RUNTIME.md)。正确的 `md.l 26004060 1` 触发 `ESR_EL2=0x96000010`：当前级 Data Abort、读取方向、同步外部访问异常；没有 CON16 数值，日志停于 `Please RESET the board`。CON17 未读，停止直接 MMIO 尝试。见 [CON16_UBOOT_READ_ABORT.md](CON16_UBOOT_READ_ABORT.md)。Linux 恢复尚待确认；本次 Agent 只作 Host 分析与记录。以下“没有重启/没有特殊寄存器访问”等陈述保留对应历史轮次的边界，不覆盖本次用户操作。

**用户原始 TRM 核验（2026-10-02）：** 已直接读取两份桌面 PDF。TRM 确认 SYS_SGRF 基址、CON16/17 映射字段；`+0x60/+0x64` 偏移来自固定 HAL，不能称为 TRM 寄存器表证据。TRM 同时明确 BUS MCU 的 16 KB unified I/D cache、reset bypass 与 CON14/15 非缓存区配置机制。当前 CON17 和共享区 cache 状态仍没有数值/运行时证据，等级不变。见 [TRM_SOURCE_RECONCILIATION.md](TRM_SOURCE_RECONCILIATION.md)。

**最新寄存器排障（2026-10-02）：** 用户解除上轮失败即停的限制后，`strace` 证明 Linux `devmem` 对 CON16 的实际 load 触发 `SIGBUS/BUS_OBJERR`；当前 BL31 的 Rockchip SiP 安全寄存器只读服务对 CON16 和 CON17 均返回 `SIP_RET_INVALID_ADDRESS=-4`，没有有效数值。一次性诊断 `.ko` 已从板端 `/dev/shm` 删除、无驻留模块，但加载行为使运行中内核 `tainted=4096`，直到重启才清除；板端未重启。只读复制了当前 eMMC `uboot` 分区并提取实际 `bl31-v1.14` 镜像，静态地址表不能提供当前 CON17 值。详见 [CON16_CON17_RUNTIME_DIAGNOSTICS.md](CON16_CON17_RUNTIME_DIAGNOSTICS.md)。**当前 M0/Linux 地址换算和 vring Linux PA 仍无法数值确定。**

**CON16/17 runtime read 补充（2026-10-02）：** TRM §1.1 的 `SYS_SGRF=0x26004000`、§8.6.2 的 `[31:10]` 映射与固定 HAL 的 `+0x60/+0x64` 一致。公开 TRM 没有两寄存器的逐项读访问属性，HAL 读函数不足以排除 MMIO 读取副作用。按用户前置停止条件，**没有执行任何板端命令或 `devmem`**。`CON16_RUNTIME_EVIDENCE=BLOCKED`、`CON17_RUNTIME_EVIDENCE=BLOCKED`、`M0_LINUX_ADDRESS_MAPPING=UNRESOLVED`；当前 CON17 与 `0x47800000` 的关系未判定。见 [CON16_CON17_RUNTIME_READ.md](CON16_CON17_RUNTIME_READ.md)。

**后续用户授权与尝试（2026-10-02）：** 用户明确修改上述禁令。新 IP `10.232.249.223` 可连；板端无独立 `devmem` 命令，但已装 BusyBox applet。CON16 只读命令仅尝试一次，退出码 1 且无 stdout/stderr；板端仍可通过 SSH 回应。按约定未读 CON17、未重试 CON16。两个当前值及 Linux PA 映射仍未知；见 [CON16_CON17_APPROVED_READ_ATTEMPT.md](CON16_CON17_APPROVED_READ_ATTEMPT.md)。静态门禁见 [CON16_CON17_ACCESS_SEMANTICS.md](CON16_CON17_ACCESS_SEMANTICS.md)。

| Gate | 本轮取得的证据 | 裁决 |
| --- | --- | --- |
| CON16/CON17 | TRM/HAL 定义已核；Linux CON16 load SIGBUS、SiP 两地址均 -4、U-Boot CON16 Data Abort；没有有效值 | **BLOCKED**；见 [CON16_UBOOT_READ_ABORT.md](CON16_UBOOT_READ_ABORT.md) 与 [CON16_CON17_RUNTIME_DIAGNOSTICS.md](CON16_CON17_RUNTIME_DIAGNOSTICS.md) |
| 当前 boot chain | cmdline 版本之外，已只读取得当前 eMMC `uboot` 分区和 U-Boot/BL31 子镜像 hash；当前 SiP 寄存器读服务拒绝两个目标地址。U-Boot `CONFIG_AMP` 与 BUS M0 SMC 功能仍未证明 | **BLOCKED**；见 [BOOT_CHAIN_BOARD_EVIDENCE.md](BOOT_CHAIN_BOARD_EVIDENCE.md) |
| AMP FIT 来源 | 固定 U-Boot 源 `amp_cpus_on()` 只从 GPT 名称 `amp` 读取；当前 eMMC 仅 `uboot/boot/rootfs` | **AMP_PARTITION_REQUIRED** 对该公开 loader 成立；实际板 loader 是否启用未证；见 [AMP_LOAD_SOURCE.md](AMP_LOAD_SOURCE.md) |
| RPMsg | M0 link `0x04`、MBOX0→Linux、MBOX4→M0 有源码依据；当前 DT 没启用 MBOX/RPMsg，shared PA 缺 CON17 | `RPMSG_RESOURCE_CANDIDATE`，板端端到端 **UNVERIFIED** |
| Coherency | TRM 列 BUS MCU 16 KB cache、reset bypass、CON14/15 非缓存范围；候选 RPMsg hook 为空，当前共享区状态未证 | **UNVERIFIED**；具有 bypass/uncached 或 maintenance 的源码调查方向 |
| Recovery | Linux-only `/boot` 文件与 hash、当前 U-Boot/BL31 镜像 hash 已识别；可核对恢复介质和 USB-TTL/Maskrom 演练仍未闭合 | **BLOCKED** |

`0x47800000` 是候选 M0 FIT payload 的物理装载地址，也被 CPU3 参考 DTS 用作 vring0；两份设计原样合并会重叠。当前板没有 AMP 节点，尚未产生运行时冲突。**不**填新的 load、Linux vring PA 或 CON17 推算值，见 [MEMORY_LAYOUT_CANDIDATE.md](MEMORY_LAYOUT_CANDIDATE.md)。

只读盘点使用 `scripts/board/amp_board_evidence_readonly.sh` 和 `scripts/board/amp_boot_package_readonly.sh`，以 board lock 和非特权 `ssh lubancat` 执行。原始输出保存在未提交的 `artifacts/local/amp-board-evidence-20261001T155533Z-187081/` 与 `artifacts/local/amp-boot-package-20261001T155602Z-187208/`。无 board write、特殊寄存器访问或 M0 release。

进入 D 的先决证据：经批准取得当前 CON16/17 及 cache 属性证据；实际 U-Boot binary/config 与 BL31 SMC 能力；FIT 实际来源与布局；准确的 shared Linux PA 与无重叠 reserve；可用的离线恢复镜像、介质和 console 路径。以上任一缺失仍保持 C。
