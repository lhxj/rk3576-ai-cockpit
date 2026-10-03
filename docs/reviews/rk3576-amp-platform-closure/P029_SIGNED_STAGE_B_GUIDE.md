# P029 SignedFix-v8：一次冷启动阶段B

2026-10-04。新U-Boot完整8MiB读回、新control DT实际冷启动、默认原Linux和实时基线已PASS。新B目录安装及独立读回已PASS，本指南只适用于下列新路径。 本轮只验证B，整体仍C，D关闭；旧root-fix-v6 B/C不再执行。用户此前本任务全部操作授权保留。

## 1. 先打开并保存两路串口日志

本次Windows只读枚举COM5和COM6均OK，用户确认COM6恢复。MobaXterm使用：

| 窗口 | 设置 | 用途 |
| --- | --- | --- |
| COM5 | 1500000、8N1、无流控 | 原Debug，输入U-Boot命令，记录启动 |
| COM6 | 115200、8N1、无流控 | UART5，只接收M0输出 |

RV排针RX接板40Pin第18脚UART5 TX，GND接板GND；TX/3V3/5V留空。使用RX排针而非RX0/TX0。保持此前接线；UART5未启动时没有输出本身不说明故障。两窗口均能打开、已开始保存日志，才进入下一步。

## 2. 关机并完整冷上电

Linux中执行 `sudo poweroff`，等关机结束，由用户断开所有给板供电或可能反供电的连接。恢复两个串口连接并打开保存日志，再给板主供电；不用warm reboot代替。本次不刷U-Boot。

COM5倒计时按Ctrl+C进入 `=>`。保存DDR/SPL到此处的完整日志，确认：

- `soc cold boot`。
- proper U-Boot `f8b4554`，`Checking fdt`哈希前缀 `43164981ef`。
- `PROJECT: factory boot/CLI allowed by runtime policy=0`。

身份不同、abort或panic时停止，不执行以下脚本。

## 3. 在COM5只执行新路径

先加载：

```text
load mmc 0:2 0x4c000000 /amp-p029/signature-fix-v8/stage-B.scr
```

必须读到 **3065 bytes**。加载失败或长度不同就停止，不执行source。脚本SHA256：`96921bd588d79d1d4a502d8bff8c096a95c8915751f65f499f35a6998964da0d`。

确认COM6正在保存日志后，仅执行一次：

```text
source 0x4c000000
```

新脚本保留v6配套Image/initrd/B DT及root p3 override，检查固定签名FIT长度130560B（0x1fe00），只调用一次新FIT路径的amp_m0load，成功后立即booti准备好的Linux。不要另行调用amp_m0load，不加载echo模块，不执行C。

## 4. 保存并提交实际结果

COM5保存FIT验签、loader/SMC结果，以及成功返回后配套Linux启动。**loader返回成功还不能证明M0代码已经运行。** COM6应保存：

- `P029 M0 entered local_fn=...`。
- `cache entry ctrl=... bypass=1`。
- 等待信息及 `P029 STOP link timeout 15s; echo task exits, MCU stays running`。

B关闭Linux RPMsg transport，**上述15秒link timeout是预期结果**。echo任务退出并不代表MCU停止。其它STOP、bypass=0、abort、panic或关键UART5输出缺失均不能判B PASS。观察上限2分钟，每份日志上限2MiB；异常或日志风暴停止本轮并保留日志。

Linux登录后在COM5执行：

```sh
uname -a
cat /proc/cmdline
tr -d '\000' </sys/firmware/devicetree/base/chosen/project,p029-stage
printf '\n'
```

预期kernel为 `6.1.99-rk3576-m0echo-p026`、root=/dev/mmcblk0p3、cmdline含 `amp_test_stage=B`、DT tag为B。把COM5和COM6日志分别发回，审核后才决定C。即使COM6无输出，也保存COM5全部结果，不据此推断M0是否启动。

## 5. 失败及测试后恢复

source后若回到 `=>` 或出现非预期STOP/异常，本次会话不再boot、不重跑source、不重试M0。保留日志；Linux正常时可正常关机，随后用户完整冷断电。另一次冷上电走factory默认流程，确认原 `6.1.99-rk3576`。M0测试后恢复必须完整冷断电。

B只验单次签名加载、M0代码执行与cache快照。整个512MiB有效映射、CON16/17以及Linux HELLO_ACK/PONG仍待后续证据，本轮不能升级D。见 [新默认Linux基线](P029_SIGNED_DEFAULT_LINUX_RESULT.json) 和 [签名修复](P029_SIGNED_REPAIR.md)。
