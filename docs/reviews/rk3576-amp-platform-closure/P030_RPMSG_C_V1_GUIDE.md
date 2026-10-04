# P030 RPMsg C v1 双向通信验证指南

2026-10-04。**新版C已安装并逐项读回，可验证双向通信。只使用本指南的新路径；不要执行旧C，也不需要重跑B或刷U-Boot。** 仍采用已实测v5固件和配套Linux/DT/KO。

## 1 保存双串口并完整冷启动

COM5：1500000、8N1、无流控；COM6：115200、8N1、无流控。两路都开始保存全文，每份最多2MiB。正常关机、确认完成，完整断电再上电；在COM5中断自动启动。核soc cold boot、SPL proper hash7d8fe670d9/control DT43164981ef均OK、proper g149b1c5、policy0；不匹配/abort/panic就停止。

先复制好第3节的两条Linux命令，登录后立即执行，避免耗尽M0 link后的180秒窗口。总观察最多3分钟；不等HDMI桌面，不在旧B会话加载模块。

## 2 在COM5加载核对并执行一次C

```text
load mmc 0:2 0x4c000000 /amp-p029/rpmsg-c-v1/stage-C.scr
printenv fileaddr filesize
```

必须是 **3100 bytes、fileaddr=0x4c000000、filesize=0xc1c**。不符停止，不手工设置变量绕过、不插入其他load/size。匹配后只执行一次：

```text
source 0x4c000000
```

首行应为 `P030 RPMsg C v1 with proven tickdiag-v5 - factory default unchanged`；RSA/hash与loader成功后继续配套Linux，不能再source或boot旧DT。SCRIPT SHA `9aacb3e6882062b705b0636ceb55ed1e656098a5b33010c43d2a006a1aabd3a9`；FIT SHA `348109ebecda0f00314d4dbcaaae0f8a51e2d88ac714c03ab74c1966a3b19bd6`。

COM6仍有128条PACE和32K率门输出，这是正常的约一秒取证。需见 `P030 tick rate gate PASS`、`P029 link up`、`cache link ... bypass=1`。若link已及时就绪，可能没有first_mdelay checkpoint，不要求再次证明B延时。**C中link timeout 15s不算通过**；它说明本次未连上，停止，不加载KO。

## 3 配套Linux登录后立即只读核验再加载一次模块

COM5登录后先运行：

```sh
sudo -n timeout 10s python3 /boot/amp-p029/rpmsg-c-v1/preflight-c.py
```

它只读取C身份、reserved DT/no-map、System RAM、实际ring/DMA backing、服务和KO身份；不会加载模块或发送RPMsg。应打印实际rings0x47d00000/0x47d08000、DMA base0x47d10000、echo service和 **`P030_C_PREFLIGHT_READONLY_PASS`**。超时、STOP、Traceback、权限拒绝、服务缺失或C启动已超过100秒，一律停止，不跳过、不修改sudo配置。

只有只读核验PASS，且COM6已有rate gate PASS、link up/cache bypass1、无STOP/异常，才立即运行一次：

```sh
sudo -n timeout 20s insmod /boot/amp-p029/rk3576_amp_echo_test.ko
```

KO自动发送HELLO；收到HELLO_ACK后发送PING。命令非0/超时或异常停止，不再次insmod、不卸载模块重试。timeout不能保证终止内核D状态；若系统无响应或发生中断/日志风暴，保留日志并结束此次会话。

## 4 双向通过需要的实际证据

| 位置 | 必须观察到 |
| --- | --- |
| Linux COM5或dmesg | 实际rings0x47d00000/0x47d08000；buffers dma0x47d10000 |
| Linux COM5或dmesg | `M0 echo channel ...`，接收 `HELLO_ACK` 和 `PONG` |
| M0 COM6 | 实际rx shared_va在[0x27d10010,0x27d20000)，以及PONG sent |
| M0 COM6 | cache entry/link/after-pong snapshots均bypass1，无STOP/异常 |

登录后可保存一次有界日志摘录：

```sh
sudo -n dmesg --color=never | tail -n 200
```

不清dmesg、不重复发送。只有M0 PONG sent、只有服务注册或preflight PASS，不能判双向通过。M0日志的pa_proposal是contract换算，要与Linux实际backing和真实通信一起审查，不能当安全寄存器原值读回。已访问子范围的通信不证明整个512MiB映射或长期/缓存开启稳定性。

## 5 结束并冷恢复默认系统

成功后立即保存双日志；失败/缺响应最多观察到总3分钟，不无限等待。正常关机，确认完成后完整冷断电再上电，走默认入口。系统失去响应时由用户切断供电；不warm reboot、不在同次会话重启M0。

恢复后在COM5保存：

```sh
uname -a
cat /proc/cmdline
```

应为默认6.1.99-rk3576 #8、root p3、无amp_test_stage。回传COM5/COM6全文、上述摘录/恢复身份，并说明已完整冷断电。当前只具备执行条件，HELLO_ACK/PONG和C后冷恢复须据本次结果确认。
