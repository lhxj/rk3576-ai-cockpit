# 冷启动 Host proposal 的内存布局

**DRAFT_NOT_DEPLOYABLE / C. HOST_BUILD_PASS。** 地址来自唯一 contract、固定 M0 port 的旧 PA 减 `0x20000000` 规则、现有 FIT/code 区及本轮实际 ELF。以下假定未来经过检查的 SiP CODE=`0x47800000`、SRAM selector=`0x40000000` 并完成 MCU reset；不是当前 CON16/17 读数。

公式：`PA = B16 + M0_code_address`；`PA = B17 + M0_shared_address - 0x20000000`。拟用 B16=`0x47800000`、B17=`0x40000000`。底部 10 bit 的寄存器定义仍不代填；这里只约束用于 remap 的 `[31:10]`。

| Region | Linux PA | M0 view | Size | Owner / Attr | Purpose |
| --- | ---: | ---: | ---: | --- | --- |
| vectors | `0x47800000` | `0x00000000` | 412 B | RTOS / cold cache bypass | MSP/vector；Reset_Handler 本地 Thumb `0x141` |
| text + rodata | `0x478001a0` | `0x000001a0` | 119,616 B | RTOS / bypass | `.text` 包含只读数据 |
| zero table/exidx/extab | `0x4781d4e0` 起 | `0x0001d4e0` 起 | 208 B及对齐间隙 | RTOS / bypass | 初始化表与 unwind |
| data | `0x4781d5b0` | `0x0001d5b0` | 2,456 B | RTOS / bypass | 实际 `.data`；`__data_start__` 在其对齐后的 `0x1d5c0` |
| bss | `0x4781df80` | `0x0001df80` | 11,304 B | RTOS / bypass | NOLOAD，初始化清零 |
| heap | `0x47820ba8` | `0x00020ba8` | 389,208 B | RTOS / bypass | 到 `0x7fc00`；线程栈从 heap 分配 |
| main stack | `0x4787fc00` | `0x0007fc00` | 1,024 B | RTOS / bypass | 初始 MSP `0x80000`，向下生长 |
| code/data reserve + FIT payload load | `0x47800000` | `0x00000000` | `0x80000` | RTOS / Linux no-map | 同一物理区，FIT 对 BIN 的最终 load；不能另当临时 buffer |
| vring0 | `0x47d00000` | `0x27d00000` | `0x8000` | SHARED_CONTROLLED / Linux Device | Linux rx / M0 tx |
| vring1 | `0x47d08000` | `0x27d08000` | `0x8000` | SHARED_CONTROLLED / Linux Device | Linux tx / M0 rx |
| payload pool | `0x47d10000` | `0x27d10000` | `0x10000` | SHARED_CONTROLLED / Linux Normal-NC WC | 128 × 512，64/方向；每帧 16 header + 496 payload |

实际 ELF 的 2 个 PT_LOAD 为 RX/RW，互不相交、均在 512 KiB 内；共享区不在 PT_LOAD，BIN 122,696 B。M0 SRAM没有用于本候选；旧 `LINUX_ATAGS@0x20000000` linker NOLOAD 描述仍保留，但 ATAGS 功能未启用、不在 PT_LOAD，也未被本应用使用；它不是已分配的第三块共享区。

**0x47800000 的处理：** CPU3 参考 DTS 的同址 vring0 不用于本 M0 overlay。M0 proposal 的 vring0 是 `0x47d00000`，与 code `[0x47800000,0x47880000)` 不重叠。旧 4 MiB M0共享预留由 128 KiB 实际几何替代；4 MiB 从 `0x47d00000` 起本来也不包含更小的 `0x47800000`，不能错误地把缩小 pool 当作这一冲突的原因。SDK kernel pin 的 CPU3示例又采用 `0x47c00000`，应与早期 Rockchip参考版本区分。

BOARD_OBSERVED_READONLY：运行 DT 的首段 RAM=`[0x40200000,0x48400000)`；候选两块 reserve 均在其中，没有与该 DT 的现有 reserved-memory 相交。HOST_TESTED：DT合并后既有节点/属性保留（仅 MBOX0/4 status/poll改变及增加候选节点），payload从专用 pool分配。**不等于 U-Boot load时动态 RAM allocator/启动所有权已经实测。** Linux reservations只在内核启动后生效。

FIT `entry=0x141` 与 ELF 本地 Thumb entry相同；实际 vendor standalone loader 用 **FIT load**配置 CODE window，忽略 FIT entry。有效 reset vector的物理指令位置拟为 `0x47800140`；不能写“Linux PA entry=0x141”。
