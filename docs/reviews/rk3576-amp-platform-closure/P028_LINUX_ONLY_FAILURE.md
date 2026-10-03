# P028：原 Linux-only 启动失败与恢复证据

2026-10-03。**C. HOST_BUILD_PASS / D BLOCKED / P026_CANDIDATE_WITHDRAWN**。

## 用户板端证据

`BOARD_OBSERVED_USER_REPORT`：SPL仍为 `8f53f800da`，proper U-Boot为 `2314a3f`，uboot SHA前缀 `6ea779087d` 与 Host P026 payload 一致；ATF/OP-TEE仍是原版本/hash前缀。出现 `Cmd interface: disabled`，autoboot后 `FIT: No FIT image`、`No CLI available`，不能称 Linux验证通过。没有 M0启动证据。用户随后截图显示一个 MASKROM设备。

这是候选启动兼容性缺陷，不应归因于用户选择了错误文件；当前没有新的板端分区读回，不能声称整分区hash已观测。

## 三项源码根因

派生源码 `artifacts/local/p026-uboot-preload` 固定SHA `2314a3f9f5795b88c7c53a805e9d59d82c6715b3`。主控与独立子代理交叉复核：

| 根因 | 源码 / 实际构建配置 | 结果 | 证据 |
| --- | --- | --- | --- |
| CLI 被关 | Kconfig `FIT_SIGNATURE` select `CONSOLE_DISABLE_CLI`；`.config`为y；`common/cli.c`编成空cli_loop | autoboot失败无救援CLI | SOURCE_VERIFIED |
| 默认启动路径改变 | `include/configs/rockchip-common.h`在FIT_SIGNATURE分支选择boot_fit；`evb_rk3576.h`覆盖CONFIG_BOOTCOMMAND | 原文件系统boot.scr路径不再被默认执行 | SOURCE_VERIFIED |
| 原legacy script不支持 | `include/config_fallbacks.h`在签名配置下不定义CONFIG_IMAGE_FORMAT_LEGACY；`cmd/source.c`排除legacy branch | 即使修CLI/default命令，仍不能解析原boot.scr | SOURCE_VERIFIED |

原板路径是 `distro → /boot.scr → uEnv → raw Image/booti`。原板未使用kernel FIT；三个问题均须闭合。不得为了兼容而关闭AMP签名检查，也不能把保持FIT_SIGNATURE说成原raw Linux已签名。required=1或TA读取失败须fail closed。

## 撤回和 Host 恢复物料

旧package SHA `4b6e615beba307d95beff1cbbff76b06bfe794f9a2f269599d2ccd5026ca4f1f`，仅保留取证；P027脚本无条件在任何文件/设备访问前退出2。fixed reference保持原样，新源码修复必须用独立派生worktree。

Windows已核原件目录：`C:\Users\27432\Desktop\RK3576-Recovery-Tools\P026-Originals-20261003`。

| 文件 | 大小B | SHA256 | 用途 |
| --- | ---: | --- | --- |
| factory-MiniLoaderAll.bin | 770553 | 4c0d04c6234b6e783aeedf317f4bb661700d9ef2ef97f876e67597807aa86206 | 原官方恢复包提取，LoaderToDDR只下载到RAM |
| uboot-original-8MiB.raw | 8388608 | ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a | 原板完整uboot分区，仅在目标核实后由用户恢复 |
| factory-parameter.txt | 399 | e93b841868d8c07991b4ac47875ce77a5692807ccf247ceb5d5e062303d8e193 | 仅导入Host工具配置核分区；不勾选刷写 |

独立工具目录 `C:\Users\27432\Desktop\RK3576-Recovery-Tools\RKDevTool_P028_Uboot_RestoreOnly`，仅将下载后自动reset关闭；没有启动工具或执行设备操作。普通Loader会写IDBlock，必须选LoaderToDDR。随包手册§1.2/1.3明确此区别，文件SHA `be6d064c5f70a7edc28a9f47a05db97aa7c3e3d9cd99cd46426b364bad7e919a`，`SOURCE_VERIFIED`。

原板分区历史只读证据：uboot起始/长度均0x4000个512B扇区（8MiB起点、8MiB长度）；boot起始0x8000，rootfs起始0x48000。**闭源RKDevTool强制地址字段单位未闭合，不直接手填猜测。** 先用户RAM Loader日志，再设备分区表/原parameter，确认EMMC和uboot边界，再仅写原raw。Parameter/GPT/boot/rootfs/普通Loader不属于本次部分恢复。

## 当前待办

Host撤回验收：生产writer5项无外部访问测试PASS；全Host CI exit0（2 CTest+41 Python+5 withdrawal测试）；git diff无空白错误。桌面旧镜像改为`uboot-P026-WITHDRAWN-DO_NOT_FLASH.bin`，hash保持原4b6e；桌面生产脚本新版SHA `6ba0c4bbe9f5f93f27fdb801d92f323c948dbcc3115c3ea5ee80eb592fd3c18d`。canonical manifest显式WITHDRAWN，P026历史manifest未改；packet hash PASS与部署BLOCKED可以同时成立。

工具副本未识别MASKROM，用户明确还没点执行。Host发现INI编码原版UTF-16LE（FF FE），副本误为UTF-8；副本修回UTF-16LE，原字节另存`.p028-before-encoding-fix`。Win32 `GetPrivateProfileStringW`实际读取Kinds=2、Selected=1、RESET_AFTER_DOWNLOAD=FALSE，`HOST_TESTED`；修后INI SHA `e1dd3afe8adaa0a1de9bfefc0bdd72608fcdffb0e9a7c3d8072efc5c0e9b827f`。这是一个已修复的Host配置缺陷，是否解释设备识别仍为SOURCE_INFERRED，待用户重开观察。不修改原工具目录、不自动启动/关闭GUI、不执行USB设备命令。

- RAM Loader下载结果、实际GPT/存储截图、原uboot写回/读回及原Linux冷启动均未观测。
- 子代理正在独立源码修复；需真实原boot.scr parser/default env与required policy回归，主控审核后才可准备新候选。本报告不提供新候选部署许可。
- 整机官方恢复曾用户报告PASS，但会覆盖rootfs/用户数据；不作为部分恢复失败后的自动动作。
- 原AMP有效映射/cache/MCU SMC仍未观测；不继续M0测试。

Agent本轮只Host操作和分步指导；板端恢复由用户完成。原始串口文本不提交仓库。
