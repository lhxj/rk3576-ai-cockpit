> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# P028：恢复通过与新候选 Host 审核

2026-10-03。整体 **C. HOST_BUILD_PASS / D BLOCKED**。

## 已恢复的原 Linux

`BOARD_OBSERVED_USER_REPORT + HOST_TESTED`：用户仅恢复原uboot，8MiB读回SHA为 `ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`，与原备份相同；用户冷启动串口确认proper U-Boot `8f53f800da`、原 `/boot.scr`、`6.1.99-rk3576 #8` 和 Debian 12登录。详见 [恢复身份](P028_RECOVERY_READBACK.json)。覆盖原Linux启动与恢复路径，不覆盖全部外设功能。

## 源码修复及审核

`SOURCE_VERIFIED`：固定LubanCat U-Boot `8f53f800da` → 旧P026 `2314a3f` → 独立派生 `f8b4554584dd475ce783c605850c5e883b0a0fd4`。统一diff补丁 `0009` 需在完成 `0008` 的源码上用 `git apply`（不是format-patch邮件），主控对旧P026执行 `git apply --check` PASS。固定RTOS/HAL reference及旧P026源码/产物不改。

- 保留 `FIT_SIGNATURE=y`、`CONSOLE_DISABLE_CLI=y`。仅一次内部策略读取明确返回0才允许factory distro/CLI/CTRL+C/preboot/source/raw booti；环境变量不能授权。非零或OP-TEE错误继续关闭raw路径，保留厂商signed FIT路径。
- proper U-Boot强符号getter调用原OP-TEE只读函数，错误返回1。Host不能替代新proper U-Boot实际调用；当前板曾取得flag0不保证新镜像运行调用必成功。
- 默认bootcmd为 `amp_project_boot`，policy0执行 `run distro_bootcmd`，BOOTDELAY=3。原厂boot.scr实际为PPC/Linux文本SCRIPT（mkimage默认tag）；专用兼容检查双CRC、单组件、未压缩、精确load大小、DRAM/AMP区域及8B对齐，最大64KiB；普通legacy kernel仍禁用。
- 源码 `amp_cpus_on()` 的显式file-only分支返回0；上电不自动启动AMP。`amp_m0load` 是启动命令，本轮及下一次Linux-only测试均禁止调用。

## 主控 Host 验收

`HOST_TESTED`：从全新 `artifacts/local/p028-primary-clean-v1` 完整构建exit0；aarch64-linux-gnu-gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0。

| 检查 | 结果 | 边界 |
| --- | --- | --- |
| 实际源码main/autoboot/CLI/booti/source/CRC及ELF默认env | 181项PASS | TA、命令执行及硬件为stubs；未执行完整目标HUSH/hardware命令 |
| 原factory boot.scr/boot.cmd/control DT | 实际原文件解析PASS | 没有目标硬件运行 |
| AMP FIT policy | 两种signature配置各23项+3顺序PASS | SMC/firmware验证返回注入 |
| 复制前预留与Image/initrd/DT保护 | 20+113+16项+9顺序PASS | 继承P026 Host保护；不是M0运行证据 |
| 标准Host CI | 2 CTest+41 Python+5撤回writer PASS | 无板端操作 |
| build warning | 新target源码0；vendor Host工具23 | 原OpenSSL3/BMP工具warning |
| canonical packet | hash PASS / readiness BLOCKED | P026保持WITHDRAWN |

## 封装身份

原BL31三payload、OP-TEE、control DT分别与实际原板8MiB备份逐字节一致，仅proper U-Boot代码替换。4MiB新FIT为两个相同2MiB slot，再接原板备份尾4MiB，得到完整8MiB候选；原尾4MiB实际全零，仍按原字节保留。完整8MiB适配已观测uboot LBA0x4000/数量0x4000/512B扇区。原boot/rootfs/GPT、DDR/IDBlock不属于待测变更。

**候选（尚未批准、尚未部署）**：`C:\Users\27432\Desktop\RK3576-AMP-P028-LinuxOnly-PendingApproval\uboot-P028-linux-only-8MiB-PENDING-APPROVAL.img`

大小8388608；SHA256 `9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e`。新proper U-Boot payload SHA256 `2e1b965bfb4209afeb8e2570899aeb32faa70e93d0e3a6e3e09490a4c45cb493`；预期SPL检查前缀 `2e1b965bfb`、proper U-Boot提交 `f8b4554`。

完整文件/配置/工具/测试hash在 [P028_CANDIDATE_MANIFEST.json](P028_CANDIDATE_MANIFEST.json)。新的8MiB候选只用于另行批准的Linux-only兼容测试，不是AMP D部署物料。旧P027脚本仍无条件停止，旧P026镜像不可再用。

## 准入及风险

`UNVERIFIED`：新proper U-Boot实际policy0调用、factory Linux冷启动；M0有效映射/reset/cache、BL31 MCU setter成功。当前AMP grade=C。Host策略失败关闭raw/CLI是有意行为：新镜像若TA失败，当前raw-Image板可能再次停在FIT/无CLI，需要已通过的MASKROM原uboot恢复。不得把安全拒绝改成默认许可或关闭验签。

下一次建议仅验证新proper U-Boot → 原Linux正常登录，不修改boot/GPT、不开AMP/RPMsg、不启动M0。AGENTS.md L3要求每次U-Boot写入/重启明确批准；中午12点前诊断授权已结束。本轮未执行任何SSH、上传板端、USB刷写、寄存器访问或重启。

## 后续Linux-only实板验收

用户在有限批准内完成新P028写入、完整8MiB读回和正常上电。最新串口证实 `gf8b4554`、proper运行时policy=0、原boot.scr/booti与Debian12登录，Linux-only=PASS。上文Host-stage的“尚未批准/未部署/策略UNVERIFIED”保留为当时事实；当前验收见 [P028_LINUX_ONLY_BOARD_RESULT.md](P028_LINUX_ONLY_BOARD_RESULT.md)。实际MCU setter、M0有效mapping/cache及RPMsg仍未验证，AMP仍C/D关闭。

后续CLI补充：用户Ctrl+C进入=>，help amp_m0load正常显示项目命令及用法，boot仍到原Debian；uname=原6.1.99-rk3576 #8，cmdline含uboot-f8b4554。记录CLI/命令注册/原Linux身份PASS；仅help，不是amp_m0load加载成功或M0启动证据。补充原始日志只在忽略目录保留。
