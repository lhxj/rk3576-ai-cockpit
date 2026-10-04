> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# APPROVAL_REQUIRED_READ — 本轮未执行

已检索普通只读 `cmdline`、有限 dmesg、运行 DT、`/sys/rk_amp`、`/boot` 文件与公开源码；没有获得当前 CON16/17 的普通只读镜像。当前用户明确禁止自行执行 `/dev/mem`、`devmem`、debugfs 特殊寄存器读或 U-Boot console。**停止该部分，不推断寄存器当前值。**

| Register | Linux PA | 目的 | 风险/前提 | 建议下一轮方法 |
| --- | ---: | --- | --- | --- |
| `SYS_SGRF_SOC_CON16` | `0x26004060` | M0 code `0x0` 实际映射 PA、复位时序 | 特权 MMIO、可能受 secure-world 访问控制；读法需授权 | 由板方批准、经受信只读固件诊断或经审查的只读接口记录值；同时留 boot log |
| `SYS_SGRF_SOC_CON17` | `0x26004064` | M0 `0x20000000` shared window 实际映射 PA | 同上；须说明 reset 前后值 | 同上，明确 read timing |
| `SYS_SGRF_SOC_CON14/15` | `0x26004058/0x2600405c` | 确认 M0 cache peripheral/uncache window | 同上；寄存器定义与 buffer 属性仍需 TRM | 同上 |

还需取得当前 U-Boot、BL31 **实际镜像的离线只读副本/配置**和批准的可追溯构建记录，以证明谁写 CON16/17。仅有一次寄存器读数也不能代替写入时序和 cache 语义。不得把这里的建议理解为当前回合读板授权。
