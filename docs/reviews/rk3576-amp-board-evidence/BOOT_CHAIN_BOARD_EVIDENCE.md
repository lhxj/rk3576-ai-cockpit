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
