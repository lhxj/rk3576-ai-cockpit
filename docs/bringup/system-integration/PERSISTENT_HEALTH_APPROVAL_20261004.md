# APPROVAL_REQUIRED_BOARD_CHANGE — persistent health test

2026-10-04。**WAITING_USER_APPROVAL / NOT_DEPLOYED**。不覆盖冻结AMP产物、不改authority tips。
冻结C基础复测与冷恢复已通过，见[FROZEN_C_RETEST](FROZEN_C_RETEST_20261004.md)。

## 本次精确变更

仅新增`/boot/amp-p029/system-health-v1/`，目录部署前必须不存在。7个新文件如下：

| 文件 | 当前状态/hash | 目标字节 | 目标SHA256 |
|---|---|---:|---|
| SHA256SUMS | NEW / 不存在（部署前再核） | 506 | `74c39990cbc0118a976f92158f197788521c062a9aebfc6f2e53321f683c7039` |
| SI_HEALTH.json | NEW / 不存在（部署前再核） | 1676 | `8d4d9d692c6bc271a77783c48cf4bae2970325d59348498e169fa3f25f93e11f` |
| amp-signed.itb | NEW / 不存在（部署前再核） | 131072 | `3be5eea15bdbb44fece3a06bec9fd80e026f27f17fd443edd7466ac67265fe28` |
| preflight-health.py | NEW / 不存在（部署前再核） | 5821 | `247d68eab63884f4a47ce7cf4383ebc411494bb2fd4c8057f82017dcc7628d11` |
| rk3576_amp_health_test.ko | NEW / 不存在（部署前再核） | 90096 | `57b1d830d6687f4f891bbabc3c9694c976c79ab2fcea3d1671a6289d95dea700` |
| stage-health.cmd | NEW / 不存在（部署前再核） | 3068 | `c51d3df591ce84f6fe84956b2d7b8acfad2adb3e734f5dfd9f0e873cf52956fc` |
| stage-health.scr | NEW / 不存在（部署前再核） | 3140 | `fc4146f3134d5f6267ca45adf81d91b9133079c7462073b41426e4bf2501fd04` |

不会修改默认uEnv、Image/initrd/DT、extlinux、boot.cmd/boot.scr或U-Boot flash。
冻结`/boot/amp-p029/rpmsg-c-v1/`与原KO保持；读回18项默认/冻结文件hash未变。
新入口需要**正常关机 + 用户完整冷断电上电**；不是warm reboot或永久启动项。
U-Boot只加载新script到原RAM地址并一次source；bootargs增加test marker仅影响此会话。

复用不写的文件：

| 文件 | SHA256 |
|---|---|
| /boot/amp-p029/Image | `8c82342d740c9aefe0a4f0be8d84c436d14ebf7edb5f462e14631ce355cf3e8d` |
| /boot/amp-p029/initrd | `c000c9ab2126b0a2e64581514077a7ca79aa80660989ebfbcb3a200366a1346d` |
| /boot/amp-p029/stage-C.dtb | `dd68818b7fd27e9abc56522d7c4782adb0b1dd49d274fe1daa75a32ad3366fbf` |

## 测试扩展范围与风险

只修改M0`amp_echo.c`的有界接收循环：180秒/首PONG退出改为900秒/最多1024请求。
HAL、RT-Thread配置、tick gate、内存、FIT resource属性、DTS、mailbox、link4、vring、
buffer、transport及HELLO/ACK/PING/PONG字节均不改。原BIN/FIT/SCRIPT/KO不覆盖。
新Linux test KO单peer/单in-flight，1秒PING、3秒deadline、720秒总窗口、trysend非阻塞。
超时/重复/未知/late reply即FAILED停止，不重试；只读module parameter输出实际counter。
没有MPU6050、sensor/control或正式VehicleCore业务协议，也不将软件模拟RTOS域变ONLINE。

风险：首次长期/并发共享区、cache/IRQ/内存回收与应用负载覆盖，故此变体不是冻结BOARD_PASS。
失败即停止叠加载，保留两路有界日志，不rmmod/insmod或同次重启M0；按批准流程冷恢复。
M0测试15分钟、Linux12分钟，达到边界结束就是边界，不以sysfs残留对象冒充健康。

## 已完成Host证据

- 新独立RTOS/HAL源码副本，SCons退出0、无warning/error；仅amp_echo.c有运行源差异。
- 新独立prepared-kernel O/M，AArch64 KO Kbuild退出0；paired vermagic/source CRC，
  frozen kernel/config/Module.symvers/KO hash均不变。
- 原pinned vendor signer签新FIT，actual controlDT SHA43164981ef...验签PASS；仅正常签名器
  使用既有签名目录，未输出/复制私钥内容；load/entry/code-size/shared资源属性保持。
- Host CI31/31 CTest、47/47 Python，撤回保护5/5；原测试未删。
- 实际共享健康状态机600轮Host模拟往返及错序/重复/timeout/late/window负例PASS；
  其ASan/UBSan/LSan退出0；不充当实板通信。

## 部署与单次执行

主控先把精确7文件和已审查安装器放到`/home/cat/cockpit/`独立staging；在默认系统、
无项目进程/RPMsg端点时，由existing sudo -n安装新增目录，逐项exclusive-create/读回hash。
安装器源码` scripts/board/system_coexistence/install_health_fixture.py`，SHA256
`21a63d6ded46b7b1c6dc168f4f70dfadbb6e50a1c482f80e2c4e7975ec8e3d26`。
Host缺审批/多参数/缺文件/错manifest/符号链接五个负例均PASS；实际精确7文件验证PASS。
不存在sudo权限就停止；不改sudoers、不索取密码写脚本，不移除已有文件。
安装器失败保留新目录取证，不自动清理/覆盖；不得从不完整安装进入冷启动。

双路日志开始后正常关机，用户完整冷断电上电；COM5打断autoboot，核以下身份：
soc cold boot、proper2017.09-g149b1c5、SPL uboot hash7d8fe670d9/controlDT43164981ef均OK、policy0。

```text
load mmc 0:2 0x4c000000 /amp-p029/system-health-v1/stage-health.scr
printenv fileaddr filesize
```

**3140 bytes / fileaddr=0x4c000000 / filesize=0xc44**全匹配后仅一次：

```text
source 0x4c000000
```

不插入其他load/size，不saveenv。COM6须tick gate PASS/link up/cache bypass1、无STOP。
Linux登录后立即执行（保留<100秒的原启动门）：

```sh
sudo -n timeout 10s python3 /boot/amp-p029/system-health-v1/preflight-health.py
# 仅SI_HEALTH_PREFLIGHT_READONLY_PASS且COM6正确后执行一次：
sudo -n timeout 20s insmod /boot/amp-p029/system-health-v1/rk3576_amp_health_test.ko
cat /sys/module/rk3576_amp_health_test/parameters/health_status
```

不加载原echo KO。需要HELLO_ACK=1，PING/PONG持续增长，timeout/error=0，
COM6实际连续PONG计数且cache bypass1；首次PONG就结束不是本变体正常结果。
基础正确后才逐级Application，不并行第二Camera owner、不用synthetic FINAL。
每级检查实际健康；300秒同载与clean shutdown/退出后counter必须在12分钟窗内。
构建/部署Application仅用户目录，UI采用实际图形session，不改系统Qt/GNOME。

## Rollback与恢复

正常停止本次Application进程、确认CAM0/ALSA/8554/worker释放，再观察实际PING/PONG增长。
保存双串口全文（每份最多2MiB），按批准窗口正常关机；用户完整冷断电上电走默认入口。
SSH应确认默认#8/root p3/boot p2、无stage/health marker，RPMsg0，默认与冻结hash保持。
新独立目录被动保留，不参与默认boot；不自动rm目录、不warm reboot、不重刷。
无响应则由用户切断供电，记录失败；不把强制断电写成clean shutdown PASS。

## 审批范围

请求批准上述7文件新增部署、一次SI_HEALTH_V1冷启动/一次KO加载、既有Application有界
逐级和至少300秒同载、正常停止及冷恢复默认入口。无需重新批准已经完成的冻结C复测。
此门来自用户本轮明确要求“涉及/boot/DTB/FIT/uEnv/boot chain/reboot必须先批准”，
及AGENTS L3“逐次请求用户明确批准”；Host编译不替代此授权。
只有全部实测通过才升级SYSTEM_INTEGRATION_BOARD_PASS，当前仍HOST_PASS。

## 后续用户批准事件

2026-10-04用户明确回复“批准上述完整测试窗口”（主控14:00:16Z读取确认）。
此批准覆盖上述独立新增部署、一次冷启动/KO、既有应用有界逐级/300秒及冷恢复。
审批前NOT_DEPLOYED快照保留；实际安装、启动、负载结果后续逐项记录，不提前标PASS。
Application共存测试驱动只组合未改的现有类，单MainWindow/Core/MediaService，
不注入FINAL，不新建RPMsg业务；它不是新增产品入口或功能。原35项native CTest保留。
