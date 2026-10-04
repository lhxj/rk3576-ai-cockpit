> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 来源与许可证审查

固定提交见 `SUMMARY.md`。这里是工程清单与风险定位，**不是法律意见或可再分发保证**。私有项目内部评估/修改与向第三方分发固件或源码是不同动作；后者须按实际链接文件和许可证核对。

| 范围 | 仓库中的证据 | 私有使用/修改与再分发关注点 |
|---|---|---|
| RT-Thread 主树 | `R/LICENSE` Apache-2.0；`R/PROVENANCE.md` 称 RT-Thread 4.1.1 + Rockchip BSP、提取自 Forlinx OK3576 SDK。 | Apache-2.0 通常允许私有使用、修改和分发；分发时保留许可证/版权/NOTICE 与修改标记等条件。原 SDK manifest/hash、社区修改权属未独立证明。 |
| Rockchip HAL | `H/LICENSE` BSD-3-Clause；`H/PROVENANCE.md` 称来自同一 SDK；`H/lib/bsp/RK3576/hal_bsp.c` 与 HAL 模块头有 SPDX。 | BSD-3-Clause 通常允许私有使用和修改；源码/二进制再分发需带版权、条件、免责声明，且不得用权利人名义背书。 |
| RPMsg-Lite 两副本 | `R/bsp/rockchip/common/drivers/rpmsg-lite/LICENSE` 与 `H/middleware/rpmsg-lite/LICENSE` 均为 NXP/Freescale 等版权的 BSD 形式许可；两份 `rpmsg_lite.c` hash 不同，当前构建选 R 副本。 | 保留各副本原版权、许可与免责声明；不要将 HAL middleware 的许可证或补丁自动套到 R 中实际编译的副本。 |
| RTOS M0 `applications/main.c` | 文件头明确 GPL v2-or-later，而仓库根许可证是 Apache-2.0；默认应用源码进入构建。 | 这是实际许可冲突/混合许可警示。内部试验与向外分发风险不同；发布前应查明该文件来源、是否可用兼容授权重写，以及组合固件的义务，不按根 LICENSE 一概断定。 |
| FIT `Image/amp.its` | 文件头 `SPDX-License-Identifier: GPL-2.0`；`mkimage.sh` 头 BSD-3-Clause。 | 如改编/分发 FIT 源文件需保留对应声明；工具、ITS、生成镜像分别审查。随仓 `../tools/mkimage` 未运行/未做二进制来源核验。 |
| Forlinx demo | `applications/forlinx_gpio_test.c`、`forlinx_i2c_rtc_test.c`、PWM/timer 测试仍存在，未见每文件明确 SPDX；`PROVENANCE.md` 声称已删，与固定提交不符。 | 不应假定根 Apache 覆盖这些提取文件；发布前隔离/替换并取得来源许可。默认 `Glob('*.c')` 会编译其中多份。 |
| CoreMark | `.config` `RT_USING_COREMARK=y`，`R/components/benchmark/coremark/core_main.c` 文件头指向 EEMBC CoreMark License；该目录未找到独立 license 文件。`H/middleware/benchmark/coremark/LICENSE.md` 含独立 CoreMark 商标使用协议及 Apache 文本，不能自动当成 R 目录的完整授权包。 | 默认构建启用了 CoreMark；发布前确认具体源码版本、软件许可与商标/成绩报告要求，若业务不需基准可在新派生配置中去除。 |
| 其他 third-party | `R/third_party/cmsis/LICENSE` Apache-2.0；`R/third_party/netutils/LICENSE` GPL-2.0；`H/middleware/benchmark/tinymembench/LICENSE` MIT；M0 `applications/kiss_fft` 文件头 BSD-3-Clause。 | 按**实际编译/链接**清单逐项保留声明。netutils、HAL OpenAMP 等存在于树中不代表本 M0 ELF 链接；不可反向推论所有第三方都获同一许可。 |
| 社区 UART 修改 | `git show 13735c5` 为 `uart5_m0_iomux_config` 增补与 EVB 调用变更；仓库作者不是原 SDK 许可证权利证明。 | 使用该提交时保留原文件头、补丁来源与作者信息；原 SDK 的分发授权仍需独立核验。 |

建议后续对派生固件生成 SBOM/链接对象清单，移除不用的 Forlinx demo 和 CoreMark，整理每个保留文件的许可证与 NOTICE，再决定私有部署或再分发方案。此轮仅记录，不改候选源码。
