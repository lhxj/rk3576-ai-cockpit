> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 当前 U-Boot 串口观察（收到于 2026-10-02）

证据来源：用户通过 MobaXterm 在实机 `=>` 提示符下手动输入，并上传串口原始文本。证据等级 **BOARD_OBSERVED_USER_LOG**；本次 Agent 未连接板端或发送串口命令。

```text
=> echo OK
OK
=> version
U-Boot 2017.09-g8f53f800da-241224 #jiawen (Apr 24 2026 - 16:49:57 +0800)

aarch64-none-linux-gnu-gcc (GNU Toolchain for the A-profile Architecture 10.3-2021.07 (arm-10.29)) 10.3.1 20210621
GNU ld (GNU Toolchain for the A-profile Architecture 10.3-2021.07 (arm-10.29)) 2.36.1.20210621
=> printenv bootcmd bootdelay
bootcmd=run distro_bootcmd;boot_android ${devtype} ${devnum};boot_fit;bootrkp;
bootdelay=0
=> base
Base Address: 0x00000000
```

`?` 正常列出的命令包括 `version`、`printenv`、`base`、`md`、`boot`、`boot_fit`、`download`、`rockusb`。此前列表内命令报 Unknown command、手动输入后正常，支持输入/粘贴问题的解释；隐藏字符或串口 TX 的确切根因未测，不写为已修复某项硬件故障。

当前 U-Boot 的版本/工具链、命令可用性、默认启动环境与 memory-display offset 均有实机证据。`base=0` 使完整 `md.l 26004060 1` 的请求地址不带额外偏移。`boot_fit` 命令存在不证明 AMP loader 编入或启用，默认 `bootcmd` 也不能单独排除更早的 AMP 初始化。

## 后续实读结果：CON16 Data Abort

用户先误输入 `.1`（数字）两次，返回提示符而无数值；固定源码对无效后缀直接返回，不能视为实读。随后手动执行正确的 `md.l 26004060 1`，触发 `ESR_EL2=0x96000010`，日志以 `### ERROR ### Please RESET the board ###` 结束；没有寄存器值或正常命令返回。CON17 未读。

这条 U-Boot 直接访问路径已失败，停止进一步 MMIO。详见 [CON16_UBOOT_READ_ABORT.md](CON16_UBOOT_READ_ABORT.md)。如果还停在 panic，保存日志后由用户重新上电、不打断默认启动；Linux 恢复状态尚待确认。不推算 CON17、不改 AMP linker/ITS/DTS。整体仍 **C. HOST_BUILD_PASS**。

后续用户确认：重新上电后 Linux 正常启动（**BOARD_OBSERVED_USER_REPORT**）。实际恢复已确认，不重试直接 MMIO；CON16/17 数值仍未知。
