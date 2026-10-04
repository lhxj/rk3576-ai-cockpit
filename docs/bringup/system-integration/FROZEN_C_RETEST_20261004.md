# Frozen C base retest — 2026-10-04

结论：**T0 Linux / T1 BUS M0 RT-Thread / T2 HELLO-PING-PONG基础复测PASS**。
证据是用户提交的同次双串口全文与后续有限只读SSH；不是五分钟共存PASS。
Authority tips及冻结BIN/FIT/SCRIPT/KO未改；本轮未启动Application负载。

## 输入与审批

用户批准一次冻结C测试窗口，并确认“双路日志已开始，可以正常关机”。
Windows指导目录：`C:\Users\27432\Desktop\RK3576-System-Coexistence-20261004-211857`。
13:24:42Z主控确认默认kernel/无stage/RPMsg0/无项目负载，发出一次正常关机；SSH断开。
用户随后返回COM5/COM6日志并表示完成步骤。日志包含真实`soc cold boot`。
物理冷恢复后的默认状态单独核验，不能由本次C日志推断。

原始日志仅保存到Git忽略目录`artifacts/local/system-board-validation-20261004/`：

| 证据文件 | 字节 | SHA256 |
|---|---:|---|
| frozen-c-linux-user-log.txt | 81541 | b4fd6c56fdc0c90a4565cfffd581bfb2d323333156f46fb0f0347e5ff91cfd2e |
| frozen-c-m0-user-log.txt | 15326 | 074545a6daa341c74b26b98aa531f461c6a8443eb02d8cf66dae3620f9a99fcf |

## 同次通信证据（BOARD_TESTED，用户提供日志）

| 门 | 证据行号与实际结果 |
|---|---|
| Cold identity | Linux50–70：无SD卡MMC2尝试失败后转MMC1；uboot SHA前缀7d8fe670d9与controlDT43164981ef均OK；soc cold boot |
| Proper/policy | Linux91、157：U-Boot2017.09-g149b1c5、policy=0 |
| 单次C入口 | Linux159–166：SCR3100 bytes、fileaddr0x4c000000、filesize0xc1c；`source`一次 |
| FIT | Linux168–192：原v5 BIN125704、load0x47800000、entry0x141、SHA28691e30790f1585782f3528dbd611c1e8005f8790947462574d73b8a75b6124；RSA/hash及loader成功 |
| T0 Linux | Linux212、245：6.1.99-rk3576-m0echo-p026；root p3、boot p2、amp_test_stage=C |
| T1 M0 | M0开头4.1.1；159–175：tick rate gate PASS、remote_init returned1、link up、entry/link cache bypass1 |
| preflight | Linux1281–1284：rings47d00000/47d08000、DMA47d10000/10000、真实echo服务；P030_C_PREFLIGHT_READONLY_PASS |
| T2 Linux reply | Linux1285–1288：一次insmod；src1024/dst12292；真实HELLO_ACK和PONG |
| T2 M0 RX/TX | M0176–180：shared_va27d18010/27d18210，pa_proposal47d18010/47d18210，len5/4；PONG sent；after-pong bypass1；task complete |

只有1个C source和1个insmod。Linux早期打印被bootconsole重复，不算两次启动。
存在一次输入`0x4c000000`被Unknown command拒绝，没有改变环境或触发第二次loader。
Out-of-tree模块taint如实保留，不能把它写成无任何warning。
启动日志还包含DP PHY -22、缺失SCMI17/22、rkvenc OPP/devfreq、remote sensor等警告。
当前基础往返确实成功，但这些警告的Application影响尚未验证，不能统称无害。

## 随后只读确认与恢复

13:29:58Z有限SSH持项目板锁、45秒外层timeout，退出0：paired kernel，C stage，
uptime218.73；echo/ctrl/ns对象及rk3576_amp_echo_test driver/module存在；无项目进程。
此时M0日志已显示endpoint teardown，**sysfs对象存在不证明远端仍可通信**。
冻结包/默认文件hash读回匹配；没有第二次insmod/rmmod、共享区访问或硬件写入。

13:30:46Z按已批准C窗口的rollback流程确认无项目负载，发出一次正常关机。
SSH被远端关闭，未读到shutdown命令退出码；不因SSH退出1重试。
等待用户完整冷断电上电并由只读SSH确认默认#8/无stage；不自动warm reboot。

随后用户确认完整冷恢复。13:33:02Z相同有界只读SSH退出0：默认#8/root p3/boot p2、
无stage、RPMsg0、仅rpmsg_ns driver、无echo模块/项目进程；冻结/默认文件hash均不变。
因此本次C后冷恢复PASS（物理动作USER_REPORTED，恢复身份BOARD_OBSERVED_READONLY）。

## 后续门

冻结服务在本次PONG后按设计退出。持续/退出后健康检查需要独立test-only变体；
不覆盖原artifact、不变资源或正式协议。900000ms/1024请求的M0 Host变体已在私有
新源码副本编译exit0、无warning/error，仅amp_echo.c变动；尚未签名、部署或实测。
Linux周期健康检查KO、Host检查、精确产物hash/部署审批尚未闭合。
Application逐级、300秒同载、shutdown/退出后RPMsg均NOT_RUN。
最终等级仍为**SYSTEM_INTEGRATION_HOST_PASS**。
