# I2C_RESOURCE_PROBE_V1 — Host包及单次运行/恢复

2026-10-05。用户最新明确“进行下一步，且不需要审批”，本次有限诊断授权沿该消息继续；不改写旧审批记录。主控已审核并被动安装八文件，逐字hash读回PASS；尚未冷进入、加载KO或执行诊断。构建JSON中的board_deployed=false是构建当时事实，不改写。

目的只验证BUS_MCU实际定向读取I2C9及前置资源状态。MPU信号线仍不接开发板，无WHO_AM_I、Setup/Transfer/START、寄存器写、INTMUX gate使能或reset deassert。完整传感器阶段均未通过。这里0写/0事务只指新增诊断动作；原AMP/tick/UART/IRQ初始化和共享传输MMIO行为保留，不声称整个固件没有MMIO写。

## 精确包与来源

Host输出 `artifacts/local/i2c-resource-probe-host-v2/packet`，目标 `/boot/amp-p029/i2c-resource-probe-v1/` 原先不存在；现已由主控安装。配套冻结kernel6.1.99-rk3576-m0echo-p026、原Image/initrd复用；DT只增加之前审查过的I2C9 Linux-held clock/pin，保持i2c9 disabled，不改CON16/17、mailbox/link4/vrings/load/entry。原最小AMP及SI_HEALTH源码、BIN/FIT/KO保持，构建后逐hash检查。

| 文件 | bytes | SHA256 |
| --- | ---: | --- |
| RESOURCE_PROBE.json | 9872 | `c649ab8c561c228be2fc58f71fc7b2e08ad1f190276eb7fc73b3ce7c784dc653` |
| SHA256SUMS | 623 | `945d57f8b2582da61923fe3c732bb56631f087a6abe981da5d4086c6b33d42b0` |
| amp-signed.itb | 132096 | `934754d95d967f773ad4b52bcd438f3077b6692e71cb1b44e2da17117659d180` |
| preflight-resource-probe.py | 12990 | `e781942a834e107741c16490ba5e8a162f6dedaffba0e9c6fccde9612ac2eb5d` |
| resource-probe.dtb | 308361 | `33dc67a8a0199d4d0b313e980657982297155f364509552ecc3a92d650c3064e` |
| rk3576_i2c_resource_probe.ko | 107704 | `ad4c6f57c5f43c07ef73c816d70324b01f819b5a58bed1f86504431f8f1115a4` |
| stage-resource-probe.cmd | 3152 | `70bb849249f9de2b959d298619009688bf3c82b33d04db77fab5075c7833bfd0` |
| stage-resource-probe.scr | 3224 | `0bd62840dd69edc5b50648d047a2501682c125fe28fdc951f193b5ee683ae98c` |

安装器单独生成 `artifacts/local/i2c-resource-probe-host-v2/install-resource-probe.py`：5844B、SHA256 `43d7dd13bf2b1e4674b14c2b642c2ef624a2b37f9b5ac15602b656c2afc5a0d8`。此固化版本用manifest c649ab8c…为pin；仓内模板不可直接替代它。当前八成员校验及错hash/缺多文件/符号链接/超限负例8/8通过。

完整来源/构建输入/产物/保护文件hash见 [JSON](RESOURCE_PROBE_HOST_PACKAGE.json)。基于实际SI_HEALTH persistent-host-v2/source，RTOS3a39b0f+0010..0015/v5、HALbc99978，保留tick/cache检查，仅独立诊断应用与配置改变。I2C框架unset，nm未发现HAL_I2C/rt_i2c/resource_ready调用符号。现有固定mkimage签名器只使用原key目录，未读取/复制/打印私钥；原controlDT验签通过，不生成新key、不改信任链。

复现（新输出目录，不覆盖v2）：

```sh
python3 scripts/dev/build_i2c_resource_probe.py --output artifacts/local/i2c-resource-probe-host-review-copy
```

原生SCons `--useconfig=.config`与build、paired Kbuild、签名/验签退出0，无新增warning/error。text124324B/data2464B；size的bss536704B包括实际.bss11396、heap385020、stack1024、既有共享atags8192及Linux共享区131072，不全是RAM静态数据。代码/data/bss终点0x21c04；heap0x21c04..0x7fc00（385020B），main stack0x7fc00..0x80000（1024B）；LOAD段不越0x80000，原共享NOLOAD地址/大小不变。未证明实际heap/stack高水位。

## 定向读白名单与门

固定TRM Part1 V1.2/20240624（原hash6094ae58…），下列页码及HAL RK3576定义，M0地址为CPU外设地址+0x20000000。所有32位读取一次，有READ_BEGIN/随后VALUE日志；无read-clear/FIFO/IPD/安全firewall遍历。CRU高16位为WO write-enable，不作为判据，只看对应低位。

| M0地址 | 含义/判据 | 固定依据 |
| --- | --- | --- |
| 0x47200830 | CRU_GATE_CON12 bit8 PCLK_I2C9，0开启 | TRMp119；HAL gate0xc8 |
| 0x47200834 | CRU_GATE_CON13 bit4 CLK_I2C9，0开启 | TRMp120；HAL gate0xd4 |
| 0x472003e8 | CLKSEL_CON58 [1:0]=3代表xin24m；2是50MHz | TRMp79；HAL CLK_I2C9_SEL_XIN_OSC0_FUNC=3 |
| 0x47200a30 | SOFTRST_CON12 bit8，0=I2C9 preset解除 | TRMp158；SRST_PRESETN_I2C9=0xc8 |
| 0x47200a34 | SOFTRST_CON13 bit4，0=I2C9 reset解除 | TRMp158；SRST_RESETN_I2C9=0xd4 |
| 0x4ae80000 | I2C9 CON，31:16 RO版本=6；EN/START/STOP位0/3/4=0 | TRMp1109–1110，RKI2C_CON；HAL CON offset0 |
| 0x4ae80004 | I2C9 CLKDIV，上下16位RW、无读清；拒绝全1异常 | TRMp1110，RKI2C_CLKDIV；HAL offset4 |
| 0x4720082c | INTMUX2BUS gate bit12=0 | TRMp118；PCLK_INTMUX2BUS_GATE=0xbc |
| 0x47200a2c | INTMUX2BUS preset bit12=0 | TRMp157；对应HAL reset0xbc |
| 0x4aec0008 | INTMUX_ENABLE_GROUP2，普通RW、只读记录 | TRMp758；RK3576 BUS_MCU INT_ENABLE_GROUP[2] |

前5读的clock/parent/reset不满足：status1，**完全不触I2C9**。CON/CLKDIV异常：status2（共7读）；INTMUX gate/reset不满足：status3（共9读），**不读INTMUX**，不偷偷使能/解除。status0才完成10读。STOP表示状态或读值不满足，不能一律解释“firewall拒绝”；HardFault/缺END则是未完成，停止不retry。即使status0，也只支持这一白名单读访问，不证明写权限、I2C交易、IRQ投递或MPU业务。

实际production header的白名单、10个失败门、mux2拒绝、一次性claim、回复deadline前/正好/迟到竞争及STOP锁存经ASan/UBSan通过。M0首条诊断请求无资格也锁存，不能后续retry恢复。Linux只在mcu-amp driver确为rockchip-amp且7clock布局、CCF额外I2C9引用/parent xin24m/rate24MHz/enable实际检查通过后开始HELLO；拿rate exclusive引用，不修改parent/rate。首HELLO_ACK+PONG后才发17字节RESOURCE_PROBE_V1一次；M0仅同peer且握手/PING已发生才执行。22字节DONE/STOP返回不替代UART逐寄存器证据。

## 时间、日志、停止及退出

M0原link等待15秒保留；120秒接收窗从link-up/NS endpoint建立后开始，最多128请求，过窗结束该echo task、不重启M0。Linux90秒health窗从RPMsg probe取得owner引用后开始；probe回复deadline3秒，失败/超时停止worker不继续PING、不再次发probe，保留CCF引用等待主控冷恢复。正常runner最多25秒等DONE及至少3个PONG，随后一次正常rmmod、cancel/join工作，释放仅额外CCF引用；mcu-amp原持有仍在，M0回到无事务的有界接收等待。

执行总诊断观察窗120秒（SOURCE后），每路UART≤256KiB，普通日志≤256KiB；达到上限/HardFault/health丢失/错误读值/无END即停止增加动作。外部timeout只兜底，不能解除总线hang/HardFault；失败不强行rmmod/reset/重新insmod/M0重试。主控正常关机或无响应时用户物理切电，明确记录强制断电未取得clean-exit。

## 单次实板步骤（主控持board_lock）

1. 最新default L1保护清单25个默认/冻结/SI文件已采集并固化；根日志 `artifacts/local/hardware-review/resource-probe-root-before.log`。安装已完成日志resource-probe-root-install.log。部署前仍核默认6.1.99/rootp3boot2、RPMsg空、无项目进程；新增目录不参与默认boot。
2. 先启动下述双UART脚本并核UART_CAPTURE_READY。两端实际打开且记录就绪后，主控才执行 `bash scripts/board/mpu6050/resource_probe_shutdown.sh`；确认正常停机后用户完整断电再上电（风扇/模块/串口接线不动）。不能用warm reboot或M0强制reset替代。
3. 双UART：COM5 1500000 8N1（固定U-Boot .config CONFIG_BAUDRATE），COM6 115200 8N1（原RTOS配置），no flow，DTR/RTS=false。用户及root已确认COM6重新枚举。脚本 `scripts/board/mpu6050/capture_resource_dual_uart.ps1` 用Windows原生System.IO.Ports同时各打开一次，拒绝其他程序占用，不kill用户进程；人工动作等待阶段≤300秒且未加载新固件，首次实际观察soc cold boot才启动coldstartup≤120秒；SOURCE后诊断阶段≤120秒，绝对总≤540秒、日志总各≤256KiB；必须先看到UART_CAPTURE_READY才开始停机/人工coldcycle。
4. 第2步前root已启动该脚本，使用新的LinuxLog/M0Log及空ControlFile，显式-InterruptColdBoot；只在实际观察完全匹配`Hit key to stop autoboot('CTRL+C'):`提示时发送一次Ctrl+C（字节0x03）；不是ESC。核soc cold boot/proper149b1c5、SPL/required controlDT等原身份，且实际确认已经停在U-Boot命令提示符，才向control文件追加LOAD。脚本仅发送以下固定load：

```text
load mmc 0:2 0x4c000000 /amp-p029/i2c-resource-probe-v1/stage-resource-probe.scr
printenv fileaddr filesize
```

第二行由root另追加INSPECT后发送；核3224B/0xc98、fileaddr4c000000、filesizec98及load成功后root追加SOURCE，脚本只发一次`source 0x4c000000`。它不直接发amp_m0load，不自行retry。脚本固定校验最新env值；root仍须实际核冷启动身份和完整输出。ControlFile不可预填LOAD/INSPECT/SOURCE让它盲跑；每步确认对应返回后才追加。脚本源码须root审查，未打开实物端口运行。
5. 新Linux起来后主控持锁执行（existing sudo-n，不改策略、不强制KO）：

```sh
sudo -n timeout 65s python3 /home/cat/cockpit/i2c-resource-probe-host-v2/run_resource_probe.py
```

runner先只读preflight核paired release/marker、原ring/DMA/NS/资源DT/无第二owner、八文件相关hash与25保护文件；随后单次insmod。保存Linux status及COM6的BEGIN→逐项READ_BEGIN/VALUE→END，按次数/顺序/地址、状态和随后PONG关联。没有成功preflight不得手动绕过加载；没有完整M0记录不得标权限PASS。
6. 保存结果后按同一shutdown脚本正常停机，用户完整冷断电上电走默认入口，不saveenv。新目录被动保留，不删除或改默认配置。

## 完整撤回核验与记录

恢复后主控先用最新read_resource_boot_baseline.sh采集（含明确RPMSG_MODULES/PROJECT_OCCUPANTS标记），然后运行：

```sh
bash scripts/board/mpu6050/read_resource_boot_baseline.sh > artifacts/local/hardware-review/resource-probe-root-after.log
python3 scripts/board/mpu6050/verify_resource_cold_recovery.py artifacts/local/hardware-review/resource-probe-root-before.log artifacts/local/hardware-review/resource-probe-root-after.log
```

要求默认6.1.99/rootp3boot2、无stage/health/probe marker、RPMsg空；25个默认/冻结/SI路径、大小、hash同前；无probe KO及项目负载，设备释放。新增probe目录不算需删除的基线文件，不影响默认入口。记录UTC、安装/启动前后身份、SOURCE次数、真实READ序列/END、Linuxhello/ping/pong/timeout/error、卸载结果、正常或强制停机、用户cold动作、25hash对比及恢复结果。任何一项缺失标PENDING/FAILED，不写PASS。

主控Host独立复核v2 production sources/八文件hash/验签、actual headers sanitizer；host_ci31/31 CTest、67/67 Python、5/5撤回通过，日志resource-probe-root-host-ci.log。当前等级仍Host诊断包完成/被动安装完成，实际权限诊断与恢复尚未执行；不是MPU_SENSOR_V1或MPU6050_RTOS_RPMSG_INTEGRATION_PASS。

主控指出的runner路径/采集先于shutdown顺序/恢复环境门已修正；恢复validator另5项回归（root/boot、残留模块/进程/marker及成功）通过，bash语法与PowerShell解析通过，未改变v2固件/KO/八文件。

## 串口占用尝试与关闭错误路径

主控首个双UART采集尝试因COM5被串口软件占用而AccessDenied；未看到READY，未关机、未加载KO、未启动新固件或执行定向读。UNC脚本先被既有RemoteSigned拒绝，主控仅复制同hash脚本到本地workspace执行，未改执行策略。空COM5日志保留；用户已确认断开串口软件连接，后续采集使用新日志名。

原PowerShell非终止错误可能让tools显示exit0，现强制ErrorActionPreference=Stop、catch记录UART_CAPTURE_STOP并非零退出、finally关闭已建立对象；两端Open全部成功后才发READY，WriteTimeout1000ms。实际生产脚本仅替换串口factory的Host fixture：COM5拒绝、COM6拒绝均exit1/无READY且finally Dispose；正常两端均Open时exit0/READY/Dispose。测试没有打开真实串口；v2生产固件/KO/八文件/runner保持不变。本仓只保留统一双口脚本，单口初稿在前次提交前已移除。

## 人工窗口修正及实际默认恢复（最新）

用户回复已上电后，主控重新只读盘点并用实际恢复validator核验通过：默认6.1.99-rk3576/rootp3boot2、无诊断marker、RPMsg空/无probe KO/无项目进程、25个默认/冻结/SI文件大小和hash与关机前一致。日志resource-probe-root-after-attempt2.log。attempt2仍未进入新AMP/执行诊断，恢复默认PASS只在此范围成立，不升级权限读/MPU业务。原先关机等待记录保留为历史，当前已恢复运行默认系统。

本次暴露collector从打开端口就起算120秒，可能在人类完成断电上电前自然结束。已修为人工等待≤300秒（不加载新固件），**首次实际看到soc cold boot**才开始coldstartup≤120秒（第一次marker只重置一次，重复marker不延长）；root核身份/长度后一次SOURCE才开始diag≤120秒。绝对stopwatch总≤540秒、每路总≤256KiB，无自动retry/coldreset。M0原120秒和Linux90秒窗口不改变，生产v2八文件及runner不改变。

实际生产PS脚本仅替换serial/clock factory的六模式Host回归通过：两路AccessDenied fail-closed；未cold人工等待过期；重复cold只一次Restart；一次SOURCE进入诊断（共两次Restart）；绝对540秒硬边界。fixture确认实际分阶段while仅匹配一次，保留生产循环，没有再次真实等待300秒或打开物理串口。最新主控host_ci31/31 CTest、72/72 Python、5/5撤回通过，日志resource-probe-root-host-ci-final.log。

## attempt3真实autoboot提示修正

主控attempt3捕获真实soc cold boot/SPL7d8fe670/controlDT/proper149b1c5/policy0；但COM5实际提示是`Hit key to stop autoboot('CTRL+C'): 3 ...`，旧脚本误用另一提示和ESC，未打断而进入默认系统。没有LOAD/INSPECT/SOURCE、新M0/FIT/KO、权限MMIO或I2C事务；该次默认恢复核验由主控另记，不据这些准备日志写诊断PASS。

现严格大小写匹配该固定提示文本（regex Escape，不泛化其他板提示），只发一次0x03；发送事件不能替代主控看到真正U-Boot提示符/身份后才允许LOAD。增加明确CANCEL token：只结束采集并finally释放两端口，不向板发送任何命令、不关机/复位/恢复，不需要借未知token触发错误退出。

实际生产脚本serial/clock factory八模式通过：原六项deadline/错误关闭保留；当前真实提示及重复提示只一次Ctrl+C、旧/错误提示零发送、CANCEL正常退出且释放端口。v2固件/KO/runner/八文件不变，尚未执行诊断。

冻结loader大小检查已独立复核：`/home/ywx/rk3576-work/worktrees/rk3576-amp-platform/project/artifacts/local/p030-uboot-final-fdt/drivers/cpu/rockchip_amp.c` SHA256 `d0f5f6f3f37e30f07531c2e6ac2970d8af0e6a4ee089513642e462b87c25cfb9`。实际`amp_m0_file`第950行及`amp_boot_fit`第293–297行限制FIT总长≤`AMP_CODE_SIZE+SZ_64K=0x90000`，第326–330行限制非空image data≤`AMP_CODE_SIZE=0x80000`且处于文件边界。v2 FIT总长`0x20400`符合总长上限；不能把旧FIT的`0x20000`大小误当loader上限。既有ELF/load段内存预算与image大小检查保留，不修改bootloader。
