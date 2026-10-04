# Board coexistence：冻结C启动审批包

状态：**APPROVAL_REQUIRED_BOARD_CHANGE / WAITING_USER_APPROVAL**。
日期：2026-10-04（Asia/Shanghai）。当前系统等级仍SYSTEM_INTEGRATION_HOST_PASS。

## 1. 基线、Git与本轮权限

`agent/system-integration`从干净的`b172c87628293bad29e78c1e5d74c7555f460eea`开始。
本轮重试`git -c http.version=HTTP/1.1 push -u origin agent/system-integration`退出0；
`git ls-remote`确认远端相同SHA。Application8585c66、AMPec568332均未修改。

用户本轮第二节明确要求：涉及启动链切换/重启，先列精确文件、hash、恢复与串口
观察点并等待明确批准。当前仅进行了L0读取及持锁L1 SSH哈希盘点，没有启动、
上传、加载KO、写/boot、修改uEnv/DT/FIT/固件、sudo配置或关机/重启。

## 2. 当前板态与文件（BOARD_OBSERVED_READONLY）

SSH用户cat；HostName覆盖10.232.249.223，原别名/公钥/严格HostKey检查不改。
盘点约2026-10-04T13:02Z，最终SSH退出0：默认6.1.99-rk3576 #8，root p3/boot p2，
无amp_test_stage，RPMsg设备0/driver仅rpmsg_ns/无echo模块。
`/boot/Image`链接Image-6.1.99-rk3576；`/boot/initrd`链接initrd.img-6.1.99-rk3576。
uEnv.txt当前是普通文件，不自行改成软链接。
下表是磁盘文件hash，不冒充内存中已应用overlay后的live DT hash。
live `/sys/firmware/fdt`对cat不可读，PermissionError保持记录，没有升级权限。
U-Boot实际flash hash本轮未读；必须在冷启动串口确认proper149b1c5及SPL检查。

| 当前默认文件 | bytes | SHA256（本轮只读） |
|---|---:|---|
| `/boot/Image` | 43188736 | `e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e` |
| `/boot/initrd` | 6536431 | `425d2a68a4a67e807518c067cfefc1111d0a33af02236761ddaa3794c3205397` |
| `/boot/dtb/rk3576-lubancat-3-v2.dtb` | 307297 | `76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90` |
| `/boot/boot.scr` | 4108 | `c498d9be3e8dc91883124cc734be54c42c271fad0501555c1a69f7ea7fc38325` |
| `/boot/boot.cmd` | 4036 | `9f0262e807a8188ec5dffe52411401bd82b5fb0d8e81af8107284725e2a7db55` |
| `/boot/uEnv/uEnv.txt` | 6061 | `4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64` |
| `/boot/uEnv/uEnvLubanCat3-V2.txt` | 6060 | `1bf53a0f2e95349d6034d3b11943225f4d8d4b90994c6c88c8e10f9587f50f9f` |
| `/boot/extlinux/extlinux.conf` | 296 | `cb12dd640120702a0ac2488f713019d00fed2081cb9276bbb2fe439d7094aa93` |

第一次盘点末尾find触及lost+found权限拒绝，exit1；只读哈希已得到。
修正为显式prune该目录后第二次exit0，原始日志保留；没有sudo或改权限。
初次猜测的MANIFEST.json不存在，源码确认实际名RPMSG_C.json，已另行只读核验。

## 3. 目标冻结产物：无需重新部署文件

板上既有文件与AMP权威索引/忽略目录冻结产物逐项一致；paired module tree已存在。
既有包不覆盖默认Image/initrd/DT/uEnv，不执行安装器、不重刷U-Boot、不重新签名FIT。

| 使用的冻结文件（已经在板上） | bytes | 当前SHA256 = 目标SHA256 |
|---|---:|---|
| `/boot/amp-p029/rpmsg-c-v1/amp-signed.itb` | 131072 | `348109ebecda0f00314d4dbcaaae0f8a51e2d88ac714c03ab74c1966a3b19bd6` |
| `/boot/amp-p029/rpmsg-c-v1/stage-C.scr` | 3100 | `9aacb3e6882062b705b0636ceb55ed1e656098a5b33010c43d2a006a1aabd3a9` |
| `/boot/amp-p029/stage-C.dtb` | 308309 | `dd68818b7fd27e9abc56522d7c4782adb0b1dd49d274fe1daa75a32ad3366fbf` |
| `/boot/amp-p029/Image` | 43188736 | `8c82342d740c9aefe0a4f0be8d84c436d14ebf7edb5f462e14631ce355cf3e8d` |
| `/boot/amp-p029/initrd` | 6603310 | `c000c9ab2126b0a2e64581514077a7ca79aa80660989ebfbcb3a200366a1346d` |
| `/boot/amp-p029/rk3576_amp_echo_test.ko` | 40920 | `cd82f24659ba4a5a09fc314f5d9d3372b28451c8b21c7f64a868b11d11b5fe43` |
| `/boot/amp-p029/rpmsg-c-v1/preflight-c.py` | 5728 | `225c1ae7df1411669f1f49df30a76103c96875a913cd60aa312943ea42c41f8e` |
| `/boot/amp-p029/rpmsg-c-v1/stage-C.cmd` | 3028 | `8ec8d5c18a7f46b8649a7f1eef54c460932162a77a45d1148d799bf347133826` |
| `/boot/amp-p029/rpmsg-c-v1/RPMSG_C.json` | — | `bf76cc4aabeabf564fe7ff12cc72c7e2528bd74e2d4904bab4a853e8cf227db3` |
| `/boot/amp-p029/rpmsg-c-v1/SHA256SUMS` | — | `3286f0dc3e991268427c8a51670badfa98bcb43d0df98ea45e9476d7b16b3aa3` |

冻结RTOS_BIN125704B的SHA256为
`28691e30790f1585782f3528dbd611c1e8005f8790947462574d73b8a75b6124`，
来自已冻结FIT payload身份；本轮未从flash/运行M0读回，不混淆证据。
冻结8MiB U-Boot artifact SHA256为
`f9beef07f807123a48f5115458dedb7bdf47337306df1a981190abf65372c0b3`；
本次不写这个产物。若串口实际loader身份不匹配，则停止，不自动重刷修复。

## 4. 请求批准的确切改变

批准范围仅为**一次原冻结C基础复测窗口**：

1. 用户确认双UART日志就绪与当前无人占用，正常关机；用户确认完成后完整断电再上电。
   需要启动切换，不能用warm reboot替代冻结指南规定的cold reset。
2. 用户在Linux串口中断自动启动，核对下节身份，再按冻结指南手动执行一次既有C SCRIPT。
   它仅在当前U-Boot RAM设置bootargs/root override并load既有Image/initrd/DT/FIT，
   `amp_m0load`一次，然后booti进入paired kernel；没有saveenv或磁盘文件替换。
3. 配套Linux登录后在100秒只读preflight窗口内运行冻结preflight；仅PASS且M0串口
   gate/link/cache正常，才运行**一次**配套KO insmod，取得HELLO_ACK/PONG。
4. 任一条件失败即停止，不启动Application、不重复source/insmod、不热重启M0。
   保存证据，正常关机并人工完整冷断电回默认入口。

这一审批不包含新persistent firmware部署、不包括改FIT/DT/uEnv、重刷、永久启动、
改资源布局/邮箱/vring、业务协议或自动修改sudo权限。

以下是**批准后**才执行的冻结命令，本轮尚未执行：

```text
load mmc 0:2 0x4c000000 /amp-p029/rpmsg-c-v1/stage-C.scr
printenv fileaddr filesize
# 必须3100 bytes、fileaddr=0x4c000000、filesize=0xc1c；匹配后一次：
source 0x4c000000
```

```sh
sudo -n timeout 10s python3 /boot/amp-p029/rpmsg-c-v1/preflight-c.py
# 仅PASS并核对COM6正常后：
sudo -n timeout 20s insmod /boot/amp-p029/rk3576_amp_echo_test.ko
```

sudo权限以已验证路径为依据但本轮未执行sudo；若非交互授权失败，停止，不索取密码
写脚本，不改变sudoers。不得因为模块已加载而rmmod/insmod重试。

## 5. Rollback与串口观察点

Rollback：保存两路有界日志（每份最多2MiB），正常关机完成后用户完整冷断电再上电，
不在同次会话重试M0；不手工改env、不saveenv、不改磁盘。默认入口应回到#8/root p3/
boot p2/无stage，上表默认文件hash应保持。系统失去响应时由用户执行物理断电。

串口连接需要用户确认：Windows本轮.NET枚举COM3/COM4/COM5；PnP显示
USB-SERIAL CH340(COM5) OK，COM6未枚举。不能只凭历史COM号确定目前接哪路UART。
Linux UART历史COM5：1500000/8N1/无流控；M0 UART5历史COM6：115200/8N1/无流控。
本轮未打开串口、未发送字符、未插拔；双路必须实际就绪后才能冷启动窗口。

| 阶段 | 必须观察到；缺失即停止 |
|---|---|
| Cold SPL/proper | soc cold boot；proper2017.09-g149b1c5；SPL proper hash前缀7d8fe670d9、control DT43164981ef均OK；policy0；无abort/panic |
| C SCRIPT | 正确banner、RSA p029dev/hash、单次loader成功、立即boot paired Linux |
| M0 | P030 tick rate gate PASS、P029 link up、cache entry/link bypass1；无STOP/15秒link timeout |
| T0 Linux | 6.1.99-rk3576-m0echo-p026、root p3、amp_test_stage=C；preflight PASS，实际rings47d00000/47d08000、DMA47d10000 |
| T1/T2最小通信 | Linux真实HELLO_ACK和PONG；M0实际receive/len5、len4、PONG sent和after-pong bypass1；双路对应 |
| Default恢复 | #8/root p3/boot p2/无stage；完整冷断电事实由用户确认，不把warm reboot当恢复 |

## 6. 健康检查与最小测试扩展（SOURCE_VERIFIED / NOT_IMPLEMENTED）

冻结`0010-m0-bounded-runtime-evidence.patch`定义180000ms窗口、最多4条接收，
PONG后break、destroy endpoint/queue、deinit。Linux frozen KO只probe HELLO、ACK后PING。
因此必须区分：Linux sysfs服务/KO对象仍存在，不证明已销毁的远端endpoint可用。

RTOS rtconfig开启FINSH/MSH，后续可能通过真实UART只读命令观察RTOS线程存活；
该能力本轮未实测，且不能恢复已deinit的RPMsg或证明负载期间通信健康。
没有找到能同时闭合M0运行、远端endpoint可用、RPMsg实际往返的冻结无修改路径。

要满足300秒负载及退出后通信，**需要test-only persistent echo变体**：

- 从固定v5来源独立新建目录，只延长受控测试窗口并允许重复原4字节PING/PONG，
  HELLO/ACK保留；无新业务消息、sensor/control、资源/地址/IRQ/transport改变。
- Linux独立test KO复用同transport，限定一条in-flight PING、有界deadline/频率/总时长，
  读出request/reply/timeout/error。timeout/异常即失败停止，不把旧PONG计新成功。
- 正常Application shutdown后再发实际PING/PONG；测试结束按规定冷恢复。
- 风险：延长原受控共享内存生命周期，首次覆盖长期/高负载；潜在cache/IRQ/队列/回收
  异常必须如实记录。变体不是冻结BOARD_PASS产物，不能沿用原FIT hash或验收。
- 冻结BIN/FIT/KO/SCRIPT不覆盖、不改原tip；新BIN/FIT/KO必须有独立hash、Host检查、
  明确的独立部署目录和**第二份精确部署审批**。本轮当前尚未编写/构建/签名变体。

原冻结C再次通过仅关闭基础复测门，不能提前标完整BOARD_PASS或开始无健康证明的
300秒叠加载。后续变体准备/部署与Application逐级测试保留独立门。

## 7. 当前结果

Git重试推送成功；板端/包只读核验完成。处于人工审批门，未进入AMP、未加载KO，
T0/T1/T2、Application逐级、300秒、shutdown/退出后RPMsg均NOT_RUN。
保持SYSTEM_INTEGRATION_HOST_PASS。
原始命令/日志在忽略的artifacts/local/system-board-validation-20261004，不提交。

冻结指南：[P030_RPMSG_C_V1_GUIDE](../../reviews/rk3576-amp-platform-closure/P030_RPMSG_C_V1_GUIDE.md)。
