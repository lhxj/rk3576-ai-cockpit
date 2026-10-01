# 当前 BL31 M0 AMP 能力：UNVERIFIED / BOOT_CHAIN_BLOCKED

板端只读 cmdline 指纹为 `bl31-v1.14`，没有取得 BL31 binary hash、分支/构建 ID 或 SIP 返回码。[Rockchip rkbin RK3576 release notes](https://github.com/rockchip-linux/rkbin/blob/master/doc/release/RK3576_EN.md) 中 `v1.14` 出现在不同 BL31 命名/发布线，单独版本串不足以选定 binary。公开 U-Boot `fit_standalone_release()` 调 `sip_smc_mcu_config(ROCKCHIP_SIP_CONFIG_BUSMCU_0_ID, ROCKCHIP_SIP_CONFIG_MCU_CODE_START_ADDR, entry_point)`，但当前 BL31 是否处理该 SMC、如何写 CON16/17 未证明。

进入 D 需只读取实际 BL31 image 并 hash，确定 rkbin/SDK 对应文件、SIP API 与 CON16/17 设置路径；当前禁止刷 BL31、读特殊寄存器和 release M0。本报告不把版本号当功能证明。
