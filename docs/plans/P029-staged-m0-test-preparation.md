# P029：分阶段 M0 测试包准备

## 当前里程碑：B实际执行后的最终Linux DT检查失败（2026-10-04）

用户单次SignedFix-v8 B实测：required-conf RSA p029dev及payload hash通过，loader返回成功，UART5记录RT-Thread、local_fn=0x66ad与cache ctrl=0x6cc/bypass=1；随后booti在final Linux DT no-map保护检查拒绝并hang，未进入配套Linux。不能把M0执行或cache快照当作完整B/mapping/RPMsg验收。要求完整冷断电回默认；用户已确认桌面/IP同。旧v8 B/C执行指南立即暂停，本次不重试。

本轮一个里程碑、三项任务：①仅Host归档实际失败并由既有子代理独立定位完整arch/board/final DT路径；②保留所有保护，重现具体失败后做最小修复，测试真实vendor memory fixup→最终guard顺序及相关恶意/边界场景；③主控审核、必要clean build/封装完整新8MiB候选与精确身份/回滚指南，提交既有Draft PR。沿用本任务用户全部操作授权，物理冷启动/刷写由用户执行；不Agent启动M0、KO、MMIO、重启，不越过读回和默认Linux门。子代理仅Host独占测试/审核，无板访问、不套娃。源码/API/日志仅作证据，真实硬件mapping/timeout/RPMsg/D仍未验证。

已用真实vendor函数链重现：arch_fixup_fdt把全部CONFIG_NR_DRAM_BANKS槽传给vendor fdt_pack_reg，零槽也写入memory reg；现guard把零槽当非法。另检查reserved reg循环长度被no-map/reusable属性读取覆盖，避免遗漏第二tuple保护。不得仅删除final guard或放宽区域边界，明确记录旧Host测试遗漏实际fixup顺序。

实际Host结果：独立根因46断言与生产补丁审核PASS；主控使用新clean cfg12槽复跑235断言/23完整final prep/18负例PASS，实际新ELF/原脚本/公钥control DT的181项factory入口回归PASS，full CI（2 CTest/41 Python/5withdrawn）PASS。最小源码149b1c53仅修改rockchip_amp.c；新8MiB f9beef07…72c0b3仅改变proper代码，control DT/AMP/BL31/TEE/tail保持。封装及独立fdtget/ELF/slot/tail/key审查PASS，主控全读接受并完成桌面5文件交付SHA核对；新板读回/默认冷Linux/B均UNVERIFIED。用户恢复桌面后一次只读SSH banner超时，远端未执行，本轮未写板。

## 前一里程碑：SignedFix-v8默认Linux基线与新B入口（2026-10-04）

用户提供原Linux6.1.99-rk3576 #8、root p3、无amp_test_stage、fwver uboot-f8b4554及原IP；此前新8MiB完整读回PASS。一个里程碑、三项任务：①主控持锁有界只读SSH核实时uboot完整8MiB/原sixhash/相关默认节点和被动物料；②Host仅为B生成新signature-fix-v8路径入口，使用已审核130560B签名FIT，保留v6配套kernel/initrd/B_DT、root override/内存保护/单次加载/失败冷断电规则；③独立审核与必要实际parser/CRC/签名/安装边界检查后，仅新增被动目录并独立读回，交付用户下一次单次B测试指南。旧B/C保持暂停，M0启动与冷上电由用户完成，不在Agent SSH中执行，不开C/KO。

沿用已有本任务全部操作授权，不重复询问权限。新增文件前核签名v8身份、完整新uboot与默认原件、目标路径absent、空间、旧物料前后不变；失败停止且不覆写旧目录。要求补充已保存的本次DDR/SPL/U-Boot日志，以区分全8MiB存储身份、默认Linux正常和本次实际control DT加载；未收到时可做独立Host准备/只读基线，不把缺失串口验签证据记PASS，不交付可执行B指令越过该门。整体C/D关闭，真正硬件AMP验签/有效mapping/cache/RPMsg待实测。实际结果在本节续记。

本里程碑实际：用户已补完整COM5冷启动日志，新control DT43164981ef/proper2e1b965bfb/policy0/默认boot.scr/原Debian链明确；主控30s持锁只读完整新8MiB/原six/旧23/目标absent与空间PASS，另一次15s只读确认sysfs uboot label。COM5/COM6本次Pnp均OK。新B3065B/SHA96921b…64da0d，仅签名FIT路径/size guard/banner变更，配套kernel/initrd/B_DT/root guard保持。39/18/80/181及full CI PASS，独立无mustfix、主控全读接受；一次被动installer exit0，新6/旧23/原six/links/new8MiB/defaultLinux独立读回PASS，RAM精确cleanup及temp prefix absent。未启动M0/KO/MMIO或重启；硬件AMP签名/B execution/cache/effective mapping/RPMsg仍未验证，整体C/D关闭。具体记录P029_SIGNED_DEFAULT_LINUX_RESULT/P029_SIGNED_STAGE_B_RESULT，下一步用户按新指南单次冷B，旧B/C保持暂停。

## 前一里程碑：SignedFix-v8完整读回与默认冷启动交接（2026-10-03）

用户沿已批准流程完成新Uboot下载/导出。一个里程碑：①Host实际读取完整8MiB并核候选SHA/逐字节一致；②保存原始读回及失败/成功截图到忽略目录，公开记录脱敏结论和证据身份；③交接用户完整断电默认Linux冷启动，旧B/C保持暂停。没有Agent板端访问或动作，不把读回等同目标AMP验签/M0启动。

实际：ExportImage.img mtime23:49:02+08:00、8388608B、SHA ef857b106476cb6fc775cd3837db3b5d254d6ff10103a9281669dffd7e67f1e4，与候选逐字节一致。归档身份一致，receipt SHA70cb0c48fb529ed5b038424f1ecd0739aa319a3097ba65373185aae3c859533f；raw被git忽略。此前工具测试设备/下载Boot失败保留，原因未证明。新默认Linux冷启动、目标硬件RSA/M0/cache/mapping/RPMsg未验证，signedFIT未暂存，整体C/D关闭。按指南后续只默认启动、不执行旧stage脚本；等待用户Debug日志和uname/cmdline后再读取实时Linux基线。

## 前一里程碑：B实际验签失败与Host签名修复（2026-10-03）

用户单次v6 B失败于缺/signature、ret=-13，源码确认reserve/copy/release前拒绝，COM6无输出不能据此判断接线。用户已完整断电默认回桌面/IP不变；旧B/C指南暂停。既有“批准所有操作”适用于本任务，仍保留物理冷启动由用户完成和A→B→C技术门。

本轮一个里程碑：①归档失败和独立根因审查；②不改P028 C代码，使用固定vendor工具生成私密Host开发测试key，trusted embedded control DT仅新增单required=conf公钥，签AMP config显式覆盖loadables，重封装完整8MiB并保留代码/BL31/TEE/尾部；③真实vendor verifier与密码学/篡改负例、原factory parser回归、主控审核后形成具体新包/部署回滚清单并提交既有Draft PR。不得降级verify、伪造trust anchor、签后改authenticated metadata或绕过缺key。新硬件RSA/U-Boot默认回归/M0/D均保持UNVERIFIED。子代理仅Host审查和其专属签名测试脚本，不接板/不套娃。

新key为本次开发测试的AMP认证锚，不设置OTP/ROM secure boot；loader容器本身仍沿实测policy0的SHA256路径。私钥不提交/不上传板/不复制桌面，原始证据忽略保存。与原已批准策略相比，AMP由policy0 hash-only改为单required-conf锚强制验签，必须在交付文档明确描述。

本里程碑完成：真实vendor新签名/负例30项（1合法、23拒绝、6checker盲点由独立数学/contract guard拦截），主控独立复跑PASS；公钥因素/cert绑定、原control DT非signature属性/memreserve/bootCPU、程序/ATF/TEE/尾4MiB、两slot逐字节核验PASS。builder改用canonical layout和M0向量，fresh generator全流程PASS；新control实际factory C/CRC/env181项、新FIT/实际B_DT preload/geometry及final full CI PASS。独立最终review无must-fix，主控已阅读接受，报告SHA见P029_SIGNED_REPAIR.json。Desktop SignedFix-v8六文件白名单逐项SHA匹配，无私钥；guide仅先现有uboot8MiB写/读回/defaultLinux回归，旧B/C指南停止。

本轮板端仅一次持锁30秒上限stdin只读：用户冷默认桌面之后核kernel/boot p2/root p3/P0288MiB/原sixhash，PASS；无上传/持久写/M0/MMIO/KO/Agent重启。新包目标硬件RSA/SPL/默认Linux、M0有效mapping/cache/RPMsg与D仍未证明。vendor outer FIT新增Host memreserve无已查实的固定loader消费者，仅SOURCE_INFERRED；control DT原reserve保持，须保留真实cold boot门。按流程commit/push既有Draft PR，下一步依赖用户物理操作和完整新8MiB读回，不能继续旧B。

2026-10-03，基线 `agent/amp-platform-closure / af08db1`。

## 目标与已有证据

用户要求开始准备：在已实测 P028 U-Boot 上，形成独立的 Linux 保留内存检查、单次 M0 启动及 RPMsg echo 测试物料。P028 完整8MiB读回、proper策略0、原Linux启动、Ctrl+C/help/boot均PASS；help只证明命令注册。原件恢复实测PASS。实际MCU setter、有效mapping/reset/cache和RPMsg仍未验证，整体C。

## 范围与权限

- 主控负责打包、分阶段脚本、manifest、验收/回滚指南和最终审核。
- 用户此前明确授权的一个子代理仅独立只读审查源码流程、保护和观测缺口；新报告放忽略目录，不改主控文件，不接触板端。
- 本轮仅Host准备，不上传/运行板端程序、不写boot/eMMC/GPT、不重启、不接线、不启动M0。后续L3操作按AGENTS.md形成具体范围后批准。
- 保留固定reference、旧P026撤回物料、已实测P028镜像和默认factory启动文件；不为打包重新刷U-Boot或重设计地址。

## 一个里程碑，三项任务

1. 核P026配套kernel/modules/initrd/M0/FIT与P028编入contract的一致性、来源、哈希、地址边界；选择分阶段测试对象，识别缺少的运行输出。
2. 形成独立passive文件包、阶段A（不调用M0）与阶段B（明确单次启动）流程；默认启动入口保留，失败后完全断电回默认。生成机器manifest、明确安装范围及恢复对象，不使用旧withdrawn writer。
3. 主控审核子代理结论，运行必要Host检查/实际解析与fault测试，交付桌面物料和简明指南；更新STATUS/SUMMARY，commit/push既有Draft PR，不merge，不升级D。

## 验收与失败处理

包中每个可执行/启动对象有size/SHA/source，禁止混入旧U-Boot；实际Image/initrd/DT/FIT边界由脚本核算，Linux no-map与模块release匹配。阶段A无M0命令；阶段B只有一次显式入口，检查失败不继续boot旧DT或在同次启动重试。超时/异常停止，保留有限日志，恢复先完整断电；default factory文件不需修改则不增加恢复动作。

若发现源码/物料无法提供必要mapping/cache观测，先完成可独立审查的Host修补或记录具体阻断，不把setter返回值/帮助文本当作有效运行映射。所有现场结果保持UNVERIFIED；Host PASS不作为M0/D证据。

## 执行与交接

开始：工作区干净，最新af08db1；一个既有子代理已接只读审查任务。主控读取工作规则及P026/P028证据。本轮结果、实际命令和剩余门在结束前更新。


## 实际结果（2026-10-03）

三项Host任务完成；桌面最终v4交付21文件SHA逐项一致，无U-Boot镜像。fresh M0 v3 build零warning、ELF向量/LOAD边界、FIT外部payload、三份实际DT/no-map/geometry核验PASS。67包/脚本 +14实际M0 C +6snapshot故障项PASS；实际P028 C/libfdt与full CI PASS，最终/memory coverage仍需目标runtime fixup，不虚记板端PASS。独立子代理审查找出的非法指针free及安装器TOCTOU/根tar问题由主控修复，复审PASS。细节、准确命令/identity及限度见P029_HOST_PREPARATION_REVIEW和P029_PACKET_MANIFEST。

没有SSH/板端文件写入/MMIO/KO/M0/重启/接线。canonical final fields和D门保持未开放，旧P026撤回状态保留。下一步A的被动新增文件/冷启动需要AGENTS.md L3明确批准；用户唯一Debug USB-TTL足够A，B/C UART5日志采集仍待安排，不假设第二适配器。实际有效映射/cache/MCU setter/RPMsg与新回滚回归保持UNVERIFIED。

## 阶段A批准与执行（2026-10-03）

用户明确回复“批准阶段A”，对应已交付6524461/P029 v4：允许一次被动新增 `/boot/amp-p029`、独立paired模块目录及临时RAM包，并指导用户手工冷启动阶段A；M0/RPMsg保持关闭。不扩大到B/C、U-Boot写入或Agent自行重启。主控串行持有既有板端锁，先核设备/原件/空间和Host身份；执行已审查Host stdin安装器及读回。实际结果随后追加，失败不覆盖已有目录或自动重试。

本次实际：Host原包/桌面21文件SHA和stager再次一致。既有alias旧IP超时；单命令当前已知IP的WSL SSH及Windows原生TCP也超时，远端命令未执行。无板端上传/写入/M0/重启；请求Debug串口当前IPv4或运行状态。记录P029_STAGE_A_RESULT/EXECUTION；阶段A批准保留，实际暂存BLOCKED_NETWORK，冷启动UNVERIFIED，整体C不变。


## 阶段A暂存实际完成（2026-10-03）

用户随后确认同一入口恢复并“批准所有操作”，此授权覆盖本任务后续必要操作，技术验收仍A→B→C，无需重复审批。持锁重新核原Linux/P028/model/mount/空间，原六内容/链接及当前8MiB U-Boot与已实测候选一致。未压缩传输未完成，只有task RAM部分包、持久目录absent；核白名单清理后同v4 gzip一次修复成功。已审查Host stdin installer只调用一次，boot10/module268/3receipt独立读回PASS；factory前后内容/路径自身metadata逐字节相同，临时源及root快照前缀最终盘点不存在。原Linux保持运行，Agent未重启/启动M0/RPMsg/KO/读未知MMIO。captured raw输出约54KiB仅ignored；具体失败、bounds、hash和交接见P029_STAGE_A_RESULT/EXECUTION及COLD_BOOT_GUIDE。

A资产PASS，A冷启动/实际DT/iomem等待用户物理冷上电和串口日志；B/C尚未执行，UART5采集待落实。整体C，D关闭，不以安装成功充当新kernel或M0运行成功。

## 阶段A首次脚本执行失败及修复（2026-10-03）

用户实际load A=1892B，source在`STOP length Image`退出，随后`printenv filesize`给出`0x2930200`。vendor f8b4554 `env_set_hex`为`0x%lx`，旧生成器/stub误用bare hex，主控对此生成错误负责。失败在Image载入前，A无M0命令，新Linux尚未启动。保留v4/hash/实板失败，不删旧物料。

沿用用户本任务全部操作授权，主控修复A/B/C六个长度字符串并在每次size/load前检查清除旧filesize，保留严格字符串比较和原边界；不用十进制-eq。先Host actual vendor C及故障脚本检查，再子代理独立审核；若原Linux可达则仅追加`/boot/amp-p029/script-fix-v5`的新脚本/receipt，原v4十文件/三receipt、kernel/DT/FIT/模块及默认入口保留。主控持锁、有界上传/安装/读回/精确RAM清理；不重启或启动M0。当前A冷启动仍未验收，B/C保持待执行，整体C。

实际结果：用户boot回原Linux已SSH确认；新的小目录安装器一次exit0，旧10/新4独立SHA读回及精确RAM清理PASS。新A2368B，桌面NOW指南更新新路径；原v4/首次STOP保存。175实际vendor C、112fault、14M0C、6旧snapshot、8新FD/noargs与完整CIPASS，核心子代理复审PASS。A再次冷启动/actualDT/iomem仍待用户，B/C不跳过，整体C。

## v5阶段A启动后的根分区查找失败（2026-10-03）

v5已通过长度检查并启动新kernel，但HDMI报短PARTUUID不存在进入initramfs。主控和独立子代理确认固定DT合并覆盖原脚本root。用户完全断电默认启动恢复原Debian；SSH只读确认完整p3UUID、short blkid无结果/full为p3、factory六内容和P0288MiB/旧文件一致。主控承担测试包参数遗漏；增加checked env bootargs_ext root，保留kernel/initrd/DT/FIT/KO/默认和旧v5，先57实际vendor C/118fault/175formatter/8FD与full CI PASS，再独立审查与只新增root-fix-v6小目录。当前新A登录/no-map/iomem待验收，B/C不跳过，整体C。实际安装和证据随后追加。

实际：独立核心审查PASS后主控一次新installer exit0，三scr/manifest读回和精确RAM cleanup PASS；旧v5五文件、v4十文件/三receipt及factory六内容/链接/路径自身metadata保留。仅新root-fix-v6被动目录，A2525B，新冷启动仍UNVERIFIED。未reboot/M0/MMIO/KO/B/C。

## v6阶段A实际验收（2026-10-03）

用户本次日志确认cold boot、f8b4554/policy0、root-fix-v6 A2525B、paired Linux到Debian并登录，root p3/stageA。主控持board_lock一次25秒上限严格SSH只读执行Host stdin Python，核实际DT三准确no-map区/无reusable、sudo iomem全部System RAM区间无重叠、五个相关节点disabled、根挂载p3、无echo模块/RPMsg设备，exit0/stderr空。独立子代理Host复核保存日志/程序和iomem非重叠PASS，主控阅读并接受。原始日志仅ignored，public记录脱敏状态/hash；本次没有持久文件上传、MMIO、KO、M0或Agent重启。

A冷启动与Linux预留内存PASS，v4/v5失败保留；整体C，D关闭。下一步B需先解决release前UART5早期输出采集，现有唯一USB-TTL接Debug，现包无已审核单适配器流程；随后由用户另一次完整冷上电。B/C未执行，Linux no-map不证明M0有效映射/缓存/BL31 MCU setter，外设回归也不在A范围。完整Host CI经固定LF wrapper确认exit0，2 CTest/41 Python/5撤回writer检查PASS；首次外层exit变量引号失败保留记录，随后确认结果。diff --check PASS，public仅脱敏结论/hash，忽略目录不进Git。独立public diff审查PASS，主控接受并更正两处历史/当前措辞；按仓库流程commit/push既有Draft PR #5，不merge。

## RV双串口与阶段B执行准备（2026-10-03）

用户进一步提供RV-debugger-plus资料并报告已接好。官方v1.0手册确认主UART排针RX/TX默认3.3V且可提供虚拟COM，自身RX0/TX0只是BL702调试接口；本次仅RX+GND接板UART5 TX，不接TX/供电，现有Debug独立保留。A已PASS，沿已有全部任务操作授权。主控当前Windows只读Ports枚举只有原CH340及蓝牙串口，未发现新增RV COM；进一步只读筛选未发现已知FTDI/Sipeed USB或异常USB节点。只说明当前枚举快照，不能判断是否接到其他电脑、线缆或驱动原因。

本轮里程碑是B执行指南和捕获条件核对，不自动source/重启/启动M0。形成P029_STAGE_B_COLD_BOOT_GUIDE及桌面副本，要求release前两个窗口已经保存日志，再另一次完整冷上电/单次B脚本2738B；B预期15s link timeout，不加载KO/C。先等待用户确认新增COM/设备截图；设备未枚举时不盲装驱动、刷调试器固件，也不跳过采集。Host核脚本hash/单次调用/配套Linux/B_DT、独立子代理review后按仓库流程提交现有Draft PR。B动态结果仍UNVERIFIED，整体C/D关闭。

随后用户截图确认新增USB Serial Port COM6；主控只读再次枚举Status=OK、VID_0403/PID_6010，COM5 CH340原Debug保留，先前无RV枚举作为历史快照保留。B guide已改为COM6/115200与COM5/1500000，均8N1/无流控，COM6必须先打开保存再source。Host实际v6 B scr2738B及SHA匹配；脚本确有一次loader、checked root override在loader前、booti在成功分支、B DT与stage参数正确、无insmod。没有访问板端或启动M0；COM可见不等于接线电测或M0运行PASS。下一步由用户执行指南并返回两份有限串口日志。

本次Host CI exit0、diff --check PASS；独立子代理核实际B SCRIPT头/数据CRC、与cmd逐字匹配和B DT状态PASS，并发现预期timeout自带STOP会被旧文案误拒绝，主控精确修正该例外后复审PASS、阅读接受。指南已同字节复制桌面，machine记录P029_STAGE_B_PREPARATION.json；没有板端访问或M0启动。按流程commit/push既有Draft PR #5，等待用户双串口B实测日志。
