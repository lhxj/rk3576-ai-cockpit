# RK3576 AMP BSP 候选审查摘要

审查日：2026-10-01。**结论等级：C. HOST_BUILD_PASS**。这表示固定候选的默认 M0 RT-Thread 工程在 Host 上生成了 ELF/map/bin；**不是** AMP VERIFIED，也**未达到** D. READY_FOR_CONTROLLED_BOARD_TEST。

## 证据边界

| 等级 | 本次可说的事实 |
|---|---|
| 仓库声称 | `PROVENANCE.md` 声称来自 Forlinx OK3576 SDK 的 Rockchip RT-Thread 4.1.1 和 HAL。来源链尚无原 SDK manifest/hash 独立核验，且 RTOS provenance 声称删除 `forlinx_*.c`，固定提交仍有这些文件。 |
| 代码证明 | `rk3576-mcu` 使用 `-mcpu=cortex-m0 -mthumb`、M0 vector/startup、RK3576 HAL、RPMsg-Lite RK3576 port；隔离 worktree 的默认 SCons exit 0，ELF ARMv6-M/EABI5。 |
| 实板验证 | 本轮零 SSH、零启动、零硬件访问。当前板 DTB/boot/FIT/时钟/IRQ/地址映射尚无运行态核验。 |

候选源固定在 `/home/ywx/rk3576-work/reference/rk3576-amp/rk3576-rtos`（`7c397f41751feb29b0b388dfda3d2c2225f1f87c`）和 `/home/ywx/rk3576-work/reference/rk3576-amp/rk3576-hal`（`277de3fd4b0e640654ee73bb3308be2ef01e3aad`）；两处 `git status --short` 为空。原 `common/hal` 链接以 SDK `<SDK>/hal` 为目标，按本次两个仓库命名会悬空；Host 构建仅在 `/tmp/rk3576-amp-audit` 通过同级 `hal` 映射修复。

## 决定性结果

1. **BSP / M0：是。** `rk3576-mcu/.config`、`rtconfig.py`、`hal_conf.h`、HAL `start_m0.S` 与 ELF 的 ARMv6-M/soft-float 互相支持；`rk3576-64` 是另一个 Cortex-A53/AArch64 BSP，不能用其固件替代 M0。
2. **HAL：已闭合默认编译路径，未闭合板级所有权。** RK3576 register、MBOX/CRU/I2C/UART/GPIO/timer/NVIC/pinctrl/cache 实现存在并被默认构建；PM 功能默认关闭，BSP_Init 为空，时钟/复位仍依赖外部启动配置。`STATIC_AUDIT.md` 逐项给代码位置。
3. **RPMsg-Lite：传输组件存在，但目标链未闭合。** 默认 `RT_USING_COMMON_TEST_LINUX_RPMSG_LITE=n`，没有实际 `rpmsg_task` 或服务进入 ELF。实验开启示例后能链接，但 `rpmsg_ns_cb` 类型不符、初始化函数签名不符；示例用 `remote_id=3`/`link-id=0x03`，而 Linux DTS 注释将 MCU 标为 `0x04`，port 的 MCU client 表只初始化 0–3。缓存维护函数为被注释的空实现。详见 `RPMSG_ALIGNMENT.md`。
4. **Linux 对齐：存在阻断差异。** RTOS linker RPMsg M0 地址是 `0x27d00000`，物理别名尚无证明；若映为 `0x47d00000`，与 Linux DTS 的 `0x47800000` vring 不同且 4 MiB 区重叠该 DTS 的 MCU reserve。更直接的是候选 FIT 的 firmware load **也是 `0x47800000`**，与参考 vring 同址。Linux 样例同时是 CPU3 `0x03` 路径。两端服务名示例 `rpmsg-mcu0-test` 可匹配 Linux `rockchip_rpmsg_test`，但当前内核是否含/启用该 test driver 未证。
5. **LubanCat：只有 UART5 m0 pinmux 社区提交可确认。** RTOS EVB 的 I2C/SPI/PWM/GPIO 默认初始化未与 LubanCat v2 的 RTC、codec、Type-C、camera 等资源隔离；Linux 参考 AMP DTS 选 UART5 m2，与候选 EVB 选 m0 不同。当前运行 DTB 不等于公开固定源码，仍须只读核验。
6. **Boot/recovery：未闭合。** `mkimage.sh` 只拷贝 bin 并调用随仓工具 `mkimage` 生成 `amp.img`；未运行。其 FIT `load=0x47800000`、签名 `dev` key、U-Boot/BL31/现板分区与回退链都未核验。

下一步文件级变更建议列在 `BOARD_VALIDATION_PLAN.md`；本轮没有对这些文件实施修改。许可证问题见 `LICENSE_AUDIT.md`。历史 `architecture-audit` 报告未改写。
