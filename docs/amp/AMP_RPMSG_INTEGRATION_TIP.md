# AMP_RPMSG_INTEGRATION_TIP

2026-10-04。**READY_TO_INTEGRATE_BOARD_TESTED_MINIMAL_CHAIN**。交接对象是已经在 LubanCat-3 v2 / RK3576 BUS M0 上真实完成 `HELLO → HELLO_ACK → PING → PONG` 并完整冷恢复默认 Debian 的最小链。

当前分支：`agent/amp-platform-closure`。最终 tip 是包含本次整理的实际 Git 提交 SHA；Windows `AMP_RPMSG_INTEGRATION_TIP.txt` 和本轮最终回复给出精确 SHA。基线为 bootstrap `3b07df7ba05a535ba18dbc7d692ef961b9b748e5`。对应 [Draft PR #5](https://github.com/lhxj/rk3576-ai-cockpit/pull/5)，本轮只提交、推送，未合并。

## 1. 冻结范围

| 范围 | 当前事实 |
| --- | --- |
| RTOS | RT-Thread 4.1.1 / BUS Cortex-M0 / 现有最小 echo，保留 v5 诊断与条件 SysTick reload 修正 |
| Linux | 独立配套 `6.1.99-rk3576-m0echo-p026` Image、modules、initrd、echo KO |
| 传输 | RPMsg-Lite remote ↔ Rockchip mailbox/virtio master，link4，NS service `rk3576-m0-echo` |
| Boot | 冷启动 U-Boot `amp_m0load` 显式 boot 文件入口，先签名 FIT/M0，再用已准备 C DT 启动 Linux；无新 amp 分区 |
| 收发 | Linux 内核测试模块发送 HELLO、收到 HELLO_ACK 后发送 PING，再收到 PONG；UART只保存运行证据 |
| 恢复 | 用户完整断电再上电，恢复默认 `6.1.99-rk3576 #8`、root p3、无 stage |
| 本轮变更 | 源码补丁归档、DTS/FIT/C 命令输入索引、证据/状态整理；实测二进制保持 |

用户已要求冻结：不开发新的 RTOS 业务，不加入 sensor/control/业务心跳，不合 UI/Voice/Media。本 tip 不包含这些分支的提交或代码增量。既有 C DT 叠加原厂 CAM0 overlay 是已测 Linux 启动兼容输入，本轮没有媒体代码或功能修改。

## 2. 集成代码与构建来源

完整机器清单：[AMP_RPMSG_INTEGRATION_TIP.json](AMP_RPMSG_INTEGRATION_TIP.json)。补丁顺序与 SHA：[series.json](../../patches/rk3576-amp-integration/series.json)。SDK 保留在仓库外，主仓保存最小改动、项目源码和输入。

| 组件 | 固定源码与当前入口 |
| --- | --- |
| U-Boot | vendor `8f53f800da2c25d0c6ba414fb45902a01675703a` → 派生 `149b1c53e368a0d77e542cfdf3bed6db3374682a`；[完整累计源补丁](../../patches/rk3576-amp-integration/uboot-8f53f800-to-149b1c5.patch) |
| RTOS 基线 | 本地 SDK import `8541f7ad0c584469cc6dd725b6e489d1e9d29e4a` → 派生 `3a39b0f4f5eb25631d05b4ffd88746ce9b1b0cdb`；[累计源补丁](../../patches/rk3576-amp-integration/rtos-import-to-3a39b0f-source.patch)，含 UART5 M0 pinmux、echo、linker/cache/地址边界 |
| RTOS v5 | 基线上按顺序叠加旧目录的 `0010-m0`、`0011`、`0012`、`0013`、`0014`、`0015`；[构建/签名封装入口](../../scripts/amp/p030_prepare_tickdiag_v3.py) 必须显式选择 revision5 |
| HAL | SDK import `277de3fd4b0e640654ee73bb3308be2ef01e3aad` → 派生 `bc99978c1a030ad79610e89a5780dcd0ee3bb1f2`；[cache bypass 源补丁](../../patches/rk3576-amp-integration/hal-277de3f-to-bc99978.patch) |
| Linux transport | `LubanCat/kernel` commit `521833e2d28decbd6473d5717f1f96cc4108e208` + [0003 pool fail-closed/barrier patch](../../patches/rk3576-amp-platform/0003-linux-m0-uncached-pool-fail-closed.patch)；[独立 kernel/modules 构建](../../scripts/amp/p026_build_isolated_kernel.sh) |
| Linux echo | [rk3576_amp_echo_test.c](../../scripts/board/amp_echo_linux/rk3576_amp_echo_test.c)、Makefile；与同一独立 kernel 构建的 KO，实测 vermagic `6.1.99-rk3576-m0echo-p026 SMP mod_unload aarch64` |
| DTS/FIT/C | [冻结输入目录](../../platform/rk3576/amp-minimal/README.md)：transport overlay、实际 v5 signing ITS 输入、实际 C 命令正文；最终 DTB/FIT/SCRIPT hash 见第3节 |
| C 只读检查 | [preflight](../../scripts/board/p030_rpmsg_c_preflight.py)，实际运行 PASS；[新 C 包生成](../../scripts/amp/p030_prepare_rpmsg_c_v1.py) 复用已经实测 v5 FIT |

新的累计补丁与旧按提交拆分补丁代表同一修改，重建时选择一种路线，不双重应用。两个 `0010` 分别属于 U-Boot 与 M0，不能按文件编号跨组件盲排。旧 Linux `0002` 草案已经包含在完整 `0003` 中。RTOS export 排除生成的 BIN 删除内容，HAL symlink 要显式指向固定派生 HAL；生成配置由已记录 SCons/defconfig 流程处理。

构建脚本依赖此前已登记的固定 SDK、交叉工具链、配置及本地原件；它们是实际构建来源记录，不能当作全新机器一键安装程序。此次没有重建或重新签名。源码恢复后新构建含时间戳/签名封装，不能自动宣称与实测字节相同，必须以本清单 hash 对照。

## 3. 实测配套产物身份

产物只在忽略的 Host 目录及 Windows 私有交付目录，不提交 firmware blob 或私钥。详细本地来源和大小在 JSON 中。

| 产物 | SHA256 |
| --- | --- |
| 完整 U-Boot 8MiB | f9beef07f807123a48f5115458dedb7bdf47337306df1a981190abf65372c0b3 |
| v5 signed M0 FIT / 131072B | 348109ebecda0f00314d4dbcaaae0f8a51e2d88ac714c03ab74c1966a3b19bd6 |
| M0 payload / 125704B | 28691e30790f1585782f3528dbd611c1e8005f8790947462574d73b8a75b6124 |
| C SCRIPT / 3100B / filesize0xc1c | 9aacb3e6882062b705b0636ceb55ed1e656098a5b33010c43d2a006a1aabd3a9 |
| C DTB / 308309B | dd68818b7fd27e9abc56522d7c4782adb0b1dd49d274fe1daa75a32ad3366fbf |
| 配套 Image | 8c82342d740c9aefe0a4f0be8d84c436d14ebf7edb5f462e14631ce355cf3e8d |
| 配套 initrd | c000c9ab2126b0a2e64581514077a7ca79aa80660989ebfbcb3a200366a1346d |
| 配套 echo KO | cd82f24659ba4a5a09fc314f5d9d3372b28451c8b21c7f64a868b11d11b5fe43 |

当前板入口 `/boot/amp-p029/rpmsg-c-v1`；C 使用 `/boot/amp-p029/Image`、initrd、stage-C.dtb 和配套 KO。默认入口保留，不能从仓库旧包名任选一个替代。现有 required-conf 公钥 control DT SHA `43164981efd8869432e42fab2411d99941d17476700f3fc03ab76948621049ce`；开发私钥不在 Git/交付索引中。外层 policy0/SPL hash 不声称 ROM secure boot；AMP FIT 实际 RSA/hash 成功有本次日志。

## 4. 真实板端证据与结论

权威执行记录：[Markdown](../reviews/rk3576-amp-platform-closure/P030_RPMSG_C_V1_EXECUTION.md)、[JSON](../reviews/rk3576-amp-platform-closure/P030_RPMSG_C_V1_EXECUTION.json)。C 前被动暂存、原件保留及独立读回见 [准备记录](../reviews/rk3576-amp-platform-closure/P030_RPMSG_C_V1_PREPARATION.md)。

| 观察 | 证据 |
| --- | --- |
| 冷 C 启动 | COM5 soc cold boot，proper149b1c5/policy0，3100B/0xc1c，单次source、签名/hash、loader成功，paired Linux/root p3/stage C |
| 实际 Linux backing | ring0=0x47d00000、ring1=0x47d08000；preflight 从 dmesg 核实际 DMA base=0x47d10000，池长0x10000来自固定源码 |
| 实际 M0 接收 | shared_va=0x27d18010 / len5、0x27d18210 / len4，位于预定池内；pa_proposal 是地址公式换算 |
| 双向收发 | Linux `HELLO_ACK`、`PONG`；M0 `PONG sent`、echo task complete，与两端实际源码匹配 |
| cache/tick | entry/link/after-pong ctrl0x6cc/bypass1；LOAD326后ISR/tick各+54、rate gate PASS，first_mdelay返回 |
| 恢复 | 用户确认完整断电再上电，随后默认#8/root p3/无stage；恢复后没有新SSH或SPL全文 |
| 操作偏离 | 只读preflight运行两次；第二次insmod返回File exists，保留记录，不构造第二次成功收发 |

COM5 源附件 SHA `0076eb99c9a2d0403a3e2332173a201d2a1ff8b9b616137f0725957775fbc6d8`；COM6 SHA `cb2e2aa97e5dbce2a4faa289089fa2b771541e7dd8f4169b832c0341bae30491`。原日志含登录/网络信息，保留用户附件；集成索引只引用 hash 和脱敏事实。

**裁决：最小 AMP/RPMsg 链 BOARD_TESTED，可集成；无需重跑 C。** 启动、预留内存、已访问映射子范围、cache bypass 快照和端到端收发有板端证据。原厂默认内核的逐字源码匹配仍未知，当前实测用独立 paired kernel，不再因此阻塞本链。原始 CON16/17 值、整个512MiB映射、缓存开启/长期一致性、热重连、用户态ABI和 RTOS 业务没有被本次短测证明，也不加入本 tip。

## 5. 提交整理与文档优先级

[完整提交/文件索引](AMP_RPMSG_COMMIT_INDEX.json) 收录 bootstrap 基线之后、成功证据 tip `8979e57` 为止的50个提交；本次整理提交接在其后。以下是依赖链关键节点，不是可独立 cherry-pick 的最小列表：

| 项目提交 | 内容 |
| --- | --- |
| c2b2a83 / 0dce8a4 | 最小 echo、Linux peer、平台合同与资源审查 |
| 080dda0 / 4cc11b8 / 6651410 | RTOS/HAL/transport、完整预留与文件加载、配套 kernel/modules/initrd |
| 237adc2 / eee6337 / 9eea47e | factory Linux/CLI兼容、trusted开发公钥与AMP签名、最终FDT guard |
| 6524461 / ac1ccbf / 14077a7 | 分阶段DTS/脚本、vendor filesize/root merge修正 |
| af303ed / 1fff5f8 / ce61df7 | SCRIPT长度表、M0 mailbox/checkpoints、tick preflight及v5时基修正 |
| c54a222 / 8979e57 | 当前 C 包/只读preflight、真实双向收发和冷恢复证据 |

文档顺序：本 tip 和 JSON → 当前 STATUS/任务/组件入口 → C 实际执行记录 → 历史分析/准备快照。旧 AMP 审查 Markdown 全部标注历史或指向当前 tip；旧 JSON 保留当时状态，由历史索引标明日期快照。旧 `BLOCKED/HOST_BUILD_PASS/D未通过` 不再作为此已实测最小链的集成裁决。

`AMP_PLATFORM_CONTRACT.yaml` 的当前观察字段索引本链；旧严格 full-deployment 字段仍用于其原范围的检查，不以填造 raw 寄存器值打开旧包。该历史 checker/packet 返回 BLOCKED 与当前最小链集成 READY 属于各自明确范围，不能用旧 packet 覆盖本次通信证据。

本轮离线读回并对照已登记 hash、源补丁/提交范围/文档链接；没有新测试套件、构建或板操作。Windows 交付目录：`C:\Users\27432\Desktop\RK3576-AMP-RPMsg-Integration`，含本 tip、JSON、提交与补丁清单、脱敏证据及真实 Git SHA。
