# RK3576 / LubanCat-3 v2 M0 AMP 上板前闭合结论

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
