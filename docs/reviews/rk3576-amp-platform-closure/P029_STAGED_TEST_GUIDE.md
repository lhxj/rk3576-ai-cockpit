> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

> 2026-10-04：v5 B与冷恢复已通过，新C已暂存读回；当前操作见 [P030新版C指南](P030_RPMSG_C_V1_GUIDE.md)。下文v6入口继续暂停，仅保留历史。

# 当前停止：v6 B/C入口验签失败

2026-10-03实际阶段B返回缺RSA key / `ret=-13`，在复制/启动前退出。以下分阶段说明保留历史；A已实测通过，**B/C执行暂停，不重跑旧脚本**。修复选择trusted内置control DT的单required=conf公钥及签名AMP config，全部检查保留，须先完成新包Host负例/审核及新U-Boot读回、原Linux冷启动回归。当前C/D关闭。

# 历史：B失败前的分阶段测试指南

以下为B失败前的准备快照，执行入口已经暂停。2026-10-03当时v6阶段A冷启动与实际Linux内存预留PASS，root=p3/stage=A；主控SSH只读核验通过。当时B/C尚未执行，整体 C. HOST_BUILD_PASS。见 [A实际验收](P029_STAGE_A_RESULT.md) 和 [当前签名修复](P029_SIGNED_REPAIR.md)。

## 1. 这次要验证什么

| 阶段 | 操作与验收 | M0 / Linux RPMsg |
| --- | --- | --- |
| A | 新配套Linux、模块/initrd与独立DT正常启动；三个no-map保留区真实存在 | 均关闭；mcu-amp与mbox0/4也disabled |
| B | 新冷上电后一次显式M0加载，观察代码执行、M0本地CACHE_CTRL bypass及15s等待超时 | M0启动；Linux RPMsg/mbox关闭 |
| C | 另一次冷上电，启用transport，手工绑定echo KO；Linux实际收到HELLO_ACK和PONG | M0启动；Linux RPMsg开启 |

当时P028 U-Boot `f8b4554`的完整8MiB读回与原Linux启动/CLI/help/boot已PASS，该历史测试包不含U-Boot镜像。实际验签失败后的新control DT修复须重新封装和写入uboot，见当前签名修复文档。

用户已明确批准本任务全部必要操作，沿用已有授权，不重复审批；技术验收仍A→B→C，A通过并落实UART5采集后再执行B，B通过再执行C。仅出现help不算M0加载成功，setter返回成功也不算M0已经执行。

## 2. 文件和安装范围

当前补充目录 `/boot/amp-p029/root-fix-v6` 只含三新脚本与manifest/SHA；A/B/C长度分别2525/2738/2774B。每条load成功且长度正确后才执行source；旧10文件/3receipt保留。

桌面交付目录：`C:\Users\27432\Desktop\RK3576-AMP-P029-StagedTest-PendingApproval`。

包内`MANIFEST.json`记录所有输入来源、大小、SHA及实际Image头计算的加载/重定位范围；`SHA256SUMS`覆盖全部交付文件。Linux release固定`6.1.99-rk3576-m0echo-p026`，Image、255模块及initrd来自同一次已审查构建；echo KO也来自该配套构建。

A首次暂存时新增（已完成）：

- `/boot/amp-p029/`：Image/initrd、A/B/C DT与脚本、诊断FIT、echo KO。
- `/lib/modules/6.1.99-rk3576-m0echo-p026/`：预先depmod的配套模块；不自动绑定echo。
- `/dev/shm/amp-p029/`：临时传输包，关机清除。

**保留原 `/boot/boot.scr`、`uEnv`、Image/initrd/DT默认链接和内容。** A/B/C仅Ctrl+C后显式运行，正常自动启动仍回原Linux。CAM0官方overlay离线合并到三份测试DT；没有借此宣称相机回归。

installer在任何持久写入前要求原Linux/P028身份、boot/root挂载设备、原六文件hash、包hash、新目录不存在、至少包大小+10MiB余量。主控从Host标准输入送入已审查脚本和可信checksum-list SHA；root创建0700私有RAM快照，固定hash核验后仅用快照，不从用户可改目录直接写入。拒绝不安全tar成员/链接/重复；新目标用原子mkdir，模块只解到核实的独立release子目录，兼容合法 `/lib -> /usr/lib`。不删除用户数据腾空间；检查失败停止。新目录已有内容时不覆盖、不重复安装。失败留下的是passive文件，默认启动不变，先检查部分安装；三份隐藏receipt留在新boot目录便于精确清理。

## 3. 阶段A（已批准并实测PASS；以下为使用过的步骤）

不重复安装历史整包。已审查root-fix-v6仅追加三脚本和manifest/SHA，读回与RAM清理PASS。当时由用户按以下步骤进行A冷启动，现已通过，无需重复；B/C仍等技术验收和UART5采集。不要运行旧v4或v5脚本。

随后用户正常关机、确认完成，完全断开主供电与供电USB再冷上电。Debug串口保持1500000 / 8N1，倒计时按Ctrl+C。先确认policy=0及CLI，再逐条执行；load失败不得source：

```text
load mmc 0:2 0x4c000000 /amp-p029/root-fix-v6/stage-A.scr
source 0x4c000000
```

脚本实际按名解析boot分区并要求编号2，先size核每个文件、有限长度load再核filesize，再用checked environment bootargs_ext保留root=/dev/mmcblk0p3，然后booti；不调用M0。脚本用PPC/Linux/SCRIPT/none封装，匹配P028受控factory SCRIPT语义，header/data CRC由U-Boot检查。file SHA由安装器在Linux核对；CRC/size不冒充密码学认证，未来policy非0则停止。

登录后，留存有限Debug日志并执行只读检查：

```sh
uname -a
cat /proc/cmdline
tr -d '\000' </sys/firmware/devicetree/base/chosen/project,p029-stage
sudo -n cat /proc/iomem
```

需要实际release与`amp_test_stage=A`/DT tag=A、三个保留区reg/no-map一致且均不属于System RAM可用分配区；mcu-amp/rpmsg/mbox0/4真实status=disabled、无RPMsg设备。不要用`/proc/iomem`单独推定no-map，需同时读取实际DT属性。Linux登录和这几项PASS后停止A，审查日志再推进。

下面只读取Linux导出的DT文件和设备目录，不访问MMIO；每项断言失败即停止本阶段：

```sh
python3 - <<'PY'
from pathlib import Path
import struct, subprocess
dt = Path('/sys/firmware/devicetree/base')
assert subprocess.check_output(['uname', '-r'], text=True).strip() == '6.1.99-rk3576-m0echo-p026'
assert 'amp_test_stage=A' in Path('/proc/cmdline').read_text().split()
assert (dt/'chosen/project,p029-stage').read_bytes() == b'A\0'
reserved = dt/'reserved-memory'
assert (reserved/'#address-cells').read_bytes() == struct.pack('>I', 2)
assert (reserved/'#size-cells').read_bytes() == struct.pack('>I', 2)
for name, base, size in [('mcu@47800000', 0x47800000, 0x80000),
                         ('rpmsg@47d00000', 0x47d00000, 0x10000),
                         ('rpmsg-dma@47d10000', 0x47d10000, 0x10000)]:
    node = reserved/name
    assert (node/'reg').read_bytes() == struct.pack('>4I', 0, base, 0, size)
    assert (node/'no-map').is_file() and (node/'no-map').read_bytes() == b''
    assert not (node/'reusable').exists()
    print(name, hex(base), hex(size), 'no-map verified')
for name in ['mcu-amp', 'rpmsg@47d00000', 'mailbox@2ae50000', 'mailbox@2ae54000']:
    assert (dt/name/'status').read_bytes() == b'disabled\0'
    print(name, 'disabled')
assert not list(Path('/sys/bus/rpmsg/devices').glob('*'))
print('P029_STAGE_A_DT_IDENTITY_PASS; review iomem and boot log separately')
PY
```

普通用户读取`/proc/iomem`可能只看到零地址，所以该项用`sudo -n`；拒绝时停止，不修改sudo权限。以上检查通过也不能单独证明Linux全部外设回归或M0动态能力。

## 4. 阶段B（沿用本任务授权；A已通过，待落实UART5采集）

Debug用来保存loader/booti过程；UART5用来保存M0输出：115200 / 8N1、无流控。最新用户报告RV-debugger-plus已接线，可提供第二个3.3V虚拟串口；仅需40Pin18=板TX→RV排针RX及共地，RV TX/VCC留空，不能用其自身RX0/TX0。**用户截图和Host只读复核新增RV虚拟串口COM6、Status=OK；原Debug CH340为COM5。先打开COM6并开始保存日志，再执行source。** COM枚举不证明UART5接线或M0输出；不能把没有日志当固件没有运行。见 [阶段B当前操作指南](P029_STAGE_B_COLD_BOOT_GUIDE.md)。

新冷上电，在CLI逐条运行B脚本：

```text
load mmc 0:2 0x4c000000 /amp-p029/root-fix-v6/stage-B.scr
source 0x4c000000
```

脚本加载所有Linux输入后，**只调用一次**`amp_m0load /amp-p029/amp-host.itb 0x48300000`，成功立即booti同一已准备DT；不让人停在CLI等日志。任何错误都停止，禁止同次启动`boot`到原DT或再次load M0。

B DT只启用MCU clocks/UART5 pin lease；没有amp-cpus子节点，不触发Linux启动M0。RPMsg/mbox保持disabled。M0应出现`P029 M0 entered local_fn=...`、`cache entry ctrl=... bypass=1`，每5s等待信息，最后`P029 STOP link timeout 15s; echo task exits, MCU stays running`。这一条带STOP的超时是B预期结果；其它STOP或异常不能PASS。echo任务退出，MCU仍运行；不是停止/复位M0。Linux可同时启动，但没有Linux共享transport。

`CACHE_CTRL@M0-local 0x43810000`是TRM §8.3.3的普通RW配置寄存器（system view 0x23810000、bit6=bypass），厂商SystemInit/HAL有实际RMW；仅M0自身读取它。不读SYS_SGRF CON16/17，不尝试Linux/U-Boot MMIO。bypass缺失时在remote_init前STOP。

## 5. 阶段C（沿用本任务授权；等待B验收）

再一次完整冷上电，用C脚本，命令形式相同，文件换`/amp-p029/root-fix-v6/stage-C.scr`。仍然一次loader后立即booti。C启用mbox/RPMsg及Linux专用uncached pool；动态debug bootarg只打开`virtio_rpmsg_bus.c`的实际DMA backing输出。

M0等待link最多15s；linkup/NS以后echo窗口最多180s且最多4条请求。原TTY/GUI登录耗时计入180s；超时就停本轮，不能warm重启固件。

登录后第一步只读核release=C DT，然后在获批C窗口内手工加载唯一配套KO：

```sh
uname -r
cat /proc/cmdline
sudo -n insmod /boot/amp-p029/rk3576_amp_echo_test.ko
```

原release时拒绝加载此KO。记录上限3分钟/2MiB双串口与相关dmesg；内核D状态/中断风暴时timeout不能保证终止，异常不重试、不清dmesg、不warm启用其他驱动。

验收需要全部出现：

- Linux实际rings PA `0x47d00000 / 0x47d08000`，实际DMA backing位于`[0x47d10000,0x47d20000)`。
- M0实际收到共享payload指针位于`[0x27d10010,0x27d20000)`，打印的`pa_proposal`只是contract转换，必须与Linux backing及真实通信一起看。
- Linux echo driver实际收到`HELLO_ACK`和`PONG`；M0的`PONG sent`不能替代Linux收到。
- M0 entry/link/after-pong snapshots均bypass=1；任何STOP或异常不判PASS。

诊断队列领取正常buffer后每条只归还一次。坏指针不调用会再次解引用header的free路径，停止、保留到冷断电回收。PONG后任务清理本地endpoint/queue/instance；Linux设备可能仍在，不把stale device当可持续服务。无热重启/长稳/缓存开启模式验证。

## 6. 地址证据的限度

布局未重设计：code PA`0x47800000`、code extent`0x80000`；拟B17`0x40000000`；shared M0`0x27d00000`→PA`0x47d00000`，ring1→`0x47d08000`，pool→`0x47d10000`。这些是拟配置值，当前CON原值仍未知。

代码执行+实际rings/pool backing+双向通信能够提供**已访问子范围的有效映射功能证据**；不是CON16/17原始读回，也不是整个512MiB窗口证明。cache snapshots证明该时刻bypass位；小消息通过不是硬件coherent或所有负载长期一致性证明。loader/hash/SMC返回、M0执行、Linux收到、代码/共享路径与cache state分别记证据。

## 7. 停止与回滚

任何M0调用以后，只能继续同一准备好的Linux DT；失败不`boot`/重试。Linux可用时正常关机，确认完成再完全断开所有供电；不可用时用户切断供电。不要仅warm reboot。恢复供电后不按Ctrl+C，走未改的原factory入口；核原release和六文件hash，M0没有自动加载路径。

passive测试文件后续在本任务既有授权范围内精确清理，boot测试目录包含后来追加的v5/v6子目录；清理前核目录身份、不是软链接且没有未知文件，不删除原modules/用户数据。默认文件没变不需要恢复它们。若发现原件不同，停止并用已有Host原件精确恢复；不贸然整机刷写。

原U-Boot8MiB（若以后另外发生bootloader损坏才需要）：`C:\Users\27432\Desktop\RK3576-Recovery-Tools\P026-Originals-20261003\uboot-original-8MiB.raw`，SHA`ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`；已有MASKROM RAM Loader→只uboot恢复和完整读回→Debian登录PASS。不是本轮写入对象。整机update会影响用户数据，本方案不需要整机刷写。

## 8. 当前裁决

P028原Linux兼容、P029被动资产读回、v6阶段A配套Linux冷启动与实际no-map/System RAM边界已PASS。本任务操作已获用户授权，B/C仍按技术门执行；B先落实UART5早期输出采集。M0/FIT加载、BL31 MCU setters、有效代码/共享映射与cache snapshots、RPMsg以及M0测试后的冷断电回原Linux仍等对应实测。只有审查这些证据后才能裁决D；当前保持C，canonical final字段与部署门不变。
