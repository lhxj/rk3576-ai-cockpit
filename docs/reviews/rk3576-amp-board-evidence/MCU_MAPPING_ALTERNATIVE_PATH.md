> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# CON16/17 直接读取失败后的替代路线

2026-10-02：用户确认重新上电后 Linux 正常启动（**BOARD_OBSERVED_USER_REPORT**）。本次 Agent 仅分析 Host 文件，没有 SSH、MMIO 或 SMC 调用。等级仍 **C. HOST_BUILD_PASS**。

## 新取得的静态证据

来源是此前从当前 eMMC 只读复制的 U-Boot FIT：`uboot-partition.img` SHA256 `ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`。`dumpimage -l` 确认 `atf-1` load 为 `0x40040000`；提取文件 SHA256 `1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07`，含 `bl31-v1.14` 字符串。

固定 U-Boot [MCU SiP 定义](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/arch/arm/include/asm/arch-rockchip/rockchip_smccc.h) 给出 `SIP_MCU_CFG=0x82000028`、BUS MCU ID `0`、CODE selector `1`、SRAM selector `3`（**SOURCE_VERIFIED**）。这里的 SRAM 名称对应 MCU `0x20000000` window 的配置功能，不自动限定为片上 SRAM 的物理目标。

实际 BL31 文件内得到以下反汇编（地址是按 FIT load 计算的 Host VMA，**HOST_TESTED**；高层接口解释为 **SOURCE_INFERRED**）：

| VMA | 行为 | 解释 |
| --- | --- | --- |
| `0x4005eb28–0x4005eb40` | 保留原 w0 为调用号，将原 x1/x2/x3 移至 x0/x1/x2 | MCU ID / selector / value 参数流 |
| `0x4005ebb0–0x4005ebbc` | 构造 `0x82000028`，匹配后跳到 `0x4005ec98` | 与固定 U-Boot MCU 配置 ABI 一致 |
| `0x4005ec98–0x4005eca8` | MCU ID 非零拒绝；selector 1 和 3 分流 | 对应 BUS MCU 配置 |
| `0x4005eccc–0x4005ecf8` | selector 1 验地址大于 `0x00800000`、1 KiB 对齐；写 CON16 等配置 | code window 配置分支 |
| `0x4005ed04–0x4005ed14` | selector 3 验 `value & 0x3ff == 0`；将 w2 写 `0x26004064` | CON17 配置分支；**写操作** |

selector 1 不只写 CON16：该代码还写 `0x26004020=0x01000100`、`0x26004058=0x20000000`、`0x2600405c=value`，最后 `0x26004060=value`。因此必须连同 CON14/15 等配置评估，不能把它称为单寄存器无影响操作。selector 3 将参数写 CON17，未看到此分支附带 read-back。

寄存器写指令使用 32-bit `w2`；未来调用参数必须证明在可表示的 32-bit 物理地址范围内，不能把超过 4 GiB 的参数直接传入并忽略高位截断。

Host helper `artifacts/local/check_bl31_mcu_mapping.py` 用 SHA256、原始指令字、MOVZ/MOVK 和 STR immediate 地址算术复核上述地址，exit 0；没有运行厂商二进制或调用固件。原镜像、反汇编和 helper 保留在忽略目录。

**本证据不能证明：** 当前 CON17 值、当前启动时已调用 selector 3、该 handler 在所有调用上下文可用、SMC 动态成功、M0 经过 reset 后的实际映射。当前寄存器取值与 Linux PA 仍 UNRESOLVED。配置分支不是新的只读 getter，禁止拿它作读取实验。

## 可继续的路径，按优先级

1. **离线追清启动配置。** 核当前 U-Boot/BL31 的 MCU 配置调用、参数、顺序和返回值检查，查是否确实配置 CON17。找到常量或函数名不够；需要从入口到写入的可达调用链，并排除后续覆盖。此路径现在可继续，不接触板端。
2. **为未来受控启动建立确定映射。** 若完整审查证明官方 SiP 配置可用，可在将来的启动流程明确配置 CON16/17，并在有效 MCU reset/release 前完成。成功的配置/取证日志与调用时序可形成未来映射的确定证据；这不等于取得今天 Linux-only 状态的旧值。当前公开 U-Boot `fit_standalone_release()` 只发 CODE 配置，然后开 clock/release；它不足以证明 CON17。需要 Host changeset、cache/资源审查、签名和恢复条件，以及单独部署审批。**本次没有修改该流程，也没有选新地址。**
3. **厂商提供支持的只读证据。** 向野火/Rockchip 索取与该 BL31 版本匹配的合法只读接口说明、MCU 地址配置文档，或受支持的安全固件诊断日志。没有这些依据时不试其它 SMC selector、不关闭安全防火墙。若需新增 EL3 只读诊断，需可信源码、构建/签名流程、恢复准备和另行审批。

现在最有价值的资料：野火 SDK 的 manifest/版本清单、U-Boot 实际 `.config`、`rkbin` 使用的 RK3576 BL31 文件/版本、MCU/AMP 配置说明，以及能取得的匹配 ATF/BL31 源码。先看网盘目录清单即可，不必先搬整个 SDK。

## 状态

正常 Linux 启动恢复已由用户确认；完整离线恢复链仍未闭合。新线索提高了“由安全固件确定配置”的可调查性，未解除 CON17、coherency、boot/FIT、最终布局等 Gate。**不执行板端配置，不升级 D。**
