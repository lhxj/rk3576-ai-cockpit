# 当前状态：事实、约定和未知项

**P030 v4计数变化但未跨周期，v5条件时基修正已暂存（2026-10-04）：** v4 source/FIT/M0 entry/配套Linux通过，预检wraps0/ISR0/tick0后STOP；不能判IRQ失效，原固定读次数覆盖不足。用户及只读SSH确认完整冷恢复默认Debian。SDK同SoC vehicle-evb支持32K，v5借UART5物理TX粗略辨别32K候选，条件匹配才将本地SysTick LOAD239998改326，再核ISR与RT tick增长后允许首次延时。fresh编译/现有control-key验签/封装核验/只新增目录及独立读回PASS、旧树与默认入口保持、RAM清理；Agent未source/M0/KO/SSH MMIO/重启，未跑测试套件。**v5时基/延时修复效果待用户一次冷B，完整RPMsg/C/D仍未通过。** 见[新指南](reviews/rk3576-amp-platform-closure/P030_TICKDIAG_V5_GUIDE.md)、[v4执行](reviews/rk3576-amp-platform-closure/P030_TICKDIAG_V4_B_EXECUTION.md)、[v5准备](reviews/rk3576-amp-platform-closure/P030_TICKDIAG_V5_PREPARATION.md)。以下保留历史。

**P030 tickdiag-v4 被动暂存完成，等待单次实板诊断（2026-10-04）：** 当前首次 mdelay 未返回，实际 board init 忽略 SysTick 配置返回值。新补丁保存配置结果、仅本地寄存器/向量/ISR/tick 有界观测，在预检失败时 STOP/return，不进 RPMsg init 或 RT delay；不盲改时钟/映射/transport。v3 暂存后交付前发现打印超过128字节，已暂停保留；v4 拆短行最大99字节。新 v4 干净编译、现有 control-key 验签、容器/脚本校验、持锁只新增目录及独立读回 PASS，原启动与旧包 metadata 保持、RAM清理，仍默认 Debian。Agent 未 source/M0/KO/MMIO/重启；未运行测试套件。具体硬件根因、v4 tick/延时和完整 B/mapping/cache/RPMsg/D 仍未验证，C 关闭。 见[新指南](reviews/rk3576-amp-platform-closure/P030_TICKDIAG_V4_GUIDE.md)、[执行记录](reviews/rk3576-amp-platform-closure/P030_TICKDIAG_V4_PREPARATION.md)、[JSON](reviews/rk3576-amp-platform-closure/P030_TICKDIAG_V4_PREPARATION.json)。以下保留历史。

**P030 source-fix-v2 B 已执行：source/FIT/M0 entry/配套 Linux 通过，首次延时未观察到返回（2026-10-04）。** COM5 全文确认新脚本 banner、RSA p029dev/MCU hash、loader 成功、6.1.99-rk3576-m0echo-p026/root p3/stage=B 登录；COM6 remote_init 返回非空、link probe up=0，停在 first_mdelay begin tick=0 后 msh。用户确认持续无后续输出。tick/IRQ/定时器唤醒为候选，具体根因和完整 B/mapping/cache/RPMsg/D 未验证；C 关闭。用户已明确完整冷断电再上电；串口及获准重试的持锁只读 SSH exit0 确认默认 6.1.99-rk3576 #8/root p3/无 stage，脚本 SHA 保持且无 echo KO/RPMsg devices。前一次 SSH exit124 无输出保留。无 Agent 板写/M0/KO/MMIO/重启，未生成新固件或运行测试。暂停重跑，准备 tick/IRQ/定时器诊断。 详见[本次执行记录](reviews/rk3576-amp-platform-closure/P030_SOURCE_FIX_V2_B_EXECUTION.md)、[机器证据](reviews/rk3576-amp-platform-closure/P030_SOURCE_FIX_V2_B_EXECUTION.json)。以下保留历史状态。

**P030 脚本静默根因已定位，source-fix-v2 被动暂存读回通过（2026-10-04）：** 固定 v1 脚本长度表结束项为 0xffffffff，实际 source parser 要求 0，正文分派前静默拒绝；原 CRC/长度检查遗漏此条件。只改结束项和两处 CRC，CMD/FIT/U-Boot 保持；实际 source 190 项及安装器快照 7 项 Host 检查通过。新目录 /boot/amp-p029/initdiag-source-fix-v2 原子安装及独立 SHA 读回 PASS，原启动文件与既有 P029/v1 内容/metadata 保持，RAM 清理。最新 COM5 全文是另一组“先 source 被 bounds 拒绝、再 load、随后冷启动”的次序，并确认默认 Debian 冷恢复；不与最初 load/source 摘录混同。Agent 未执行 source/M0/KO/MMIO/重启。修正版 B 尚未执行，完整 B/mapping/cache/RPMsg/D 未通过，C 关闭。 见[执行记录](reviews/rk3576-amp-platform-closure/P030_INITDIAG_B_EXECUTION.md)、[新操作指南](reviews/rk3576-amp-platform-closure/P030_INITDIAG_SOURCE_FIX_V2_GUIDE.md)、[机器记录](reviews/rk3576-amp-platform-closure/P030_INITDIAG_SOURCE_FIX_V2.json)。以下各段保留当时历史状态，涉及“全文尚缺”等不代表当前状态。

**此前的 P030 SignedFix-v8 B 观察（2026-10-04；历史记录）：** 用户直接冷执行一次v8 load/source，proper149b1c5/7d8fe670、公钥DT43164981、验签/hash/M0load通过；最终DT通过并启动配套6.1.99-rk3576-m0echo-p026/B到Debian登录。COM6观察M0 entry/cache0x6cc/bypass1/msh，用户确认无后续waiting/预期15s timeout，**完整B未接受，C/KO继续关闭**。主控持锁40秒一次只读exit0：当前uboot完整8MiB匹配f9beef07…72c0b3，runtimeDT三精确no-map、12内存槽、iomem code/shared reserved、transport/mailboxes禁用、无RPMsg设备/echoKO，原six和旧23/新6 hashes一致，无Agent板写/MMIO/M0/重启。当前读回证明存储身份，不倒改用户跳过的事前门；在该次 B 记录时默认 cold Linux 证据尚缺；现已由用户报告恢复并经 SSH 核对默认系统，COM5 SPL/U-Boot 冷启动全文仍未收到。15s超时在remote_init返回后才计时，缺输出不能唯一证明mapping错误。当时计划准备 initdiag-v1 并手动执行一次 B；该次实际结果见当前 P030 initdiag-v1 记录。整体C/D关闭。见 [实际B记录](reviews/rk3576-amp-platform-closure/P030_B_EXECUTION.json)。以下保留历史。

**P030修复Host审核交付（2026-10-04）：** 真实最终DT链确认12槽含9个零占位，最小guard修复只跳过(0,0)，并独立reg_len检查全部reserved tuple，保留所有范围/no-map/验签门。新源码149b1c53干净构建、主控新cfg235断言/23完整流程/18负例与新ELF181factory入口及fullCI PASS；独立fdtget/ELF重提取/slot/tail/签名复核无must-fix，主控全读接受。新8MiB f9beef07…72c0b3仅proper代码变化，原公钥DT/签名AMP/BL31/TEE/尾4MiB逐字节保持；桌面5文件白名单SHA复核PASS，无私钥。**新板读回、默认冷Linux和B未验证；旧B/C撤回，整体C/D关闭。** 用户已冷恢复默认桌面，本轮一次SSH banner超时、远端未执行，无Agent板写/重启/M0/KO/MMIO。下一步按 [P030读回与默认启动指南](reviews/rk3576-amp-platform-closure/P030_FINAL_FDT_GUIDE.md) 由用户仅写uboot，再完整8MiB核对和默认Linux冷启动；Host通过不代替实板。见 [机器记录](reviews/rk3576-amp-platform-closure/P030_FINAL_FDT_REPAIR.json)。以下保留历史状态。

**P030：B已启动M0，最终Linux DT检查失败（2026-10-04）：** 用户一次3065B SignedFix-v8 B，required-conf RSA/payload校验通过，UART5记录RT-Thread、local_fn0x66ad与cache ctrl0x6cc/bypass1；booti在最终DT保护检查hang，未启动配套Linux，不能判完整B或mapping/RPMsg PASS。实际vendor arch/memory/final prep链Host重现：12槽memory reg包含9个(0,0)空槽，原guard误拒；no-map三段未被覆盖。另发现reserved reg循环长度被属性读取覆盖、第二tuple逃逸，纳入最小保护修复。旧B/C指南暂停，用户已完整冷断电回默认桌面；SSH banner核对超时、远端未执行。主控承担旧Host检查遗漏真实fixup顺序。整体C/D关闭；新proper-code-only修复先Host测试/clean build/独立审核，再完整8MiB读回和默认Linux门。见 [实际失败及根因证据](reviews/rk3576-amp-platform-closure/P030_FINAL_FDT_FAILURE.json)。以下保留此前历史状态。

**P029 SignedFix-v8默认Linux与新B暂存PASS（2026-10-04）：** 用户保存COM5日志确认soc cold boot、proper f8b4554、control DT哈希前缀43164981ef、policy0及默认boot.scr到原Debian；实时只读基线核默认6.1.99/root p3/boot p2、新完整8MiB及原sixhash/旧23一致。新B3065B保留v6配套资产和root/内存保护，改用固定签名FIT并增130560B长度门；39脚本fault、18 FD/原子不覆盖、80 vendor formatter、181实际SOURCE/CRC/libfdt及full Host CI PASS，主控接受独立审核。持锁一次仅新增signature-fix-v8六文件、独立读回PASS、旧23/原six/链接metadata保留，RAM精确清理。COM5/COM6均枚举OK。**目标硬件AMP验签/M0/cache/mapping/RPMsg仍未实测，整体C/D关闭，旧B/C暂停；下一步用户保存双日志后仅冷启动新B一次。** Agent未启动M0/KO/MMIO或重启，不改uboot/GPT/默认入口。见 [默认Linux记录](reviews/rk3576-amp-platform-closure/P029_SIGNED_DEFAULT_LINUX_RESULT.json)、[新B暂存与边界](reviews/rk3576-amp-platform-closure/P029_SIGNED_STAGE_B_RESULT.json)、[新B指南](reviews/rk3576-amp-platform-closure/P029_SIGNED_STAGE_B_GUIDE.md)。以下保留各时点历史状态。

**2026-10-03 23:49 P029 SignedFix-v8完整读回PASS：** 用户RKDevTool截图显示仅Uboot/EMMC/地址0x4000，测试设备成功、候选下载100%/完成，随后确认导出完成。主控实际读取ExportImage.img=8388608B，完整SHA `ef857b106476cb6fc775cd3837db3b5d254d6ff10103a9281669dffd7e67f1e4`，与已审核候选逐字节一致（first mismatch=-1），另存忽略目录并核归档身份。此前测试设备/下载Boot失败截图保留，具体原因未证明。**新uboot读回PASS；新默认Linux冷启动、目标AMP验签、M0/cache/mapping/RPMsg仍UNVERIFIED；signedFIT未暂存，旧B/C继续暂停，整体C/D关闭。** 本轮Agent只Host校验/归档，无板端访问/写入/重启；物理刷写/导出由用户完成。下一步完整冷断电正常启动原Linux并保留Debug日志，不执行stage脚本。见 [读回证据与限度](reviews/rk3576-amp-platform-closure/P029_SIGNED_UBOOT_READBACK.json)。以下保留此前未写入的历史状态。

**P029签名修复Host包已审核交付（2026-10-03）：** 主控保留实测P028程序/BL31/TEE，仅在trusted内置control DT新增单required=conf开发测试公钥，对AMP config显式覆盖loadables签名，原M0/contract不改。真实vendor验签30项PASS（1合法accept、23拒绝、6软件checker盲点由独立guard拦截），主控独立复跑、RSA因素数学/cert匹配、181 factory实际C回归、preload/geometry与full CI均PASS；独立最终审查无must-fix，主控已接受。桌面SignedFix-v8白名单6文件逐项SHA一致，私钥不交付。新完整8MiB SHAef857b…67f1e4、签名FIT331431…6c041；**新U-Boot尚未写入/读回/冷启动，signedFIT尚未暂存，B/C暂停；C/D关闭。** 本轮仅一次持锁有界SSH只读确认默认冷恢复/P0288MiB/原sixhash，无Agent板写/重启/M0/MMIO/KO。下一步先 [新U-Boot8MiB写入、完整读回和默认Linux冷启动](reviews/rk3576-amp-platform-closure/P029_SIGNED_UBOOT_GUIDE.md)，再交付新B入口。Host RSA不等于目标硬件RSA/MCU/cache/mapping/RPMsg证明；无OTP或ROM secure-boot变更。见 [具体修复与证据](reviews/rk3576-amp-platform-closure/P029_SIGNED_REPAIR.md)。以下保留失败/准备历史。

**P029阶段B验签失败（2026-10-03）：** 用户一次执行v6 B脚本，Debug输出 `No RSA key found`、`ret=-13`，UART5/COM6无输出。实际厂商image verifier在trusted U-Boot control DT缺 `/signature` 时拒绝，发生在reserve/copy/M0 release之前；通用“required signature”报错不代表存在required-image key。旧Host policy测试stub遗漏此路径，主控承担准备缺口。**B_FAILED_SIGNATURE_GATE，v6 B/C执行指南暂停，不能同次boot/retry；整体C/D关闭。** 用户随后完整断电默认启动回桌面、IP未变化。主控正准备单个required=conf可信内置公钥及签名AMP FIT的Host修复包，保留所有校验，不改OTP/BL31/TEE；仅外部FIT或Linux DT加key不足以修复。新包尚未交付/写入/实测。历史准备记录保留。

**P029阶段B采集准备（2026-10-03）：** 用户报告RV-debugger-plus已接UART5，随后截图新增COM6；主控Windows只读复核COM6 USB Serial Port/Status=OK/VID_0403/PID_6010，原Debug CH340 COM5保留。现有双窗口方案为COM5 Debug 1500000、COM6 UART5 115200，均8N1/无流控，后者仅RX+GND，不接TX/供电。Host再次核B脚本2738B/SHA与已读回v6相同，单次loader/root override/随后booti顺序正确。**接线USER_REPORTED、COM枚举PASS，尚未观测UART5输出或B启动；整体C/D关闭。** 沿已有任务授权，按 [阶段B当前指南](reviews/rk3576-amp-platform-closure/P029_STAGE_B_COLD_BOOT_GUIDE.md) 先打开保存双日志，再由用户另一次完整冷上电执行一次B；不启动C/KO。

**P029 v6阶段A实测PASS（2026-10-03）：** 用户串口cold boot、proper U-Boot f8b4554/policy0、2525B新A脚本、配套6.1.99-rk3576-m0echo-p026到Debian登录，最终root=/dev/mmcblk0p3和stage=A。主控一次有界SSH只读核实际DT三段no-map、无reusable且均不与System RAM相交；MCU/RPMsg/mbox0/4/UART5 disabled，无RPMsg设备/echo模块；独立子代理保存记录复核PASS。A没有启动M0，Agent未写板/重启/MMIO/KO。下一步B先落实release前UART5采集，用户仅一只Debug USB-TTL，现包没有已审核单适配器采集流程；M0/BL31 setter/有效mapping/cache/RPMsg及M0测试后冷回滚仍未实测。**A验收PASS，整体C. HOST_BUILD_PASS，D关闭。** 见 [阶段A实际结果](reviews/rk3576-amp-platform-closure/P029_STAGE_A_RESULT.md)。以下保留各阶段历史。

**P029阶段A根分区失败已定位（2026-10-03）：** v5脚本已启动配套kernel，但HDMI救援终端报短PARTUUID不存在。固定DT→env合并覆盖脚本root；用户默认冷启动已恢复原Debian，SSH确认p3、原sixhash/P0288MiB/旧物料一致，短blkid不匹配/完整查询p3。新的checked envext root脚本Host57 vendor bootargs/118fault/175filesize及full CI PASS，独立核心审核PASS，主控持锁一次仅新增root-fix-v6小目录，旧文件/原件保留、独立读回与RAM清理PASS；新A2525B。新A登录/实际DT仍未验收，M0/B/C未执行，整体C、D关闭。见 [根分区修复](reviews/rk3576-amp-platform-closure/P029_STAGE_A_ROOT_FIX.md)。以下保留历史。

**P029阶段A脚本修复（2026-10-03）：** 首次旧A在STOP length Image退出，用户filesize=0x2930200；vendor formatter与旧bare字符串失配。已修复六处0x比较并清旧值，175 vendor C/112 fault项等Host检查及独立审查PASS。仅新增script-fix-v5目录，旧10文件/3receipt及原默认六文件保留，独立读回PASS，RAM清理，仍原Linux。新A脚本2368B，用户需按新路径冷启动；没有M0/重启，整体C、D关闭。见 [脚本修复](reviews/rk3576-amp-platform-closure/P029_STAGE_A_SCRIPT_FIX.md)。以下保留历史。

**P029阶段A暂存PASS（2026-10-03）：** 用户确认连接恢复并批准本任务所有操作；仍按A→B→C验收。当前板P028完整8MiB只读SHA匹配，Host stdin安装器一次执行成功，boot10/module268/3receipt读回PASS，原六内容和启动链接/路径自身metadata前后相同，RAM源包及root快照前缀均清理。首轮未压缩上传只有部分RAM包，持久目录尚不存在；一次压缩传输修复后安装，失败历史保留。原Linux仍运行，无Agent重启/M0/KO/MMIO操作。**A冷启动待用户手动执行，C. HOST_BUILD_PASS，D关闭。** 见 [实际暂存与交接](reviews/rk3576-amp-platform-closure/P029_STAGE_A_RESULT.md)。以下保留历史。

**P029阶段A已批准（2026-10-03）：** 用户明确批准已交付v4的被动文件暂存及手工A冷启动，M0/RPMsg关闭。Host再次21文件SHA与安装器身份复核PASS；既有alias、当前已知IP的WSL SSH及Windows原生TCP均连接超时，没有认证/板端预检、上传、写入或重启。等待Debug串口报告当前Linux IPv4/板端状态，重连后沿用A批准，先核身份/空间再暂存。**A_ASSET_STAGING=BLOCKED_NETWORK，A_LINUX=UNVERIFIED，C. HOST_BUILD_PASS，D关闭。** 见 [A批准与实际预检](reviews/rk3576-amp-platform-closure/P029_STAGE_A_RESULT.md)。以下保留历史。

**P029 Host准备完成（2026-10-03）：** 已交付A/B/C独立DT/脚本、配套Linux/initrd/模块、有界M0诊断FIT和回滚指南；21文件桌面SHA复核PASS。fresh M0 build零warning，67脚本/包 +14实际M0 C +6snapshot故障项、实际P028 preload/libfdt及full Host CI PASS。只Host，未访问实板；A暂存/配套Linux、B/C有效mapping/cache/RPMsg均UNVERIFIED，未取得本轮部署批准。用户只有一个Debug USB-TTL，A足够，B/C UART5采集待落实。**C. HOST_BUILD_PASS，D关闭**。 见 [审核与下一步](reviews/rk3576-amp-platform-closure/P029_HOST_PREPARATION_REVIEW.md)。以下保留历史证据及其当时状态。

**P028新Linux启动PASS（2026-10-03）：** 完整8MiB新P028此前读回与获批SHA9bd8cc03...71c6d5e逐字节相同；用户新串口确认 proper U-Boot `f8b4554`、banner后运行时policy=0、原 `/boot.scr` 与 `booti`，随后原 `6.1.99-rk3576 #8` 到Debian12登录。新proper策略调用和原厂Linux启动兼容缺口闭合；补充日志确认Ctrl+C进入CLI、help显示amp_m0load用法、手动boot及uname/cmdline身份PASS；help没有加载/启动M0。不是AMP FIT/M0或所有外设测试。M0有效映射/reset/cache、BL31 MCU setter与配套RPMsg仍UNVERIFIED；**C. HOST_BUILD_PASS，D关闭**。Agent只Host解读与归档。见 [P028实板结果](reviews/rk3576-amp-platform-closure/P028_LINUX_ONLY_BOARD_RESULT.md)。以下保留此前各阶段的待审批/失败/读回历史。

**2026-10-03 15:09 P028完整读回PASS：** 用户截图Uboot已改选新P028，导出成功；Host实读新ExportImage.img=8388608B，与获批候选逐字节相同，SHA=`9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e`，已独立保存忽略目录。**新candidate readback=PASS；新Linux冷启动尚UNVERIFIED**，可按既有有限批准由用户正常冷启动，M0关闭。此前14:43原件读回及随后原件路径截图保留历史，不将失败记录改写为PASS。Agent只Host校验，整体C。见 [执行历史和当前读回](reviews/rk3576-amp-platform-closure/P028_LINUX_ONLY_EXECUTION.json)。

**2026-10-03 P028读回未匹配新候选：** 用户导出成功，Host实读14:43更新的ExportImage.img=8388608B，SHA=`ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`，逐字节匹配原件；预期新P028为9bd8cc03...71c6d5e，未匹配。**新candidate写入未证明、读回BLOCKED、Linux冷启动未验证**。停止新候选冷启动/重复写入，等待下载镜像页所选文件名及完整写入日志，原因尚UNVERIFIED。不能归因用户或认定其他分区状态。Agent只Host分析，整体C不变。见 [实际读回](reviews/rk3576-amp-platform-closure/P028_LINUX_ONLY_EXECUTION.json)。

**2026-10-03 P028测试批准：** 用户明确批准仅更换新P028 U-Boot并验证原Linux、M0保持关闭；候选8MiB/hash9bd8cc03...71c6d5e与原件/MiniLoader在Host复核一致。仅指导用户手工MASKROM/RAM Loader→现有EMMC uboot→读回→原Linux冷启动。**新写入/读回/启动未观测**，C不变、D与AMP部署门关闭，不扩大到boot/GPT/IDBlock/M0。见 [审批与执行状态](reviews/rk3576-amp-platform-closure/P028_LINUX_ONLY_EXECUTION.json)。

**2026-10-03 P028 Host修复交付：** 原Linux恢复PASS后，主控集成0009/Host build/test并独立clean构建，181项factory script/boot policy检查、FIT/preload及full CI PASS。新8MiB候选仅用于另行审批的Linux-only兼容，SHA9bd8cc03...71c6d5e；BL31/TEE/control DT与实际原件相同。不把Host成功当作新镜像实板启动成功，D关闭、C保持。见 [审核与待审批物料](reviews/rk3576-amp-platform-closure/P028_HOST_REPAIR_REVIEW.md)。本轮只Host操作，未访问实板。

**2026-10-03 P028恢复进展：** 用户经MASKROM RAM Loader读取GPT、仅写EMMC的uboot原8MiB后，按起始/数量均0x4000扇区只读导出。Host实际读取新导出文件，size=8388608、SHA256=`ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`，与原备份完全一致，已另存忽略的Host证据目录。**ORIGINAL_UBOOT_READBACK=PASS；ORIGINAL_LINUX_COLD_BOOT=PASS。** 用户最新串口原始文本与明确确认：proper U-Boot `8f53f800da` → 原 `/boot.scr` → `6.1.99-rk3576 #8` → Debian 12登录。原始日志仅在忽略目录保留；不把登录成功外推为所有外设功能已回归。 旧P026候选保持撤回，D门关闭，C不变。Agent未写板/重启/M0操作；记录 [P028读回身份](reviews/rk3576-amp-platform-closure/P028_RECOVERY_READBACK.json)。下面“当前原Linux不可用”是失败时刻的历史状态；现在原Linux已恢复至登录界面。

**2026-10-03 P028紧急更新：** 用户手工写入 P026 候选后，串口确认 proper U-Boot `2314a3f` 和候选 payload hash 前缀 `6ea779087d`，但原 Linux 启动失败：`FIT: No FIT image` 后 `No CLI available`。P026/P027候选与写入指导已撤回，生产脚本在所有 I/O 前无条件停止。根因是 FIT_SIGNATURE 触发 CLI 禁用、默认 boot_fit 与 legacy boot.scr 禁用三项兼容问题。用户截图确认 MASKROM 可识别，正按 RAM-only LoaderToDDR 前置步骤恢复；**原 U-Boot 恢复未观测、当前原 Linux 不可用**。子代理仅在独立 Host 源码修复。整体仍 **C. HOST_BUILD_PASS**，D门关闭，无 M0 启动证据；Agent没有写板/重启。见 [P028失败与恢复](reviews/rk3576-amp-platform-closure/P028_LINUX_ONLY_FAILURE.md)。以下历史记录保留其当时范围。

**2026-10-03 P026最新AMP证据：** 用户批准中午12:00前受控取证；原厂OP-TEE只读command5返回4B flag0，原8MiB U-Boot已复制并核六个payload hash，UART5两脚MUX/GPIO UNCLAIMED。原firmware/boot/modules及MiniLoader另存Windows恢复目录并核hash。独立子代理修复复制前完整code/shared预留、Linux Image/initrd/DT防覆盖与no-map检查，主控阅读/复测；无GPT改动的显式boot文件入口、fresh独立Linux release/255modules/配套initrd和精确rollback已形成。整体仍 **C. HOST_BUILD_PASS**：实际映射/reset/cache及新Ub/MCU setter动态能力未证明，不启动M0。未修改boot/GPT/OTP/MMIO、未加载KO/重启；唯一新板端程序是获批RAM临时只读诊断，已清理。见 [P026结论](reviews/rk3576-amp-platform-closure/P026_CLOSURE_RESULT.md)。以下为历史，不把旧“缺备份/分区方案/flag”当当前完成事实。

整理日期：2026-10-01。来源是本次对话中用户提供的实板输出与确认；
不是本包生成过程对实体板的实时读取。历史日志日期可能受板端时钟影响。
新增记录注明时间、命令、实际输出、版本与证据位置。

**2026-10-01 AMP 更新（新证据）：** 固定 RK3576 RT-Thread/HAL 候选已完成 Host 构建；派生 M0 最小 RPMsg echo clean build 通过。上板前等级仍为 **C. HOST_BUILD_PASS**：候选 FIT `0x47800000` 与参考 RPMsg vring 同址，BUS M0 shared DDR remap 未证明；当前板无 `amp` 分区与 AMP/RPMsg DT 节点。只读 boot/DTB/分区盘点和所有 blocker 详见 [preboard SUMMARY](reviews/rk3576-amp-preboard/SUMMARY.md)。以下历史段落保留其当时证据层级。

## 1. 主机 / Git / SSH

| 项目 | 最后已知情况 | 证据等级 |
|---|---|---|
| WSL | Ubuntu 22.04.5 LTS，x86_64 | USER_LOG |
| WSL磁盘 | 当时df报告758G available；WSL虚拟磁盘视图不保证Windows宿主还有同等物理空间 | USER_LOG |
| 工作区 | `/home/ywx/rk3576-work/cockpit/rk3576-ai-cockpit` | USER_LOG |
| Git | `.git`存在，main，无commit；可能已按之前指引操作，须重新检查 | LAST_OBSERVED |
| GitHub CLI | 用户lhxj已登录，Git走HTTPS | USER_LOG |
| GitHub仓库 | 期望lhxj/rk3576-ai-cockpit，是否已创建/推送尚未核验 | UNVERIFIED |
| 板端SSH | WSL使用`ssh lubancat`，用户cat，公钥登录已由用户确认完成 | USER_CONFIRMED |
| USB-TTL | 已有 | USER_CONFIRMED |
| WLXray环境 | 曾在WSL激活其venv，不能用于本项目 | WORKFLOW_CONSTRAINT |

## 2. 开发板

- LubanCat-3 v2 标准单板，Rockchip RK3576。
- RAM约4GB，eMMC约32GB，依据用户lscpu/free/lsblk及板型记录。
- Debian GNU/Linux 12 Bookworm。
- Kernel `6.1.99-rk3576`，构建信息2026-04-24。
- 镜像 `lubancat-rk3576-debian12-gnome-20260424`。
- Device tree：`rk3576-lubancat-3-v2.dtb`。
- 初始CPU在线0-7；不能仅凭这条判断所有可能的片内RTOS方案。
- **历史** `/boot/uEnv/uEnv.txt -> uEnvLubanCat3-V2.txt`。
  之后曾建议使用`sed -i`，可能已将软链接替换为普通文件；第一轮必须重新`ls -l`和`readlink -f`。
  不擅自“修复”链接；先核对实际启动配置读取路径。

## 3. 已验证功能

| 功能 | 已有证据 | 尚未覆盖 |
|---|---|---|
| RTL8822CE | PCIe识别，驱动rtw_8822ce，Wi-Fi联网，SSH/SCP | 长期吞吐/恢复/RTSP质量 |
| 音频 | `hw:0,0`录5秒16kHz/mono/S16_LE，耳机可回放 | 全双工、长稳、ASR/TTS集成 |
| 音频命名 | ALSA卡名rockchip-es8388，日志出现ES8323 | 物理Codec型号不要只由卡名推断 |
| CAM0单摄 | OV8858，1632×1224 NV12，300帧约29.87~29.88fps，用户确认JPG颜色正常 | 双摄并发、长稳、编码/NPU同载 |
| CAM1单摄 | 更换到已知正常排线后完成30帧并正常停流 | 长稳与当前线缆状态 |
| Camera B | Camera B + Cable A采流正常 | 不据短测声称所有光学/AF能力通过 |
| USB触摸 | WaveShare WS170120，0eef:0005，hid-multitouch，历史event6 | event编号不是固定映射 |
| HDMI/触摸交互 | 用户最新确认正常显示且可以触控 | 真实mode、刷新率、X11/Wayland、Qt平台插件未盘点 |
| MPU6050 | 用户已有 | 未接入/未采样/未分配RTOS I²C资源 |

LED / 按键 / 蜂鸣器：用户明确决定软件模拟，不购买为前提，不宣称GPIO实测。

## 4. 摄像头当前约定与已知问题

- 当前软件开发约定：Camera A + 已知正常线缆，CAM0启用，CAM1禁用。
- CAM1替换BTB排线需要等待；“至少两天”是用户当时估计，不自动认定某日已到。
- Camera B使用Cable A可正常采流，问题**高度集中于第二套连接组件**。
  尚未把故障完全定位到哪段FPC、BTB接头或永久损伤，不能无证据写成烧毁。
- 双摄曾完成两路probe及节点创建，但CAM1曾ERR2；**双摄同时稳定出帧未通过**。
- 物理CAM0历史sensor `3-0036` → DPHY0 → rkisp-vir0。
- 物理CAM1历史sensor `4-0036` → DPHY1 → rkisp-vir1。
- 单摄mainpath曾为video11；双摄时曾为video22/video31。
- `/dev/video-camera0`在双摄时曾指向video31，不能作为前摄的永久别名。
- 当前每路真实节点须运行时核验；配置文件样例默认不绑定节点。
- 当前格式曾为MPLANE API、NV12、num_planes=1、stride=1632、sizeimage=2996352。

## 5. 尚需回归的警告

- Camera启用后日志出现ES8323寄存器写入-6，之后没有完整的音频回归证据；需受控回归。
- 单摄有`vblank need >=1000us ... cur 693us`；短测通过不等于长期/双摄无风险。
- 异常线缆采流出现`MIPI_CSI2 ERR2:0x10000000`；本包不硬解释未知位定义。
- 初期有regulatory.db缺失、DPK calibration警告；Wi-Fi实测联网，但无线性能/法规配置仍待核对。
- 曾有journal非干净关机提示；不可用等待固定秒数代替确认正常关机完成。

## 6. AMP / RPMsg：选定路线，尚未实现

历史运行kernel配置：

```text
CONFIG_MAILBOX=y
CONFIG_ROCKCHIP_MBOX=y
CONFIG_RPMSG=y
CONFIG_RPMSG_NS=y
CONFIG_RPMSG_ROCKCHIP_MBOX=y
CONFIG_RPMSG_VIRTIO=y
# CONFIG_REMOTEPROC is not set
# CONFIG_RPMSG_TTY is not set
# CONFIG_RPMSG_CHAR is not set
# CONFIG_RPMSG_CTRL is not set
```

已选择Linux + RT-Thread片内AMP + RPMsg；未确认RTOS固件、启动链、核号、
保留内存/vring、通知方式、Linux用户态接口、真实SDK是否齐全。
不保证有`/sys/class/remoteproc`、`/dev/ttyRPMSG0`或`/dev/rpmsg*`。
MPU6050资源分配必须等SDK/板级资源审查，不能抢走Camera/Audio所在I²C。

## 7. 软件状态

本启动包仅提供规则、任务、README、配置样例、只读脚本和host构建烟测。
Qt、media_srv、audio_srv、voice_srv、infer_srv、vehicle_core、rpmsg_srv、
RTOS业务都**尚未由本包实现或验证**。厂商例程、完整SDK与参考工程尚需获取/核验。
所有PASS指此前用户的具体测试，不代表当前Codex可以跳过盘点。

## 8. AMP Host 候选包（2026-10-02）

**C. HOST_BUILD_PASS；尚未上板。** RK3576 BUS M0最小echo的独立冷启动proposal已clean Host构建（122,696B、无warning），包含cache bypass和有界payload地址转换；Linux pool/barrier、U-Boot reset/SiP错误传播patch与生成式contract已形成。完整候选Kernel Image/modules/v2DTB、echo ko、U-Boot bin、Host overlay/FIT构建通过，Host一致性/CI通过。

普通只读SSH已复制boot原件和运行DT到Host，已有Image/uEnv/DTB/hash；不是完整eMMC备份。当前CON值、板上AMP加载/签名入口、动态SMC、实际UART5和完整恢复介质仍需证据，**无D、无部署授权**。详见 [P023交接](reviews/rk3576-amp-platform-closure/HOST_PREBOARD_PACKAGE.md)。本包没有新增sensor/vehicle业务或验证实板RPMsg。

## 9. AMP恢复文件/Windows工具（2026-10-03，Host）

完整官方Debian12 GNOME 20260424 `update.img`已取得，整文件发布MD5、内部content MD5和SHA256校验通过。解析器exit0、标准dumpimage/debugfs只读提取后逐字节比较：恢复包U-Boot/BL31/Kernel/DTB/boot脚本与前轮板端原件一致。原件同时复制到桌面并核hash。Windows RKDevTool3.32已解压，Rockusb5.14目录签名Valid，PnPUtil预装exit0并枚举为oem71.inf；没有请求重启。

本轮未访问开发板或运行镜像代码。恢复介质文件缺口已关闭；USB/MR实际识别、用户数据备份、实际AMP boot入口/验签/动态SMC及最终可部署changeset仍未闭合，**C. HOST_BUILD_PASS**。详见 [P024恢复核验](reviews/rk3576-amp-platform-closure/RECOVERY_IMAGE_ANALYSIS.md)、[Host工具](reviews/rk3576-amp-platform-closure/HOST_RECOVERY_TOOLS.md)。Host CI：2项CTest、22项Python PASS。

## 10. 整机恢复实测用户确认（2026-10-03）

用户明确回复 **“整机恢复实测通过”**。记录 `WHOLE_BOARD_RECOVERY_TEST=PASS`，证据等级 **BOARD_OBSERVED_USER_REPORT / USER_CONFIRMED**；无实际刷写命令、工具日志或恢复后 hash 提供，不能写作 Agent 执行/观测。无需重复整机恢复演练。具体模式和实际刷入镜像未提供，旧 boot 备份 hash 不自动当作恢复后当前状态。

本次 Agent 仅更新文档，无板端访问、部署或 M0 启动。后续只读盘点应重新核恢复后的版本/DT/分区/boot hash。AMP 专用精确 rollback、用户数据备份及 boot/映射/coherency 仍需闭合，整体 **C. HOST_BUILD_PASS**。见 [恢复实测证据](reviews/rk3576-amp-board-evidence/RECOVERY_USER_CONFIRMATION.md)。

## 11. P025：恢复后只读基线、AMP验签与changeset（2026-10-03）

普通SSH exit0：恢复后Kernel`6.1.99-rk3576 #8`、U-Boot`8f53f800da-04/24/2026`/BL31`v1.14`、v2 model及六项boot hash与原件一致，重新复制原boot/运行DT；仍无amp GPT/AMP DT。已有完整恢复包和用户整机恢复PASS无需重复。

Host无重叠地址/cache bypass+Linux uncached pool方案复核PASS（9672检查）；新增required-conf-key/FIT policy和partition边界U-Boot派生补丁，signature-enabled完整fresh build、原BL31/OPTEE/DT保留封装及46+3条件测试PASS。产物未签名、未上板。机器changeset核文件身份PASS、准入检查BLOCKED/exit2；真实验签policy、amp GPT/数据计划、复制前RAM保护、动态SMC/mapping/cache等仍需闭合，整体 **C. HOST_BUILD_PASS**。

见 [P025结果](reviews/rk3576-amp-platform-closure/P025_CLOSURE_RESULT.md)、[部署回滚清单](reviews/rk3576-amp-platform-closure/P025_DEPLOYMENT_AND_ROLLBACK.md)。本轮仅Host/普通只读SSH，无MMIO/SMC重试、上传候选、写板、reboot或M0启动。

用户后续完整串口日志显示SPL `Verified-boot:0`与六个原厂payload SHA检查PASS：记录的SPL FIT未要求签名（BOARD_OBSERVED_USER_REPORT）。proper U-Boot AMP验签走独立OP-TEE flag路径，日志未触发，仍UNVERIFIED；不能据SPL的0或缺security分区推定AMP允许unsigned。完整日志证据已取得，无需重复请求。见 [分阶段验签证据](reviews/rk3576-amp-board-evidence/SPL_VERIFIED_BOOT_SERIAL_EVIDENCE.md)。整体仍C，本补充只做Host分析/文档。
