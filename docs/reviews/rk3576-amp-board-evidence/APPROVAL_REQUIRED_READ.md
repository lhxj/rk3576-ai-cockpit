> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# APPROVAL_REQUIRED_READ：CON16/CON17

本轮权限边界由用户明确规定：如果只能通过 `devmem`、`/dev/mem`、U-Boot console 或其它特殊寄存器访问取得 CON16/17，**先停下、列出请求，未经批准不执行**。普通只读 SSH 的现有日志和文件未暴露寄存器值。

| 寄存器 | 系统物理地址 | 申请后的建议纯读命令，**未执行** | 结果用途 |
| --- | ---: | --- | --- |
| `SYS_SGRF_SOC_CON16` | `0x26004060` | `devmem 0x26004060 32` | 读 code 映射字段 `[31:10]`，核对候选 FIT load 与 M0 code window |
| `SYS_SGRF_SOC_CON17` | `0x26004064` | `devmem 0x26004064 32` | 读 shared base `[31:10]`，换算 M0 `0x27d00000` 对应 Linux PA，确定 vring/buffer 区间 |

地址依据：[RK3576 TRM v1.2 §8.6.2](https://rockchip.fr/Rockchip%20RK3576%20TRM%20V1.2%20Part1.pdf) 与固定 HAL `soc.h:651-652`。命令仅描述 **32-bit 读取意图**；`devmem` 实现通常需特权 MMIO，可能被内核拒绝、触发 secure firewall/bus fault，寄存器读取本身也有平台相关副作用风险。尚未验证板上工具是否存在、账户权限或总线访问安全性；不能把“命令无写参数”当作零风险。若批准，应先由板端负责人确认可读性和允许的方式，并保持 board lock、单次有界读取、时间戳和原样记录。若需 U-Boot console，则会中断正常启动/涉及重启，不属于本轮自动获准范围。

读到当前值仍不能单独证明 **谁写、何时写、软复位后是否保持**；还需与实际 U-Boot/BL31 镜像和源码对应。这个请求不授权写 CON16/17、M0 release 或修改启动链。
