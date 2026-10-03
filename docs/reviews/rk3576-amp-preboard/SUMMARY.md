# RK3576 / LubanCat-3 v2 M0 AMP 上板前闭合结论

**P029阶段A脚本修复（2026-10-03）：** 首次旧A在STOP length Image退出，用户filesize=0x2930200；vendor formatter与旧bare字符串失配。已修复六处0x比较并清旧值，175 vendor C/112 fault项等Host检查及独立审查PASS。仅新增script-fix-v5目录，旧10文件/3receipt及原默认六文件保留，独立读回PASS，RAM清理，仍原Linux。新A脚本2368B，用户需按新路径冷启动；没有M0/重启，整体C、D关闭。见 [脚本修复](../rk3576-amp-platform-closure/P029_STAGE_A_SCRIPT_FIX.md)。以下保留历史。

**P029阶段A暂存PASS（2026-10-03）：** 用户确认连接恢复并批准本任务所有操作；仍按A→B→C验收。当前板P028完整8MiB只读SHA匹配，Host stdin安装器一次执行成功，boot10/module268/3receipt读回PASS，原六内容和启动链接/路径自身metadata前后相同，RAM源包及root快照前缀均清理。首轮未压缩上传只有部分RAM包，持久目录尚不存在；一次压缩传输修复后安装，失败历史保留。原Linux仍运行，无Agent重启/M0/KO/MMIO操作。**A冷启动待用户手动执行，C. HOST_BUILD_PASS，D关闭。** 见 [实际暂存与交接](../rk3576-amp-platform-closure/P029_STAGE_A_RESULT.md)。以下保留历史。

**P029 Host准备完成（2026-10-03）：** 已交付A/B/C独立DT/脚本、配套Linux/initrd/模块、有界M0诊断FIT和回滚指南；21文件桌面SHA复核PASS。fresh M0 build零warning，67脚本/包 +14实际M0 C +6snapshot故障项、实际P028 preload/libfdt及full Host CI PASS。只Host，未访问实板；A暂存/配套Linux、B/C有效mapping/cache/RPMsg均UNVERIFIED，未取得本轮部署批准。用户只有一个Debug USB-TTL，A足够，B/C UART5采集待落实。**C. HOST_BUILD_PASS，D关闭**。 见 [审核与下一步](../rk3576-amp-platform-closure/P029_HOST_PREPARATION_REVIEW.md)。以下保留历史证据及其当时状态。

**等级：C. HOST_BUILD_PASS（保持）。** 派生 M0 echo 已 Host clean build；M0 架构属性也修成 ARMv6S-M。但参考 FIT load 与 RPMsg vring 真正同址，CON17 alias 未证明；当前板没有 `amp` 分区、AMP/RPMsg DT 节点，MBOX0/4 disabled；cache/恢复/签名仍未闭合。不得部署。

| READY 条件 | 结果 |
| --- | --- |
| M0 源码身份、Arm GNU clean build、echo 编入 | PASS；ELF/Map/bin 135,256 B |
| 七 warning 分类和必修 warning | PASS：6 个无关 demo warning 不再编入；RWX 裸机 ELF 提示保留且说明；旧 RPMsg callback 签名已修 |
| 物理内存无冲突、`0x47800000`、M0 alias | **BLOCKED**：FIT 实际装载与参考 vring 同址；CON17 值/写入路径未知 |
| link-id / MBOX / vring / buffer / NS | **BLOCKED**：ring 数值与 service 源码可对齐；参考 Linux 为 link3/MBOX3，M0 目标 link4/MBOX4；当前 DT 缺节点，alias/cache 未证 |
| coherency/barrier | **BLOCKED**：DSB 有据；M0 cache module enabled 但 RPMsg cache hook 空；实际 shared memory 属性未知 |
| LubanCat UART/pinctrl | 部分 PASS：当前 UART5 disabled、Linux console ttyFIQ0；M0 m0 管脚函数已核；v2 物理与后续 overlay 互斥仍待核 |
| I2C 配置 | PASS：实际构建来自生成 `rtconfig.h`；派生 defconfig→.config→rtconfig.h 一致且 echo 不使用 I2C |
| FIT/load/entry | **BLOCKED**：形状/装载源码清楚，Host FIT 未签名；无 amp 分区与 U-Boot feature 运行证据 |
| 当前 boot 盘点 | PASS：只读盘点与关键 hash 完成；运行 FDT 哈希不可读 |
| rollback / 精确 changeset | **BLOCKED**：草案已成，U-Boot/分区镜像、最终 DT/FIT hash、离线恢复演练缺失 |

最短闭合路径：取得当前版 U-Boot/BL31 及 CON17 确证 → 设计并 Host 验证无重叠物理 DDR，生成 LubanCat 专用 DTS 而非照搬 CPU3 样例 → 对齐 Linux/M0 mailbox4、vring、DMA/cache → 锁定 FIT 签名与 U-Boot AMP 分区装载机制 → 基于准确 kernel headers Host 编译小型 Linux echo peer → 形成可恢复且可审查的最终变更清单。下一轮若任何条件仍未知，继续停在 C；即便达 D，也须另行取得用户明确批准才可上板。

详见本目录其余 15 份 gate 报告及 `HOST_VALIDATION.md`，原 `architecture-audit` 历史报告未改。
