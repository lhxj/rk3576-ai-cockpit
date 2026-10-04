> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 当前 BL31 M0 AMP 能力：UNVERIFIED / BOOT_CHAIN_BLOCKED

## P025/P024新增证据

原实板/恢复包ATF1与SDK选定payload的SHA256已匹配：`1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07`。MCU CODE/shared setter分支及CON16/17目标写入静态追踪通过；P025重跑验证器exit0。新Hostloaderpackage保留此ATF及OPTEE字节，未更新BL31。恢复后cmdline仍v1.14；实际目标SMC返回和映射/reset有效状态尚未测试，故SOURCE_VERIFIED但BOARD_RUNTIME能力仍UNVERIFIED。见 [P025结果](P025_CLOSURE_RESULT.md)。下文“未取得hash”是早期事实，不是当前缺口。

板端只读 cmdline 指纹为 `bl31-v1.14`，没有取得 BL31 binary hash、分支/构建 ID 或 SIP 返回码。[Rockchip rkbin RK3576 release notes](https://github.com/rockchip-linux/rkbin/blob/master/doc/release/RK3576_EN.md) 中 `v1.14` 出现在不同 BL31 命名/发布线，单独版本串不足以选定 binary。公开 U-Boot `fit_standalone_release()` 调 `sip_smc_mcu_config(ROCKCHIP_SIP_CONFIG_BUSMCU_0_ID, ROCKCHIP_SIP_CONFIG_MCU_CODE_START_ADDR, entry_point)`，但当前 BL31 是否处理该 SMC、如何写 CON16/17 未证明。

进入 D 需只读取实际 BL31 image 并 hash，确定 rkbin/SDK 对应文件、SIP API 与 CON16/17 设置路径；当前禁止刷 BL31、读特殊寄存器和 release M0。本报告不把版本号当功能证明。
