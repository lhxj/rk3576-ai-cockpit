> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# FIT / ITS / loader 形状

`Image/amp.its`: `images/mcu`，`description="bus_mcu"`，`type="standalone"`，`arch="arm"`，`compression="none"`，`load=0x47800000`，`udelay=1000000`，SHA256 hash；`configurations/conf` 的 `loadables="mcu"`，包含 `sha256,rsa2048` PSS `dev` 签名节点；**无 `entry` 属性**。板版本 U-Boot `boot_get_loadable()` 把 `rtt.bin` 复制到 load，standalone release 用 load 作 entry，经 BL31 SMC 把 M0 code address 0 映射到该 PA。该链在源码层闭合，当前板 AMP feature/分区/签名策略未闭合。

| 文件 | 生成 / 用途 | 本轮状态 |
| --- | --- | --- |
| `rtthread.elf` | SCons 链接，M0 entry `0x141`（Thumb Reset_Handler 地址 `0x140`），含符号/map | PASS，不能当作 U-Boot payload |
| `rtthread.map` | linker 地址、section、符号审计 | PASS |
| `rtthread.bin` | objcopy 裸镜像，135,256 B，最终 clean build SHA256 `641c5813266a57443a40fcb5c545e60ffe56a135cd8fb07d65e7dd9bfaef484c` | PASS，未运行 |
| `Image/rtt.bin` | `mkimage.sh` 从 rtthread.bin 拷贝的 FIT 输入 | 仅 Host 临时复制 |
| `amp.img` / FIT | `mkimage.sh` 用候选 `../tools/mkimage` 生成；本轮改用系统 `/usr/bin/mkimage -f ... -E -p 0xe00` 在忽略的 Host 目录生成 | 结构生成 PASS；最终 SHA256 `6fb95fd6544c6582666faa4fe08321bfa623497e1d0ac957622ca5243af9d4d2`；**签名值 unavailable，非可部署产物** |
| `rttmcu.bin` | 候选 SCons/mkimage 不生成 | 不需要作为这条 FIT loader 的独立输入 |

`mkimage -l` 证实 payload 135,256 B、load `0x47800000`、entry unavailable、配置 loadables=mcu、hash 正确、RSA 签名 **unavailable**。因为未提供 dev 私钥，也未查明板端 U-Boot 的签名/rollback 策略，不推断 unsigned FIT 能通过 `images.verify=1`。当前板无 `amp` 分区，loader 无读取位置。**FIT/load/entry Gate = BLOCKED**，且 load 与参考 RPMsg vring PA 冲突。来源：[LubanCat U-Boot loader 固定源码](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/drivers/cpu/rockchip_amp.c)、[RK3576 release hook](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/arch/arm/mach-rockchip/rk3576/rk3576.c)。
