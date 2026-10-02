# Boot/FIT 加载路径裁决：BLOCKED

## P025更新

官方AMP来源确认为GPT`amp`，当前恢复后仍无此分区；未找到该pin官方file/memory MCU入口。新Host U-Boot候选已增加cold reset/shared setter、完整config-policy/required key检查和partition边界检查，保留正常Linux boot；当前板是否含该入口/实际验签仍未证。其unsigned package仅Host封装。详见 [P025_BOOT_LOAD_AND_VERIFY.md](P025_BOOT_LOAD_AND_VERIFY.md)。没有创建分区/部署/重启；以下保留早期路径。

源码链为 BootROM → DDR loader → U-Boot `board_late_init()`（仅 `CONFIG_AMP` 时 `amp_cpus_on()`）→ 按名称 `amp` 查 GPT 分区 → `blk_dread()` 读取 FIT → `boot_get_loadable()` 核验并把 standalone payload 写到 FIT `load` → `fit_standalone_release("bus_mcu",entry_point)` → `sip_smc_mcu_config(...MCU_CODE_START_ADDR,entry_point)` → U-Boot 写 TOP_CRU gate/reset → M0 从映射后 vector 启动 → Linux boot。来源：[LubanCat U-Boot board.c](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/arch/arm/mach-rockchip/board.c)、[AMP loader](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/drivers/cpu/rockchip_amp.c)、[RK3576 release hook](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/arch/arm/mach-rockchip/rk3576/rk3576.c)。这是源码路径，**不是板端已证启动路径**。

`AMP_PART="amp"` 是 `part_get_info_by_name()` 的 GPT partition label，不是 `/boot` 文件、resource、FIT subimage 或环境变量。当前板只读分区表为 `uboot/boot/rootfs`，无 `amp`；这条公开 loader 在当前布局找不到输入。未找到已验证的不改分区替代加载机制，不为规避 GPT 改动而臆造从 boot filesystem 加载。当前板的 U-Boot `CONFIG_AMP`、实际镜像、BL31 M0 SMC、FIT 签名策略均未知，见分别报告。

首次测试的 Linux 先/后启动顺序只可在 AMP loader 和最终 FIT 确证后确定。当前派生 M0 echo 会等待 link-up 并每 5 秒打印 heartbeat，避免完全无诊断，但并不具备 Linux/M0 热重启自恢复证明。
