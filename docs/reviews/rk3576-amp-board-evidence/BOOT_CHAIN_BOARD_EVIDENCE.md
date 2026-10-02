# 当前 LubanCat-3 v2 boot chain：只读证据

## BOARD_OBSERVED_READONLY

- Linux 为 `6.1.99-rk3576 #8`，板端 `/proc/cmdline` 自报 `ddr-v1.09-2f85f4b2d4,bl31-v1.14,bl32-v1.05,uboot-8f53f800da-04/24/2026`。这是各阶段写入的**版本指纹**，不是镜像 hash、编译配置或 SMC 功能测试。
- eMMC GPT 为 `uboot` 8 MiB、`boot` 128 MiB、`rootfs` 约 29 GiB；`/boot` 是 ext2。`/boot/boot.cmd:6-99` 从 `uEnv/uEnv.txt` 取参数，加载 `/Image`、`/rk-kernel.dtb`/overlay，再 `booti`。脚本中没有 AMP FIT 文件加载；不排除脚本执行前的 U-Boot `board_late_init()` 路径。
- `/boot` 和常见 `/usr/lib`、`/lib/firmware`、`/opt` 下未找到现成 U-Boot、BL31、AMP FIT 镜像。板上未安装 `u-boot` 软件包，`u-boot-tools` 是用户空间工具。`/dev/mmcblk0p1` 对普通 `cat` 用户为 `root:disk 0660`，本轮未读原始分区、未提权。
- 有界 `dmesg` 与 `/sys/firmware` 普通节点未提供 `CONFIG_AMP`、`amp_cpus_on`、BL31 MCU SMC 调用或 CON17 实值。当前运行 DT 无启用的 AMP/RPMsg 节点。

## SOURCE_VERIFIED 与边界

公开 [LubanCat U-Boot `8f53f800da2c25d0c6ba414fb45902a01675703a`](https://github.com/LubanCat/u-boot/tree/8f53f800da2c25d0c6ba414fb45902a01675703a) 短哈希与 cmdline 一致；Host pinned clone 已核 `git rev-parse HEAD`。`configs/rk3576-amp.config:1-3` 是**可选 fragment**，设 `CONFIG_AMP=y`、`CONFIG_ROCKCHIP_AMP=y`；基础 `rk3576_defconfig` 没有 `CONFIG_AMP`。`arch/arm/mach-rockchip/board.c:512-513` 仅在 `CONFIG_AMP` 时于 `board_late_init()` 调 `amp_cpus_on()`。公开源码存在能力，不证明当前板 binary 编入该 fragment。

[同源 `rk3576.c:255-275`](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/arch/arm/mach-rockchip/rk3576/rk3576.c) 的 BUS M0 release 通过 `sip_smc_mcu_config()` 请求 BL31 配置 code base，然后写 gate/reset；未在此路径找到 CON17 写入。当前 `bl31-v1.14` 无镜像 hash、构建 ID 或 SIP 能力证据；[rkbin RK3576 发布说明](https://github.com/rockchip-linux/rkbin/blob/master/doc/release/RK3576_EN.md) 也不能仅凭版本字符串匹配实际板上二进制。

**实际 U-Boot AMP 能力 = UNVERIFIED；实际 BL31 BUS M0 SMC 能力 = UNVERIFIED；当前启动链有无有效 AMP FIT 入口 = UNVERIFIED。** 后续需取得供应商烧录 manifest 与当前板 loader/BL31 镜像的一一对应、构建配置和验证策略。不能以公开源码或 `boot.cmd` 单独推断当前固件。

## 2026-10-02 补充：实际 eMMC loader 镜像只读取得

上面的“未读原始分区、未取得实际镜像”是 **2026-10-01 当轮事实**，现由新证据更新而不删除历史。用户本轮解除读取限制后，以 `lsblk` 确认 `/dev/mmcblk0p1` 为 8 MiB `uboot` 分区，`sudo dd if=/dev/mmcblk0p1 bs=1048576 count=8 status=none` 经 SSH 只读流送至 Host 忽略目录。复制退出 0、长度 8,388,608，分区镜像 SHA256 `ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`。没有写 eMMC。

`dumpimage -l` 识别到带 `sha256,rsa2048:dev` 配置签名的 FIT，包含 U-Boot、`atf-1/2/3`、OP-TEE 与 FDT。提取的 U-Boot 子镜像 SHA256 `c084f257e761b898c45d681a2d1b739ca2cdd99b3916b64f3e9b34109d7a865d`，字符串指纹为 `U-Boot 2017.09-g8f53f800da-241224 #jiawen (Apr 24 2026 - 16:49:57 +0800)`；`atf-1` SHA256 `1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07`，含 `bl31-v1.14`、`v2.3-859-gc481e5368`。这是当前分区镜像的 **BOARD_OBSERVED_READONLY** hash/静态内容，不是公开源码与二进制完全匹配的证明。

在实际 U-Boot 子镜像中未搜到公开 `rockchip_amp.c` 的诊断文本 `AMP Error`、`Brought up amps` 等，故 **实际未启用 AMP 是有根据的推断，而非已证明的配置位**。实际 BL31 有 SiP 应答，但对 CON16/17 的 `SIP_ACCESS_REG` 只读均返回 `-4`；这不能证明或否定独立的 BUS M0 release SMC。参见 [CON16_CON17_RUNTIME_DIAGNOSTICS.md](CON16_CON17_RUNTIME_DIAGNOSTICS.md)。

因此目前可纠正先前“实际 U-Boot/BL31 镜像不可获得”的阻塞；但 **`CONFIG_AMP`、BUS M0 SMC 和 AMP FIT 加载入口仍未闭合**。

## 2026-10-02 补充：用户实机 U-Boot 串口

用户手动运行 `version`、`printenv bootcmd bootdelay` 与 `base` 成功，实机版本与提取镜像指纹一致；当前默认启动环境为 `run distro_bootcmd;boot_android ${devtype} ${devnum};boot_fit;bootrkp;`，`bootdelay=0`、memory offset `base=0`。这些是 **BOARD_OBSERVED_USER_LOG**，完整关键原文见 [UBOOT_SERIAL_RUNTIME.md](UBOOT_SERIAL_RUNTIME.md)。此证据确认了交互入口与当前环境，尚未确认 `CONFIG_AMP` 或 CON16/17 数值。

## 2026-10-02 补充：官方 SDK BL31 与板端镜像逐字节匹配

用户提供官方 manifests 后，固定 `db55f9658b2460b40e6e873d391e86d1b29e2916` 的历史 `20260424` 清单 pin：U-Boot `8f53f800da2c25d0c6ba414fb45902a01675703a`、rkbin `58a39b47f77a26a1e110fa1a1ce80bcfcb0b3505`。从后者取 `rk3576_bl31_v1.14.elf`，SHA256 `e11e2c86320638b533492a3a78634fe2116f53da539d91dab84d219932fa07e7`；三个非空 PT_LOAD 文件 payload 与当前 eMMC 提取的 atf-1/2/3 全字节一致（**HOST_TESTED**）。

这补齐了上文“发布二进制尚未匹配”的缺口；实际 U-Boot `.config`、BL31 MCU 配置调用成功、CON17 当前值和实际 FIT 来源仍未知。MCU 配置分支有反汇编静态证据，不能用匹配文件身份代替动态能力验证。详见 [SDK_MANIFEST_EVIDENCE.md](SDK_MANIFEST_EVIDENCE.md) 与 [MCU_MAPPING_ALTERNATIVE_PATH.md](MCU_MAPPING_ALTERNATIVE_PATH.md)。本轮无板端操作。

## 2026-10-02 补充：CON17 caller 与状态保存表

固定源码 `board_late_init()` → `amp_cpus_on()` → FIT standalone → RK3576 release 仅调用 CODE selector 1，未调用 shared selector 3。对应板端 BL31 中后者写 CON17 的值来自 caller，不是固定默认；另有 EL3 的 CON16/17 保存/恢复表，不能用该表的 buffer-pointer 零推断寄存器值。实际 U-Boot payload 未命中公开 AMP loader 的关键字符串，“可能未编入”只标 SOURCE_INFERRED，实际 `.config` 仍缺。SMC 返回值及 standalone handler 错误的传播缺口已记录。详见 [CON17_BOOT_CALL_PATH.md](CON17_BOOT_CALL_PATH.md)。没有取得当前 CON17，没有板端操作，不升级 D。
