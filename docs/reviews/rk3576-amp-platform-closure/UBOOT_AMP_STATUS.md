# 当前 U-Boot AMP 能力：UNVERIFIED / BOOT_CHAIN_BLOCKED

## P025更新

原板/恢复包payload静态身份已在P024匹配；恢复后的fwver相同，raw分区本轮未重读。新派生`96c9a009eed997c318cd247943e5fc88e8e1bcc6`已完整fresh Host构建（AMP/FIT_SIGNATURE=y），形成保留原ATF/OPTEE/DT的unsigned双slot FITpackage；不能把新候选能力当作当前板CONFIG_AMP。官方pin仍只找GPT`amp`，当前板没有；实际policy/key未知。见 [P025加载/验签](P025_BOOT_LOAD_AND_VERIFY.md) 和manifest。下文是早期证据范围。

板端 cmdline 自报 `uboot-8f53f800da-04/24/2026`（BOARD_OBSERVED_READONLY）。公开 [LubanCat U-Boot commit 8f53f800da2c25d0c6ba414fb45902a01675703a](https://github.com/LubanCat/u-boot/tree/8f53f800da2c25d0c6ba414fb45902a01675703a) 有 AMP loader；`board.c:board_late_init()` 由 **`CONFIG_AMP`** 控制，而读取的 `configs/rk3576_defconfig` **没有列 `CONFIG_AMP`**。板上实际 `.config`/U-Boot image 不可由 `uname` 或这个 defconfig 推定；`/dev/mmcblk0p1` 对普通用户不可读，本轮未用 sudo/设备读。不能称当前 U-Boot 支持 AMP。

`drivers/cpu/rockchip_amp.c:AMP_PART="amp"` → `part_get_info_by_name()` → `blk_dread()` → `images.verify=1` → `boot_get_loadable()`。当前 GPT 无 `amp` 分区，按此公开路径即使启用 `CONFIG_AMP` 也会在查分区时返回 `-ENODEV`。需取得实际 bootloader 镜像、配置/签名 policy 和可信源码构建关联；若考虑新的无分区 loader，须单独实现、审查、Host 验证和恢复设计，本轮没有这样的方案。
