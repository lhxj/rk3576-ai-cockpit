# 实测最小 AMP/RPMsg 配置输入

状态：BOARD_TESTED_MINIMAL_CHAIN / FROZEN。见 [集成 tip](../../../docs/amp/AMP_RPMSG_INTEGRATION_TIP.md) 和 JSON 的精确输入/产物 hash。

- `m0-transport.dtso`：复制实际用于配套 DT 构建的 transport overlay 输入，三段 no-map、独占 DMA pool、link4、RX MBOX0/TX MBOX4，UART5 Linux驱动 disabled。mcu-amp 仅提供已规划 pin/clock lease，没有 amp-cpus 再次启动 M0。
- `amp-signed.its`：实际 v5 signing 输入。load0x47800000、entry0x141、code512KiB/shared128KiB、窗口方案0x40000000、签名conf覆盖loadables。输入 totalsize0x1fc00 经固定vendor工具对齐后，实测 FIT根totalsize/实际长度均0x20000；不手工后改签名属性。`p029dev`只是公钥提示名，私钥不交付。
- `stage-C.cmd`：复制实测3100B SCRIPT的3028B正文，SHA8ec8d5c18a7f46b8649a7f1eef54c460932162a77a45d1148d799bf347133826，保留 root override、正确0x filesize、预加载Linux DT、单次loader和失败冷恢复。

最终 C DT 是固定 paired kernel 基础DT + 原厂CAM0 overlay +本transport overlay，再由 `scripts/amp/p029_make_packet.py` 的C分支加chosen stage并生成；不是仅编译本overlay即可替代的完整板DT。CAM0原输入为保持既有Linux兼容，未合媒体模块。实际 C DTB308309B/SHA dd68818b7fd27e9abc56522d7c4782adb0b1dd49d274fe1daa75a32ad3366fbf。

SCRIPT由 `scripts/amp/p030_prepare_rpmsg_c_v1.py` 封装：PPC/Linux/script/none、单组件长度表(len,0)、data/header CRC。签名FIT/C DT/SCRIPT/KO冻结产物hash是本次交接身份；旧生成器/旧脚本用于历史来源，不直接写板。

本目录无完整 SDK/固件/私钥，仅归档原已测项目配置输入。本轮未修改这些输入内容或重新构建。板端双向通信日志与冷恢复见集成 tip。
