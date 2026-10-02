# CON16：当前 U-Boot 直接读取异常

收到日期：2026-10-02。来源为用户通过 MobaXterm 上传的实机串口文本，证据等级 **BOARD_OBSERVED_USER_LOG**。本次 Agent 没有连接开发板或发送串口命令。

## 实际命令与结果

当前串口版本为 `U-Boot 2017.09-g8f53f800da-241224 #jiawen (Apr 24 2026 - 16:49:57 +0800)`；此前 `base` 返回 `0x00000000`。

用户先输入两次 `md.1 26004060 1`，均无数值而返回提示符，随后按 Ctrl+C。这里 `.1` 是数字后缀。固定 U-Boot `common/command.c:412-435` 对未知后缀返回 `-1`，`cmd/mem.c:71-72` 随即返回，尚未执行 memory load。**SOURCE_INFERRED**：按对应源码解释该无输出；不能把它写成寄存器值为零或 MMIO 失败。

之后用户只执行一次有效命令。原始日志关键摘录如下；其余 GPR 和 stack 输出省略：

```text
=> md.l 26004060 1
26004060:"Synchronous Abort" handler, esr 0x96000010

* Reason:        Exception from a Data abort, from current exception level
* PC         =   00000000402c9830
* LR         =   00000000402c978c
* ESR_EL2    =   0000000096000010
* Reloc Off  =   00000000bda0d000

Resetting CPU ...

### ERROR ### Please RESET the board ###
```

`26004060:` 是显示地址标签，**不是 CON16 读数**。命令未返回可用寄存器值，也未回到正常 `=>`；没有执行 CON17 读取。日志中的 `Resetting CPU ...` 不能证明自动复位完成或 Linux 已恢复。

## ESR 解码

Host 用位运算脚本计算并断言以下字段，运行 exit 0（**HOST_TESTED**）：

| Field | Calculation | Value | Meaning |
| --- | --- | --- | --- |
| EC | `(esr >> 26) & 0x3f` | `0x25` | 当前异常级的 Data Abort |
| IL | `(esr >> 25) & 1` | `1` | 32-bit 指令 |
| ISS | `esr & 0x1ffffff` | `0x10` | Data Abort syndrome |
| WnR | `(esr >> 6) & 1` | `0` | 数据读取方向 |
| DFSC | `esr & 0x3f` | `0x10` | 同步外部访问异常 |
| ISV / FnV / S1PTW | bits 24 / 10 / 7 | 全为 `0` | 无额外有效指令 syndrome；日志没有 FAR，不能补造 FAR |

定义依据（**SOURCE_VERIFIED**）：当前板复制的 headers `arch/arm64/include/asm/esr.h:48,71-83,108-111`；亦与 [Linux v6.1 ARM64 ESR 定义](https://github.com/torvalds/linux/blob/v6.1/arch/arm64/include/asm/esr.h) 一致。日志显示 `ESR_EL2`，不应将本次异常记为 Linux EL1 或 BL31 EL3 的直接读取。

固定 U-Boot SHA `8f53f800da2c25d0c6ba414fb45902a01675703a` 的 `arch/arm/lib/interrupts_64.c:184-196` 在 `do_sync()` 打印 syndrome/registers 并进入 `panic("Resetting CPU ...")`，与所见日志一致（**SOURCE_VERIFIED**）。这不是可以继续执行下一条寄存器命令的正常返回。

## 裁决与边界

- **BOARD_OBSERVED_USER_LOG**：当前 U-Boot 此路径直接读 CON16 发生 Data Abort；未取得数值。命令是读操作，但对无副作用属性仍未取得新的证明。
- **SOURCE_INFERRED**：安全域/总线防火墙限制可解释这一症状；ESR 不能唯一证明具体拒绝模块、策略或 register read semantics。
- **UNVERIFIED**：CON17 在 U-Boot 的直接可读性、两个寄存器实际值、谁配置 remap 和 cache 属性。
- 停止 U-Boot `md` 和 Linux `devmem` 的直接读取路线；不试其它地址、宽度、别名或未知 SMC，不写防火墙/CON16/CON17。
- 最初的恢复建议为保存日志后手动重新上电、不打断默认 Linux-only 启动；该建议的后续实际结果见下一段。

后续用户确认：重新上电后 Linux 正常启动（**BOARD_OBSERVED_USER_REPORT**）。这完成了本次异常后的启动恢复确认；不证明完整备份/Maskrom 恢复链已演练。本次没有新的板端读取。下一步可调查的静态配置路径见 [MCU_MAPPING_ALTERNATIVE_PATH.md](MCU_MAPPING_ALTERNATIVE_PATH.md)。

| Result | Status |
| --- | --- |
| `CON16_RUNTIME_EVIDENCE` | `BLOCKED`；Linux load `SIGBUS`、SiP `-4`、本次 U-Boot Data Abort |
| `CON17_RUNTIME_EVIDENCE` | `BLOCKED`；SiP `-4`，未做 U-Boot 直接读取 |
| `M0_LINUX_ADDRESS_MAPPING` | `UNRESOLVED`；缺有效 CON17 |
| `0x47800000_CONFLICT` | 原样合并的 FIT/CPU3 DTS 静态冲突仍成立；当前 M0 shared PA 关系未知 |
| AMP | `C. HOST_BUILD_PASS` |

后续取值需要板厂认可的安全固件只读接口，或有明确权限和恢复条件的诊断方案。当前失败记录不构成继续试读的依据，也不允许据此猜测 linker、ITS、DTS 地址。
