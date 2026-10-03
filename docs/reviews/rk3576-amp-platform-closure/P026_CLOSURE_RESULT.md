# P026：主控审核结果

> **后续P028实测修正：P026 U-Boot已在用户板上运行，但原 Linux-only 启动 FAILED；旧候选撤回。以下是当时的 Host审查范围，不再表示可用于部署。见 [P028失败证据](P028_LINUX_ONLY_FAILURE.md)。**

2026-10-03。**C. HOST_BUILD_PASS；D 准入 BLOCKED，部署未授权。** 一个子代理完成 Host loader 实现，主控独立阅读源码、复跑实际 C 测试、审查其返回并整合文件身份。本轮做了获批只读取证，没有改 boot/GPT、写 OTP/MMIO、启动 M0、加载 KO 或重启。

## 已关闭的缺口

| 项目 | 本轮结果 | 证据等级 |
| --- | --- | --- |
| 验签策略取证 | Linux 经固定厂商 OP-TEE command5 成功取得4B零值；同 ABI 的 proper U-Boot 解释为 required=0 是源码推断。仍保留运行时策略，失败按 required=1处理 | BOARD_OBSERVED_READONLY / SOURCE_INFERRED |
| 原 U-Boot 身份及备份 | 完整8MiB复制、6个FIT payload SHA检查；前4MiB等于已核官方恢复payload，后4MiB全零 | BOARD_OBSERVED_READONLY / HOST_TESTED |
| 无 amp 分区方案 | 项目新增 `amp_m0load <file> <linux_fdt_addr>`，读取现有名为boot的文件系统；上电不自动启动M0，不改GPT/rootfs。原厂没有此命令，必须未来单独批准更换派生U-Boot | SOURCE_VERIFIED / HOST_TESTED |
| 复制前完整保护 | payload首次写入前保留code512KiB、shared128KiB；拒绝U-Boot framework/heap/stack及后续Image/initrd/FDT覆盖；启动前和最终Linux prep校验DT no-map | SOURCE_VERIFIED / HOST_TESTED |
| UART5所有权 | GPIO3_D4/D5实时MUX/GPIO均UNCLAIMED，用户确认3.3V TTL和16/18脚无外设；Debug与M0 UART分开 | BOARD_OBSERVED_READONLY / USER_REPORT |
| 内核/module身份 | 原CONFIG_MODVERSIONS=n、全零CRC，仅相同vermagic不证明ABI。fresh构建独立 `6.1.99-rk3576-m0echo-p026` Image、255 modules、echo KO、配套initrd；保留原目录 | HOST_TESTED |
| 恢复方案 | 当前原boot、modules、8MiB U-Boot和已核整机镜像有确切hash；用户整机恢复实测PASS。无GPT移动方案不要求搬迁用户数据；整机回刷仍会覆盖rootfs | BOARD_OBSERVED_READONLY / USER_REPORT / SOURCE_VERIFIED |

## 当前仍阻止 D 的两组事实

1. **实际映射/reset/cache**：没有CON16/17当前或等价有效映射观察，没有实际cold reset/setter返回及MCU cache bypass状态。TRM、原BL31反汇编、checked setter与Host fault stub只能证明未来初始化设计；不能充当运行成功证据。拟 B16=`0x47800000`、B17=`0x40000000`仍是 `host_proposal`。保持全局bypass/双侧uncached的Host方案，实际coherency未通过。
2. **实际加载能力**：当前原U-Boot未证明有AMP功能，尚未运行新派生U-Boot/显式文件入口。其源码、完整Host编译、原ATF/OP-TEE保留包装已通过，但不是板端调用能力观察。厂商TA这次flag=0不证明proper U-Boot已经调用它或MCU SMC已成功。

因此没有把合同final字段填成“实际值”。`check_deployment_packet.py`：Host文件完整性PASS，`board_test_readiness=BLOCKED`、`deployment_gate=BLOCKED`。最终部署许可与D准备等级单独判断：将来达到D也仍须请求部署审批，缺部署批准本身不把D改成C。

## 地址与 RPMsg 的准确范围

在**未来显式设置 B16/B17 并经cold reset生效**这一前提下，源码公式及Host脚本一致：RTOS PA=`0x47800000`，M0 local Thumb entry=`0x141`，实际指令PA=`0x47800140`；vring0/1拟PA=`0x47d00000/0x47d08000`，pool=`0x47d10000`。code `[0x47800000,0x47880000)`与shared `[0x47d00000,0x47d20000)`不重叠。

这是明确的未来配置方案，不是新取得的运行寄存器值。CPU3参考link03/vring PA47800000不再原样并入M0方案。M0 link04、Linux RX MBOX0/ch0/SPI125、TX MBOX4/ch0/SPI129；M0 TX MBOX0/ch0、RX MBOX4/ch0/NVIC175；NS=`rk3576-m0-echo`。双侧源码/Host参数一致，尚无真实link。

## 下一阶段的边界

授权到中午12:00的是本輪取证；已审查工具临时在可执行tmpfs `/run`运行并清理，没有把它扩大成boot改写/启动许可。不会再次直接 `devmem/md` 读SGRF；两条普通世界路线已经异常，现有安全读服务也拒绝目标地址，重复不能提供新证据。

剩余门需要安全固件提供可审查的映射/reset/cache诊断证据，或用户明确批准一轮单独的受控启动链/冷初始化验证。后者会涉及寄存器配置或候选bootloader运行，超出本轮“不启动、不改启动链”范围，不能把它包装成普通只读动作。若维持原准入门且没有新合法诊断接口，D保持BLOCKED；不建议把完整echo部署当作取证捷径。

完整对象见 [P026_ARTIFACT_MANIFEST.json](P026_ARTIFACT_MANIFEST.json)、[P026_DEPLOYMENT_CHANGESET.json](P026_DEPLOYMENT_CHANGESET.json)；测试见 [P026_HOST_VALIDATION.md](P026_HOST_VALIDATION.md)，恢复见 [P026_DEPLOYMENT_AND_ROLLBACK.md](P026_DEPLOYMENT_AND_ROLLBACK.md)。旧P025及旧架构审查保持历史原文。

收尾验收：`bash scripts/dev/host_ci.sh` exit0，2 CTest +40 Python及shell语法检查PASS；`git diff --check` PASS，fixed reference status空。严格packet准入返回2是预期BLOCKED，不是Host build失败。具体Git交付以本轮commit/PR为准，保持Draft未merge。
