# RK3576 AMP board evidence closure

结论：**C. HOST_BUILD_PASS**。截至 2026-10-02，尚无足够真实板端证据设计唯一可信的 LubanCat-3 v2 BUS M0 AMP 内存、启动和 RPMsg 配置。派生 echo Host 构建通过的历史结论保留；本轮没有构建、修改或启动板端固件。

| Gate | 本轮取得的证据 | 裁决 |
| --- | --- | --- |
| CON16/CON17 | TRM 定义与 HAL 寄存器地址 SOURCE_VERIFIED；普通只读板端日志/文件未暴露当前值，尤其 CON17 | **BLOCKED**；见 [CON16_CON17_BOARD_EVIDENCE.md](CON16_CON17_BOARD_EVIDENCE.md) 与 [APPROVAL_REQUIRED_READ.md](APPROVAL_REQUIRED_READ.md) |
| 当前 boot chain | cmdline 报 `uboot-8f53f800da-04/24/2026`、`bl31-v1.14`，BOARD_OBSERVED_READONLY；同指纹公开 U-Boot 源有可选 `CONFIG_AMP` fragment，实际板镜像/config 与 BL31 SMC 均未证明 | **BLOCKED**；见 [BOOT_CHAIN_BOARD_EVIDENCE.md](BOOT_CHAIN_BOARD_EVIDENCE.md) |
| AMP FIT 来源 | 固定 U-Boot 源 `amp_cpus_on()` 只从 GPT 名称 `amp` 读取；当前 eMMC 仅 `uboot/boot/rootfs` | **AMP_PARTITION_REQUIRED** 对该公开 loader 成立；实际板 loader 是否启用未证；见 [AMP_LOAD_SOURCE.md](AMP_LOAD_SOURCE.md) |
| RPMsg | M0 link `0x04`、MBOX0→Linux、MBOX4→M0 有源码依据；当前 DT 没启用 MBOX/RPMsg，shared PA 缺 CON17 | `RPMSG_RESOURCE_CANDIDATE`，板端端到端 **UNVERIFIED** |
| Coherency | shared window 为 WBWA，M0 cache hook 为空；三种可接受机制无一证明 | **UNVERIFIED** |
| Recovery | Linux-only `/boot` 文件与 hash 已识别；未取得实际 bootloader/BL31 镜像、可核对恢复介质和 USB-TTL/Maskrom 演练 | **BLOCKED** |

`0x47800000` 是候选 M0 FIT payload 的物理装载地址，也被 CPU3 参考 DTS 用作 vring0；两份设计原样合并会重叠。当前板没有 AMP 节点，尚未产生运行时冲突。**不**填新的 load、Linux vring PA 或 CON17 推算值，见 [MEMORY_LAYOUT_CANDIDATE.md](MEMORY_LAYOUT_CANDIDATE.md)。

只读盘点使用 `scripts/board/amp_board_evidence_readonly.sh` 和 `scripts/board/amp_boot_package_readonly.sh`，以 board lock 和非特权 `ssh lubancat` 执行。原始输出保存在未提交的 `artifacts/local/amp-board-evidence-20261001T155533Z-187081/` 与 `artifacts/local/amp-boot-package-20261001T155602Z-187208/`。无 board write、特殊寄存器访问或 M0 release。

进入 D 的先决证据：经批准取得当前 CON16/17 及 cache 属性证据；实际 U-Boot binary/config 与 BL31 SMC 能力；FIT 实际来源与布局；准确的 shared Linux PA 与无重叠 reserve；可用的离线恢复镜像、介质和 console 路径。以上任一缺失仍保持 C。
