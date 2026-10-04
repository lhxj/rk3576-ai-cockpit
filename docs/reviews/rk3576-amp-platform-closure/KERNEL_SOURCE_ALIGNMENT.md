> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 当前 Linux kernel source/headers 对齐

板端只读 `uname -a` 与 `/proc/version`：`6.1.99-rk3576 #8 SMP Fri Apr 24 16:46:57 CST 2026`、AArch64、编译器 `aarch64-none-linux-gnu-gcc 10.3.1`。`/lib/modules/6.1.99-rk3576/build` 指向板上 `/usr/src/linux-headers-6.1.99-rk3576`；存在 `.config`/`Module.symvers`，二者只读复制 Host 后 SHA256 分别 `f1f1dad7b2987c425b2807919952c57176a56f660cb1b5a5dd662893513191ee`、`bebc886c6f4ef09ae0d38fc077cb25877f1d70c32e760736814bd032d596b19c`。`/lib/modules/.../source` 不存在。

公开 [LubanCat kernel `lbc-develop-6.1`](https://github.com/LubanCat/kernel/tree/lbc-develop-6.1) 在板 build 日期前有候选 commit `521833e2d28decbd6473d5717f1f96cc4108e208`；日期相关不等于实际 source match。板镜像中的 Git commit/tag/SDK manifest 未找到，**BLOCKED_KERNEL_SOURCE_MATCH**。从当前板 headers/Module.symvers Host Kbuild 成功只证明 ABI/配置候选编译，不证明公开仓代码逐字匹配、模块可加载或可用于板端。

当前 `.config`：`CONFIG_RPMSG_ROCKCHIP_MBOX=y`、`RPMSG_VIRTIO=y`、`RPMSG_NS=y`、`ROCKCHIP_AMP=y`、`CONFIG_MODULES=y`、`CONFIG_MODULE_SIG=n`；`RPMSG_CHAR/CTRL/TTY=n`。这是 Linux kernel 配置，**不证明 U-Boot `CONFIG_AMP`**。确切 source/manifest 与相同编译器/符号 CRC 的板级 KO gate 保持未闭合。
