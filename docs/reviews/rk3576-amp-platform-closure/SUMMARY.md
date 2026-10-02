# RK3576 AMP platform closure — 2026-10-01

## 最新补充：P024（2026-10-03）

**仍为 C. HOST_BUILD_PASS。** 官方Debian12 GNOME 20260424完整恢复镜像已校验，SHA256=`18247661a898821a751262d57f18f6edf6aab0f62b4b706cfc1b74a4ea277a66`，发布MD5与内部content MD5一致。恢复包U-Boot/BL31/Kernel/DTB/boot脚本与前轮板端原件逐字节一致；当前active uEnv仅相比factory v2模板启用CAM0。Windows桌面已准备RKDevTool3.32，微软签名Rockusb5.14预装成功(oem71.inf/exit0)，原boot副本已另存桌面。

完整恢复介质/物料缺口关闭；包内仍只有uboot/boot/rootfs，无amp。此次没有取得当前CON值、实际AMP加载入口/验证策略或动态SMC证据；实际USB恢复识别、用户数据恢复与最终changeset仍待闭合。没有板端访问/写入、启动M0或修改Host AMP artifact/contract。

详见 [RECOVERY_IMAGE_ANALYSIS.md](RECOVERY_IMAGE_ANALYSIS.md)、[RECOVERY_IMAGE_IDENTITY.json](RECOVERY_IMAGE_IDENTITY.json)、[HOST_RECOVERY_TOOLS.md](HOST_RECOVERY_TOOLS.md)、[ROLLBACK_FINAL.md](ROLLBACK_FINAL.md)。八项解析器边界测试及全Host CI（2 CTest、22 Python）PASS。以下保留P023及更早事实。

## 最新补充：P023（2026-10-02）

**仍为 C. HOST_BUILD_PASS。** 已完成冷reset + cache bypass的最小echo候选（122,696 B、0 warning）、专用Linux DMA pool fail-closed/barrier patch、明确setter的U-Boot完整Host build、Host overlay/FIT、完整候选Kernel Image/modules/v2DTB及对应echo。运行DT普通只读副本和原boot文件已复制到Host；硬件PDF确认UART5 16/18脚，DT无启用引脚冲突，实时pinmux因权限不足未确认。

唯一参数源已增加`host_proposal`，只描述拟用SiP CODE=`0x47800000`、shared base=`0x40000000`之后的布局：ringsPA=`0x47d00000/0x47d08000`、poolPA=`0x47d10000`。没有获取或假定当前CON值，没有部署、启动M0或改boot。9,672项Host合同检查、26+1项冷启动fault-injection及Host CI通过。

当前板的AMP boot入口/GPT来源、动态SiP可用性、签名策略、精确运行Image源码身份和完整离线恢复介质仍未闭合；不给D。最新交接：[HOST_PREBOARD_PACKAGE.md](HOST_PREBOARD_PACKAGE.md)、[HOST_PACKAGE_MANIFEST.json](HOST_PACKAGE_MANIFEST.json)、[RECOVERY_PACKAGE_EVIDENCE.md](RECOVERY_PACKAGE_EVIDENCE.md)。以下保留上一轮历史结果，旧135,256 B/cache-on固件及旧FIT/manifest不可与本轮候选混用。

**结论：C. HOST_BUILD_PASS；D 未达到，禁止上板。** 本轮只做 Host 与板端只读盘点。固定 RTOS/HAL reference 未改；派生 worktree 位于 `worktrees/rk3576-amp-platform/rtos`，项目分支 `agent/amp-platform-closure` 基于 `c2b2a83`。之前的 BSP 身份、M0 架构、callback 与 I2C 配置结论保持有效。

| Gate | 结果 | 证据等级 |
| --- | --- | --- |
| 0x47800000 | 是实际 FIT payload 物理装载地址；CPU3 参考 DTS 又用作 vring0，二者原样合用确实冲突。参考 DTS 注释明确 CPU3 link3、MCU link4，故它不适用于 M0 原样部署。当前板无 AMP DT，不存在已发生的运行时冲突。 | SOURCE_VERIFIED / BOARD_OBSERVED_READONLY |
| CON16/CON17 | TRM 映射公式、寄存器位置有据；当前板值、CON17 写入者和时序未证。 | SOURCE_VERIFIED；板值 UNVERIFIED |
| RPMsg | 候选 M0 link4、MBOX0 TX/MBOX4 RX、NS service、两 ring 参数源码可追；Linux 仅 CPU3 link3/MBOX3 参考，当前板无最终 M0 DT。 | SOURCE_VERIFIED；端到端 BLOCKED |
| Cache | M0 侧 RPMsg cache hook 为空，U-Boot 源含 uncache window CON14/15 写入，但当前板执行及实际 shared DDR 属性未证。 | UNVERIFIED |
| Boot | 匹配版本 U-Boot 源的 `AMP_PART` 是 GPT 分区 `amp`；当前 eMMC 无该分区，实际 U-Boot `CONFIG_AMP`、BL31 MCU SMC、FIT 验签未知。 | SOURCE_VERIFIED / BOARD_OBSERVED_READONLY / BLOCKED |
| Host | 派生 M0 echo clean SCons + unsigned FIT 结构生成；Linux echo 模块对板上实际 headers/Module.symvers 构建通过，但 exact kernel source commit 未证。均是 **非部署候选产物**。 | HOST_TESTED |
| DTS / UART / recovery | 未选最终内存地址，未生成可部署 LubanCat AMP DTB；UART5 当前 DT disabled 但 v2 物理/互斥未闭合；无完整离线恢复链。 | BLOCKED |

唯一参数源 `docs/amp/AMP_PLATFORM_CONTRACT.yaml` 用 JSON 语法的 YAML 1.2；未知最终值为 `null`，检查器 `scripts/amp/check_platform_contract.py` 返回非零。报告标题中的 “FINAL” 表示最终 Gate 裁决，不表示已有可部署最终地址。

下一步需取得 **批准的只读寄存器/bootloader 证据**、当前 U-Boot/BL31 镜像与配置、CON17 和缓存属性，再依证据设计 M0 专用 reserved-memory/ITS/DTS 并完成离线恢复。当前不得把 Host FIT、KO、参考 DTS 或本报告用于部署。`BOARD_TEST_APPROVAL_PLAN.md` 只定义未来审批输入。
