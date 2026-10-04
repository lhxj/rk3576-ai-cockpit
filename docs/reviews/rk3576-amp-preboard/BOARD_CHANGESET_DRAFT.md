> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 板级 changeset 草案（未部署）

**当前无可部署 changeset。** 表中 `UNRESOLVED` 是刻意阻塞，不用推测目的分区或新地址。任何 boot/DTB/分区动作均需下一轮明确批准和独立恢复演练。当前文件 hash 来自只读盘点；新 hash 仅对已经生成的 Host 候选给出。

| Source → Destination | Old hash | New hash | Purpose | Risk | Rollback |
| --- | --- | --- | --- | --- | --- |
| 准确的 LubanCat kernel tree `arch/arm64/boot/dts/rockchip/rk3576-lubancat-3-v2.dts` + **新板级 AMP `.dtsi`/overlay** → `/boot/dtb/rk3576-lubancat-3-v2.dtb` 或独立文件：**UNRESOLVED** | 当前 boot DTB `76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90` | **UNRESOLVED** | reserve 不重叠 DDR、MBOX0/4、RPMsg link4、UART5 Linux 释放；保留 CAM0 | 误 reserve/IRQ/pinctrl 可使 Linux 无法启动 | 恢复原 DTB + uEnv，离线介质应可操作 |
| 派生 `rtos/.../gcc_link.ld.S` → Host RTOS build | 固定/派生单文件 SHA256 均为 `37e542a29bd3e8dd26fb61df16f5114e16e493f1afd79d107979d5a49bf07b87` | 待设计的地址版 **UNRESOLVED** | 统一 M0 RPMsg window 与经证明的 CON17 mapping | 物理地址错误会破坏 Linux DDR | 回退候选 commit；板端不运行 |
| 派生 `rtos/.../Image/amp.its` → Host `amp.img` / 实板装载位置 **UNRESOLVED** | 固定/派生 ITS 单文件 SHA256 均为 `eca92c11337a61c23154c31c48d5cfbb4c2e4f14e914c88c2523f3d6ad91bd4a`；当前板无 AMP FIT | Host 未签名 FIT `6fb95fd6544c6582666faa4fe08321bfa623497e1d0ac957622ca5243af9d4d2`；最终签名新 hash **UNRESOLVED** | M0 load/entry/签名 | 当前 `0x47800000` 与参考 vring 重叠；签名 unavailable | 从 boot 流程移除 AMP loadable；恢复 Linux-only |
| 派生 `rtos/.../rtthread.bin` → FIT `images/mcu`，板上目的地 **UNRESOLVED** | 无当前板 RTOS | Host `641c5813266a57443a40fcb5c545e60ffe56a135cd8fb07d65e7dd9bfaef484c` | 最小 M0 echo | alias/cache/mailbox 未闭合 | 不释放 M0，恢复原 boot 流程 |
| 当前 `/boot/uEnv/uEnv.txt` → 下一轮变更位置 **UNRESOLVED** | `4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64` | **UNRESOLVED** | 如采用 overlay，需显式选择；当前 UART5 overlay 注释 | 错误 overlay 影响 boot/相机 | 原文件逐字还原，核 SHA |
| Kernel config/source → board kernel/module | 当前 Image `e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e`；config 各项见 baseline | **UNRESOLVED** | 选择 B Linux echo peer；现有 RPMsg transport=y，可能只需匹配模块，需 exact kernel headers | ABI/模块签名/版本不合 | 卸载或移除 test 模块；保留原 Image |
| U-Boot `configs/rk3576-amp.config`、`rockchip_amp.c` + BL31 固件 → eMMC `uboot` 分区或其他：**UNRESOLVED** | 当前 U-Boot 分区固件 hash **未只读整分区获取**；cmdline 指纹 `8f53f800da` | **UNRESOLVED** | 开 AMP loader、证明 CON17、提供 FIT 来源 | 最高风险，可能断 boot | 必须先有离线 eMMC 恢复镜像和可用写入介质，下一轮单独批准 |
| eMMC partition table / `amp` 分区 → **UNRESOLVED** | 仅 `uboot/boot/rootfs`，无 amp | **UNRESOLVED** | 板版本 U-Boot AMP loader `AMP_PART="amp"` | 分区改动可能毁 rootfs；本轮不提实施地址/大小 | 完整 eMMC 离线镜像与分区表恢复演练 |

必须先得到：exact kernel/U-Boot/BL31 构建版本与配置、CON17 值/写入者、签名策略、内存避让方案、boot 介质恢复方法。没有这些，不能给上述 UNRESOLVED 填任意物理地址/目的路径/新 hash。
