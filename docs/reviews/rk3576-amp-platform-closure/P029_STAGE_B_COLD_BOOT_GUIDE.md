> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 当前停止：v6 阶段B已在实际验签入口失败

2026-10-03用户一次B调用返回 `No RSA key found` / `ret=-13`，发生在固件reserve/copy/release之前。当前U-Boot内置control DT没有公钥；以下v6执行步骤仅留作历史，**不要再次执行B或C**。失败当次必须完整断电回默认Linux，用户已确认恢复到桌面。等待新可信control DT/签名FIT的Host校验、审核、精确部署和新Linux-only回归后再交付新的B流程。不能用COM6无输出判断UART接线故障；M0未成功启动。整体C，D关闭。

# 历史：P029 v6阶段B执行指南（已暂停）

2026-10-03。A冷启动与Linux预留内存已经PASS，沿用用户本任务操作授权。用户报告RV-debugger-plus已接线，随后截图显示新增COM6；主控Windows只读复核COM6为USB Serial Port、Status=OK、VID_0403/PID_6010，原CH340 COM5保留。**先完成第1节，未能打开UART5日志窗口时，不执行source。** 这份指南是准备物料，不是B实测成功记录。整体仍C，D关闭。

## 1. 先准备两个串口日志窗口

现有Debug USB-TTL保持原接线，1500000 / 8N1、无流控。本次Host枚举原CH340为COM5，以原MobaXterm会话实际配置为准，不更改既有Debug窗口。

RV-debugger-plus用Type-C数据线接电脑。在MobaXterm新建Serial会话，选择 **COM6、115200 / 8N1、无流控**，开始保存日志。原Debug窗口保持COM5。M0尚未运行时UART5无输出是正常情况；有COM并能打开不等于UART5接线已经电测通过。

只接收M0输出即可：

| RV 调试器排针 | LubanCat-3 v2 |
| --- | --- |
| RX | 40Pin第18脚，UART5 TX |
| GND | 开发板GND |
| TX、3V3、5V | 留空 |

使用排针RX，不能用RX0/TX0：后者是RV调试器自身BL702日志接口。功能/3.3V/RX与RX0区别依据 [Sipeed官方手册第6、9页](https://dl.sipeed.com/fileList/Accessories/RV-Debugger%20Plus/RV%20Debugger-Plus_user_guide_v1.0.pdf)。板端UART5管脚沿用已审核的LubanCat-3 v2硬件对应关系。现有Debug与RV保持为两个独立串口，不依赖中途换线。

若没有新增COM、不能打开或型号/引脚不符，停在本节，提供设备管理器或MobaXterm串口列表；先核USB枚举，不盲目安装驱动或刷RV固件。两个窗口均打开并保存日志后，才进行冷启动。

## 2. 正常关机，再完整冷上电

当前Linux可用时，在Linux执行：

```sh
sudo poweroff
```

确认关机完成，由用户完全断开所有会给开发板供电的连接；包括主电源、板上USB以及可能经串口信号反供电的连接。重新接回两个串口并打开保存日志，随后恢复开发板主供电。物理插拔由用户完成，不以warm reboot代替。本次不刷U-Boot或修改默认启动文件。

## 3. 只在Debug窗口输入两条命令

启动倒计时按Ctrl+C，进入 `=>`。确认proper U-Boot为 `f8b4554`、运行时policy=0。身份不同或日志出现abort/panic则停止。

先执行：

```text
load mmc 0:2 0x4c000000 /amp-p029/root-fix-v6/stage-B.scr
```

必须读到 **2738 bytes**；load失败、长度不同就停止，不执行source。该脚本SHA256：`bdd9284ca70be822be9ec980d907bc4675c0bfe0028e2d88a7384da4ec4a6d8e`；此前已安装并独立读回匹配。

确认UART5日志窗口已经开始保存后，仅执行一次：

```text
source 0x4c000000
```

脚本先加载配套Image/initrd/B DT并逐项检查长度，用root override保持p3，再调用一次 `amp_m0load /amp-p029/amp-host.itb 0x48300000`。成功后立即booti准备好的Linux。不要另外手动调用amp_m0load；不加载echo KO，不执行阶段C。

## 4. 保存这几项实际输出

Debug应保存完整loader结果、FIT检查/SMC输出、`P029 M0 loader returned success - immediately boot prepared Linux`及Linux启动。仅这条返回成功不能证明M0已执行，必须与UART5一起审查。

UART5应保存：

- `P029 M0 entered local_fn=...`：M0代码实际执行。
- `cache entry ctrl=... bypass=1`：M0本地cache状态快照。
- 等待信息，以及 `P029 STOP link timeout 15s; echo task exits, MCU stays running`。

**B的上述link timeout 15s是预期结果，即使该行带STOP字样**：B关闭Linux RPMsg transport，不做双向通信。超时只是echo任务退出，不代表M0已经停止/复位。仅允许这条预期超时STOP；其它STOP、bypass=0、abort、panic或没有捕获到关键输出均不能判B PASS。

保存两个串口日志，观察上限2分钟/每份2MiB；异常或日志风暴立即停本轮，不无限等待。Linux若进入登录，在Debug窗口执行：

```sh
uname -a
cat /proc/cmdline
tr -d '\000' </sys/firmware/devicetree/base/chosen/project,p029-stage
printf '\n'
```

预期release为 `6.1.99-rk3576-m0echo-p026`、root=p3、cmdline `amp_test_stage=B`、DT tag `B`。将Debug和UART5两份日志分别发给主控；主控审核后再决定C。没有UART5输出时同时保留Debug结果与COM/接线状态，不能据此断言M0未启动。

## 5. 失败后的动作

source以后若Debug返回 `=>`、脚本STOP或出现异常，**本次会话不再boot、不重跑source、不重启M0**，保留日志；UART5的上述预期link timeout STOP按第4节判读。正常Linux可用时正常关机；不可用时由用户完整断电。另一次完整冷上电可走未改的factory默认启动入口，核原release `6.1.99-rk3576`。M0测试后须完整冷断电，不能把warm reboot当恢复。

阶段B只验收单次加载、代码执行与cache快照；共享DDR的有效映射和Linux收到HELLO_ACK/PONG仍等待阶段C。CON16/17原始值与整个512MiB窗口不由本轮日志证明。见 [分阶段完整验收规则](P029_STAGED_TEST_GUIDE.md)、[A实板验收](P029_STAGE_A_RESULT.md)。
