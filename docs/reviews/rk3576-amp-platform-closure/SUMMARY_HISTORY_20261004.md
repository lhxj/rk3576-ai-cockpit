> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# RK3576 AMP platform closure — 2026-10-01

**P030 新版 C 双向通信及冷恢复实板通过（2026-10-04）：** 用户 COM5/COM6 确认冷启动新版3100B/0xc1c C、验签/loader和配套Linux成功，只读preflight PASS、实际rings0x47d00000/0x47d08000及DMA base0x47d10000；Linux收到HELLO_ACK和PONG，M0实际rx 0x27d18010/len5、0x27d18210/len4并PONG sent，entry/link/after-pong均bypass1，echo task完成。第二次insmod返回File exists，记录为操作偏离；首次收发仍通过。用户随后明确完整断电再上电，并贴默认6.1.99-rk3576 #8/root p3/无stage身份，C后冷恢复PASS。**最小双向通信与已访问共享子范围功能证据通过，无需重跑C；整体D/canonical final、长期或缓存开启、业务心跳错误恢复未自动验收。** 本轮Agent仅离线审查/文档，无板访问/新测试。 见[本次执行记录](P030_RPMSG_C_V1_EXECUTION.md)、[JSON](P030_RPMSG_C_V1_EXECUTION.json)。以下保留历史状态。

**P030 新版C已暂存，可开始用户冷启动双向验证（2026-10-04）：** v5 B与冷恢复已通过。新rpmsg-c-v1沿用已实测signed FIT、paired C DT/KO；SCRIPT3100B/0xc1c，正确结束项0，单次loader与全部保护保持。Host现有公钥验签/DT/KO/封装校验通过，持锁仅新增独立目录及SHA256SUMS五项/receipt独立读回PASS，factory/旧tree保持、RAM清理、仍默认Debian；Agent未source/M0/KO/MMIO/重启。新只读preflight要求C身份/no-map/iomem/service/真实rings和DMA base，再由用户一次配套KO自动HELLO/ACK/PING/PONG。**C执行条件具备；实际通信/共享映射与C后冷恢复仍UNVERIFIED，D未通过。** 见[新C指南](P030_RPMSG_C_V1_GUIDE.md)、[准备记录](P030_RPMSG_C_V1_PREPARATION.md)、[JSON](P030_RPMSG_C_V1_PREPARATION.json)。以下保留历史。

**P030 v5首次延时与B冷恢复实板通过（2026-10-04）：** UART参照baseline counts17448；本地LOAD239998→326后ISR/RT tick均+54、rate gate PASS，first_mdelay tick61→167/delta106（含串口输出），heartbeat后按B关闭transport的预期15s退出。支持约32K输入误按24MHz计算重载，当前首次延时停滞已解决。COM5 source/FIT/M0/cache entry/配套Linux通过；用户随后明确完整冷断电并补默认6.1.99-rk3576 #8/root p3/无stage串口身份，恢复通过。本轮无Agent板访问/测试套件。**可准备新版C，旧C不可执行；共享mapping/HELLO_ACK/PONG/长期稳定性/D未证明。** 见[实板结果](P030_TICKDIAG_V5_B_EXECUTION.md)、[JSON](P030_TICKDIAG_V5_B_EXECUTION.json)。以下保留历史。

**P030 v4计数变化但未跨周期，v5条件时基修正已暂存（2026-10-04）：** v4 source/FIT/M0 entry/配套Linux通过，预检wraps0/ISR0/tick0后STOP；不能判IRQ失效，原固定读次数覆盖不足。用户及只读SSH确认完整冷恢复默认Debian。SDK同SoC vehicle-evb支持32K，v5借UART5物理TX粗略辨别32K候选，条件匹配才将本地SysTick LOAD239998改326，再核ISR与RT tick增长后允许首次延时。fresh编译/现有control-key验签/封装核验/只新增目录及独立读回PASS、旧树与默认入口保持、RAM清理；Agent未source/M0/KO/SSH MMIO/重启，未跑测试套件。**v5时基/延时修复效果待用户一次冷B，完整RPMsg/C/D仍未通过。** 见[新指南](P030_TICKDIAG_V5_GUIDE.md)、[v4执行](P030_TICKDIAG_V4_B_EXECUTION.md)、[v5准备](P030_TICKDIAG_V5_PREPARATION.md)。以下保留历史。

**P030 tickdiag-v4 被动暂存完成，等待单次实板诊断（2026-10-04）：** 当前首次 mdelay 未返回，实际 board init 忽略 SysTick 配置返回值。新补丁保存配置结果、仅本地寄存器/向量/ISR/tick 有界观测，在预检失败时 STOP/return，不进 RPMsg init 或 RT delay；不盲改时钟/映射/transport。v3 暂存后交付前发现打印超过128字节，已暂停保留；v4 拆短行最大99字节。新 v4 干净编译、现有 control-key 验签、容器/脚本校验、持锁只新增目录及独立读回 PASS，原启动与旧包 metadata 保持、RAM清理，仍默认 Debian。Agent 未 source/M0/KO/MMIO/重启；未运行测试套件。具体硬件根因、v4 tick/延时和完整 B/mapping/cache/RPMsg/D 仍未验证，C 关闭。 见[新指南](P030_TICKDIAG_V4_GUIDE.md)、[执行记录](P030_TICKDIAG_V4_PREPARATION.md)、[JSON](P030_TICKDIAG_V4_PREPARATION.json)。以下保留历史。

**P030 source-fix-v2 B 已执行：source/FIT/M0 entry/配套 Linux 通过，首次延时未观察到返回（2026-10-04）。** COM5 全文确认新脚本 banner、RSA p029dev/MCU hash、loader 成功、6.1.99-rk3576-m0echo-p026/root p3/stage=B 登录；COM6 remote_init 返回非空、link probe up=0，停在 first_mdelay begin tick=0 后 msh。用户确认持续无后续输出。tick/IRQ/定时器唤醒为候选，具体根因和完整 B/mapping/cache/RPMsg/D 未验证；C 关闭。用户已明确完整冷断电再上电；串口及获准重试的持锁只读 SSH exit0 确认默认 6.1.99-rk3576 #8/root p3/无 stage，脚本 SHA 保持且无 echo KO/RPMsg devices。前一次 SSH exit124 无输出保留。无 Agent 板写/M0/KO/MMIO/重启，未生成新固件或运行测试。暂停重跑，准备 tick/IRQ/定时器诊断。 详见[本次执行记录](P030_SOURCE_FIX_V2_B_EXECUTION.md)、[机器证据](P030_SOURCE_FIX_V2_B_EXECUTION.json)。以下保留历史状态。

**P030 脚本静默根因已定位，source-fix-v2 被动暂存读回通过（2026-10-04）：** 固定 v1 脚本长度表结束项为 0xffffffff，实际 source parser 要求 0，正文分派前静默拒绝；原 CRC/长度检查遗漏此条件。只改结束项和两处 CRC，CMD/FIT/U-Boot 保持；实际 source 190 项及安装器快照 7 项 Host 检查通过。新目录 /boot/amp-p029/initdiag-source-fix-v2 原子安装及独立 SHA 读回 PASS，原启动文件与既有 P029/v1 内容/metadata 保持，RAM 清理。最新 COM5 全文是另一组“先 source 被 bounds 拒绝、再 load、随后冷启动”的次序，并确认默认 Debian 冷恢复；不与最初 load/source 摘录混同。Agent 未执行 source/M0/KO/MMIO/重启。修正版 B 尚未执行，完整 B/mapping/cache/RPMsg/D 未通过，C 关闭。 见[执行记录](P030_INITDIAG_B_EXECUTION.md)、[新操作指南](P030_INITDIAG_SOURCE_FIX_V2_GUIDE.md)、[机器记录](P030_INITDIAG_SOURCE_FIX_V2.json)。以下各段保留当时历史状态，涉及“全文尚缺”等不代表当前状态。

**此前的 P030 SignedFix-v8 B 观察（2026-10-04；历史记录）：** 用户直接冷执行一次v8 load/source，proper149b1c5/7d8fe670、公钥DT43164981、验签/hash/M0load通过；最终DT通过并启动配套6.1.99-rk3576-m0echo-p026/B到Debian登录。COM6观察M0 entry/cache0x6cc/bypass1/msh，用户确认无后续waiting/预期15s timeout，**完整B未接受，C/KO继续关闭**。主控持锁40秒一次只读exit0：当前uboot完整8MiB匹配f9beef07…72c0b3，runtimeDT三精确no-map、12内存槽、iomem code/shared reserved、transport/mailboxes禁用、无RPMsg设备/echoKO，原six和旧23/新6 hashes一致，无Agent板写/MMIO/M0/重启。当前读回证明存储身份，不倒改用户跳过的事前门；P030默认factory冷Linux仍缺。15s超时在remote_init返回后才计时，缺输出不能唯一证明mapping错误。下一步当时缺失的默认恢复随后经用户报告及只读 SSH 核验完成；COM5 冷启动全文仍待补。整体C/D关闭。见 [实际B记录](P030_B_EXECUTION.json)。以下保留历史。

**P030修复Host审核交付（2026-10-04）：** 真实最终DT链确认12槽含9个零占位，最小guard修复只跳过(0,0)，并独立reg_len检查全部reserved tuple，保留所有范围/no-map/验签门。新源码149b1c53干净构建、主控新cfg235断言/23完整流程/18负例与新ELF181factory入口及fullCI PASS；独立fdtget/ELF重提取/slot/tail/签名复核无must-fix，主控全读接受。新8MiB f9beef07…72c0b3仅proper代码变化，原公钥DT/签名AMP/BL31/TEE/尾4MiB逐字节保持；桌面5文件白名单SHA复核PASS，无私钥。**新板读回、默认冷Linux和B未验证；旧B/C撤回，整体C/D关闭。** 用户已冷恢复默认桌面，本轮一次SSH banner超时、远端未执行，无Agent板写/重启/M0/KO/MMIO。下一步按 [P030读回与默认启动指南](P030_FINAL_FDT_GUIDE.md) 由用户仅写uboot，再完整8MiB核对和默认Linux冷启动；Host通过不代替实板。见 [机器记录](P030_FINAL_FDT_REPAIR.json)。以下保留历史状态。

**P030：B已启动M0，最终Linux DT检查失败（2026-10-04）：** 用户一次3065B SignedFix-v8 B，required-conf RSA/payload校验通过，UART5记录RT-Thread、local_fn0x66ad与cache ctrl0x6cc/bypass1；booti在最终DT保护检查hang，未启动配套Linux，不能判完整B或mapping/RPMsg PASS。实际vendor arch/memory/final prep链Host重现：12槽memory reg包含9个(0,0)空槽，原guard误拒；no-map三段未被覆盖。另发现reserved reg循环长度被属性读取覆盖、第二tuple逃逸，纳入最小保护修复。旧B/C指南暂停，用户已完整冷断电回默认桌面；SSH banner核对超时、远端未执行。主控承担旧Host检查遗漏真实fixup顺序。整体C/D关闭；新proper-code-only修复先Host测试/clean build/独立审核，再完整8MiB读回和默认Linux门。见 [实际失败及根因证据](P030_FINAL_FDT_FAILURE.json)。以下保留此前历史状态。

**P029 SignedFix-v8默认Linux与新B暂存PASS（2026-10-04）：** 用户保存COM5日志确认soc cold boot、proper f8b4554、control DT哈希前缀43164981ef、policy0及默认boot.scr到原Debian；实时只读基线核默认6.1.99/root p3/boot p2、新完整8MiB及原sixhash/旧23一致。新B3065B保留v6配套资产和root/内存保护，改用固定签名FIT并增130560B长度门；39脚本fault、18 FD/原子不覆盖、80 vendor formatter、181实际SOURCE/CRC/libfdt及full Host CI PASS，主控接受独立审核。持锁一次仅新增signature-fix-v8六文件、独立读回PASS、旧23/原six/链接metadata保留，RAM精确清理。COM5/COM6均枚举OK。**目标硬件AMP验签/M0/cache/mapping/RPMsg仍未实测，整体C/D关闭，旧B/C暂停；下一步用户保存双日志后仅冷启动新B一次。** Agent未启动M0/KO/MMIO或重启，不改uboot/GPT/默认入口。见 [默认Linux记录](P029_SIGNED_DEFAULT_LINUX_RESULT.json)、[新B暂存与边界](P029_SIGNED_STAGE_B_RESULT.json)、[新B指南](P029_SIGNED_STAGE_B_GUIDE.md)。以下保留各时点历史状态。

**P029 v6阶段A实测PASS（2026-10-03）：** 用户串口cold boot、proper U-Boot f8b4554/policy0、2525B新A脚本、配套6.1.99-rk3576-m0echo-p026到Debian登录，最终root=/dev/mmcblk0p3和stage=A。主控一次有界SSH只读核实际DT三段no-map、无reusable且均不与System RAM相交；MCU/RPMsg/mbox0/4/UART5 disabled，无RPMsg设备/echo模块；独立子代理保存记录复核PASS。A没有启动M0，Agent未写板/重启/MMIO/KO。下一步B先落实release前UART5采集，用户仅一只Debug USB-TTL，现包没有已审核单适配器采集流程；M0/BL31 setter/有效mapping/cache/RPMsg及M0测试后冷回滚仍未实测。**A验收PASS，整体C. HOST_BUILD_PASS，D关闭。** 见 [阶段A实际结果](P029_STAGE_A_RESULT.md)。以下保留各阶段历史。

**P029 v5 A已进入initramfs救援终端，原Linux已恢复。** 照片短PARTUUID不存在、固定vendor DT覆盖root与SSH短/完整blkid证据闭合；v6仅增加checked env root override，Host及独立核心review PASS；仅新增root-fix-v6、读回/精确RAM清理PASS，新A2525B。A/B/C技术门保持未验收、整体C。见 [根分区修复](P029_STAGE_A_ROOT_FIX.md)。以下保留历史。

**P029阶段A脚本修复（2026-10-03）：** 首次旧A在STOP length Image退出，用户filesize=0x2930200；vendor formatter与旧bare字符串失配。已修复六处0x比较并清旧值，175 vendor C/112 fault项等Host检查及独立审查PASS。仅新增script-fix-v5目录，旧10文件/3receipt及原默认六文件保留，独立读回PASS，RAM清理，仍原Linux。新A脚本2368B，用户需按新路径冷启动；没有M0/重启，整体C、D关闭。见 [脚本修复](P029_STAGE_A_SCRIPT_FIX.md)。以下保留历史。

**P029阶段A暂存PASS（2026-10-03）：** 用户确认连接恢复并批准本任务所有操作；仍按A→B→C验收。当前板P028完整8MiB只读SHA匹配，Host stdin安装器一次执行成功，boot10/module268/3receipt读回PASS，原六内容和启动链接/路径自身metadata前后相同，RAM源包及root快照前缀均清理。首轮未压缩上传只有部分RAM包，持久目录尚不存在；一次压缩传输修复后安装，失败历史保留。原Linux仍运行，无Agent重启/M0/KO/MMIO操作。**A冷启动待用户手动执行，C. HOST_BUILD_PASS，D关闭。** 见 [实际暂存与交接](P029_STAGE_A_RESULT.md)。以下保留历史。

**P029阶段A已批准（2026-10-03）：** Host物料再次复核PASS；WSL SSH及Windows TCP到已知入口连接超时，尚未上传/写板/执行A。等待Debug串口当前IPv4/板端状态，沿用已有A批准继续身份/空间检查。M0/RPMsg保持关闭，整体C、D关闭。见 [A实际结果](P029_STAGE_A_RESULT.md)、[machine状态](P029_STAGE_A_EXECUTION.json)。

**P029 Host准备完成（2026-10-03）：** 已交付A/B/C独立DT/脚本、配套Linux/initrd/模块、有界M0诊断FIT和回滚指南；21文件桌面SHA复核PASS。fresh M0 build零warning，67脚本/包 +14实际M0 C +6snapshot故障项、实际P028 preload/libfdt及full Host CI PASS。只Host，未访问实板；A暂存/配套Linux、B/C有效mapping/cache/RPMsg均UNVERIFIED，未取得本轮部署批准。用户只有一个Debug USB-TTL，A足够，B/C UART5采集待落实。**C. HOST_BUILD_PASS，D关闭**。 见 [审核与下一步](P029_HOST_PREPARATION_REVIEW.md)。以下保留历史证据及其当时状态。

**P028新Linux启动PASS（2026-10-03）：** 完整8MiB新P028此前读回与获批SHA9bd8cc03...71c6d5e逐字节相同；用户新串口确认 proper U-Boot `f8b4554`、banner后运行时policy=0、原 `/boot.scr` 与 `booti`，随后原 `6.1.99-rk3576 #8` 到Debian12登录。新proper策略调用和原厂Linux启动兼容缺口闭合；补充日志确认Ctrl+C进入CLI、help显示amp_m0load用法、手动boot及uname/cmdline身份PASS；help没有加载/启动M0。不是AMP FIT/M0或所有外设测试。M0有效映射/reset/cache、BL31 MCU setter与配套RPMsg仍UNVERIFIED；**C. HOST_BUILD_PASS，D关闭**。Agent只Host解读与归档。见 [P028实板结果](P028_LINUX_ONLY_BOARD_RESULT.md)。以下保留此前各阶段的待审批/失败/读回历史。

## 最新裁决：P028（2026-10-03）

**15:09新P028读回PASS，待正常冷启动。** 用户改选P028文件后成功导出，Host实际8MiB逐字节与候选一致，完整SHA9bd8cc03...71c6d5e。现批准范围允许用户正常冷启动到原Debian；新Linux结果仍UNVERIFIED、M0关闭，AMP仍C。此前original-only mismatch保留，未倒改历史。见 [最新读回及历史](P028_LINUX_ONLY_EXECUTION.json)。

**本次新候选读回BLOCKED：** Host新导出8MiB逐字节为原件（SHAae0a507...461de8a / payload c084f257...），不等于获批P028（SHA9bd8cc03...71c6d5e / payload2e1b965...）。尚缺本次写入日志/所选文件证据，原因UNVERIFIED，暂不进行新候选冷启动或重复刷写。原候选身份仍核对正确，本次不能判新P028写入成功/启动失败，整体C。见 [实际读回与有限批准](P028_LINUX_ONLY_EXECUTION.json)。

**Linux-only下一步已获批准，尚未执行确认。** 用户只批准新P028 U-Boot测试原Linux，M0保持关闭。候选/原件/MiniLoader再次核hash，待用户MASKROM/RAM Loader、实际EMMC/GPT核对、写入及独立读回/冷启动证据。见 [有限批准](P028_LINUX_ONLY_EXECUTION.json)。整体C，不解锁AMP部署或D。后文“待审批”保留为批准前状态。

**新候选Host审核通过、板测待审批：** 主控独立clean build及181项实际C检查、FIT/preload/full CI PASS；原BL31/TEE/control DT与实板备份一致。只交付8MiB Linux-only待测镜像，SHA `9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e`，不自动部署、不启动M0。新proper U-Boot实际policy与原Linux兼容仍UNVERIFIED，旧P026保持撤回，canonical D门关闭。见 [Host审核](P028_HOST_REPAIR_REVIEW.md)、[新候选身份](P028_CANDIDATE_MANIFEST.json)。

**恢复已验收：ORIGINAL_UBOOT_READBACK=PASS；ORIGINAL_LINUX_COLD_BOOT=PASS。** 8MiB读回SHA与原备份相同；用户原始串口日志确认 proper U-Boot `8f53f800da`、原 `/boot.scr`、原 `6.1.99-rk3576 #8` 与 Debian 12 登录界面。证据 [P028恢复身份](P028_RECOVERY_READBACK.json)。Agent未写板或重启，旧P026保持撤回，AMP仍C。下面“恢复尚未观测”保留为失败后早期过程。

**C. HOST_BUILD_PASS；P026 Linux-only = FAILED，旧 U-Boot 候选已撤回。** 用户串口显示候选 proper U-Boot `2314a3f`，随后 `FIT: No FIT image / No CLI available`，原 Linux 未启动。主控确认 FIT_SIGNATURE 引起 CLI、default bootcmd、legacy boot.scr 三项配置兼容问题；P027生产脚本无条件拒绝所有 check/write。用户已进入 MASKROM，正在 RAM-only LoaderToDDR 前置步骤，原 U-Boot 恢复尚未观测。子代理只在独立 Host 源码修复；没有 Agent 写板/重启/M0 操作。详见 [P028失败、根因和恢复证据](P028_LINUX_ONLY_FAILURE.md)。下面 P026 的 Host PASS 是历史编译/测试范围，不是原 Linux 启动通过或可继续使用的候选许可。

## 当前裁决：P026（2026-10-03）

**C. HOST_BUILD_PASS；D准入与部署门仍关闭。** 主控已审核一个子代理并复跑最终实际C测试。获批取证得到厂商OP-TEE command5的4B验签flag=0、原Ub完整8MiB、GPIO3_D4/D5无owner；未改boot/GPT/OTP/MMIO、未启动M0、未加载KO/重启。原件及MiniLoader另存Windows桌面恢复目录并核hash。

已形成：从现有boot文件系统显式加载FIT的项目入口（不新建amp/GPT、不自动启动）、复制前完整code/shared预留及Image/initrd/DT覆盖保护、Linux DT no-map前后检查、fresh成套独立Linux release/255modules/initrd/echo、精确objects/rollback。子代理不能仅凭Host PASS升级D，主控也不把TA flag读数外推为MCU映射/cache证据。

仍缺两组实际事实：CON16/17等价有效映射与cold reset/cache状态；新Ub文件入口和原BL31 MCU setter实际可用性。当前原Ub没有项目入口，静态BL31分支不是实际成功调用。canonical final保持null；Host proposal无物理overlap但不是运行状态。见 [P026完整结论](P026_CLOSURE_RESULT.md)、[实际只读取证](P026_BOARD_READ_EVIDENCE.md)、[Host验收](P026_HOST_VALIDATION.md)、[精确部署/回滚](P026_DEPLOYMENT_AND_ROLLBACK.md)、[当前manifest](P026_ARTIFACT_MANIFEST.json)。以下P025及更早内容保留其历史语境，旧“验签flag/原Ub备份/分区加载设计缺失”不再作为当前Host blocker。

## 此前裁决：P025（2026-10-03）

串口日志后续补充：已观测该次SPL `Verified-boot:0`及六个payload SHA检查PASS，SPL FIT未要求签名。proper U-Boot AMP阶段另经OP-TEE查flag；日志未走此路径，实际结果/key仍UNVERIFIED。日志已取得，无需重复提供或重新上电。见 [原始摘录与边界](../rk3576-amp-board-evidence/SPL_VERIFIED_BOOT_SERIAL_EVIDENCE.md)。

**C. HOST_BUILD_PASS；APPROVAL_GATE_BOARD_TEST关闭。** 本轮恢复后普通只读SSH成功：相同Kernel/U-Boot/BL31版本，Image/uEnv/DTB/boot脚本六项hash与原备份一致，重新取得保留软链接的boot原件；仍只有uboot/boot/rootfs，无amp或AMP/RPMsg DT。整机恢复实测用户确认PASS保持有效。

Host地址/cache方案冻结：未来checked SiP/cold reset设置B16=`0x47800000`、B17=`0x40000000`，ringsPA=`0x47d00000/0x47d08000`、poolPA=`0x47d10000`；全MCU cache bypass+Linux uncached pool/device barriers。实际CON/运行cache/SMC仍未证明，final合同保持null。

新增AMP FIT config-policy/required-conf-key/partition边界补丁；signature-enabled U-Boot fresh完整Host build和原BL31/OPTEE/DT保留封装PASS，**unsigned/nondeployable**。46项实际C测试+3顺序检查、26+1冷启动fault检查、9,672合同检查PASS。部署机器清单的文件身份PASS、准入检查Linux exit2/BLOCKED，含exact旧/新hash和rollback。完整结论：[P025_CLOSURE_RESULT.md](P025_CLOSURE_RESULT.md)、[加载与验签](P025_BOOT_LOAD_AND_VERIFY.md)、[部署/回滚](P025_DEPLOYMENT_AND_ROLLBACK.md)、[P025 manifest](P025_ARTIFACT_MANIFEST.json)。

剩余：真实验签policy/key、官方amp GPT加载目标和用户数据备份、复制前完整code/shared内存保护、运行SiP/mapping/cache、kernel/module升级身份和UART5实时/接线。下文均为历史，不用“尚未取得镜像/未做整机恢复”作为当前blocker。没有板端写入/上传候选/重启/M0启动。

## 用户实测补充（2026-10-03）

用户明确确认 **“整机恢复实测通过”**：`WHOLE_BOARD_RECOVERY_TEST=PASS / BOARD_OBSERVED_USER_REPORT`，无需重复恢复演练。模式、实际刷入镜像和恢复后 hash 未提供；旧备份身份不等于恢复后基线。整机恢复实测缺口关闭，AMP 专用 rollback/用户数据备份、boot 入口/加载源/验签/SMC/有效映射/coherency 仍需闭合，整体 **C. HOST_BUILD_PASS**。本次 Agent 只更新文档，无板端访问。见 [用户恢复证据](../rk3576-amp-board-evidence/RECOVERY_USER_CONFIRMATION.md)。以下保留历史记录。

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
