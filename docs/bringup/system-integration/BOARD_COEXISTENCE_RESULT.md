# Board coexistence result

2026-10-04。**SYSTEM_INTEGRATION_HOST_PASS / BOARD_COEXISTENCE_BLOCKED**。
Host门全部完成后，于`2026-10-04T12:42:00Z`（北京时间20:42）进行了有限串行只读SSH。
调用既有`_common.sh`的board_lock与SSH安全参数，使用
`ssh -o HostName=10.232.249.223 lubancat 'sh -s'`（timeout40秒），退出0。
没有上传/加载KO、修改boot、重启、采流、录音或启动任何Application负载。

| 门 | 状态 |
|---|---|
| T1 Linux/M0/paired RPMsg | Linux默认系统正常；M0持续运行UNVERIFIED；paired/RPMsg前提BLOCKED（见下） |
| T2 Qt + VehicleCore | NOT_RUN |
| T3 CAM0 Preview | NOT_RUN |
| T4 RKNN Vision | NOT_RUN |
| T5 RTSP | NOT_RUN |
| T6 Recording | NOT_RUN |
| T7 real VoiceRuntime ALSA/VAD/Sherpa | NOT_RUN |
| 300秒全系统与期间echo | BLOCKED_NOT_RUN；默认环境无RPMsg端点，冻结服务亦无重复echo |
| clean shutdown/退出后echo | BLOCKED_NOT_RUN；没有本轮负载可停止，无重复echo入口 |

## 当前只读证据（BOARD_OBSERVED_READONLY）

| 命令/观察 | 实际输出 |
|---|---|
| `uname -a` | `Linux lubancat 6.1.99-rk3576 #8 SMP Fri Apr 24 16:46:57 CST 2026 aarch64 GNU/Linux` |
| `/proc/device-tree/model` | EmbedFire LubanCat-3-v2 |
| `/proc/cmdline`只提取root/boot/stage字段 | root=/dev/mmcblk0p3，boot_part=2；amp_test_stage不存在 |
| `/sys/bus/rpmsg/devices` | 0个设备 |
| `/sys/bus/rpmsg/drivers` | 仅`rpmsg_ns` |
| `/proc/modules`按rpmsg/amp_echo过滤 | 无匹配模块 |
| `/proc/device-tree/chosen`AMP属性 | 无 |
| `/boot/amp-p029/rpmsg-c-v1/stage-C.scr` | 文件存在；不代表已执行/运行 |
| 按项目程序名查看process、ss监听8554/8556 | 未见匹配进程/监听；不等于本轮shutdown测试PASS |
| dmesg按rpmsg/m0.echo/mcu-amp过滤 | 可读取、无匹配行；未清日志或更改日志级别 |

不能仅据Linux sysfs空列表宣称M0已物理停机；结论是**没有当前可用最小RPMsg链证据**。
当前kernel/无stage与AMP冷恢复记录一致，不能在默认#8加载paired kernel的KO来补测。

## 停止原因与解除条件

1. **SI-B01：已有冻结AMP运行态不在当前默认boot中。** 使用冻结C入口需要用户安排
   单独的受控启动/冷恢复窗口；本轮不修改boot/FIT/DT、自动重启或重刷。
   即使恢复该运行态，应用在paired kernel下仍需全部回归，默认#8历史PASS不能迁移认定。
2. **SI-B02：持续/退出后重复echo与冻结服务能力不符。**
   `patches/rk3576-amp-platform/0010-m0-bounded-runtime-evidence.patch`第169–184行
   PONG后break、destroy endpoint/queue、deinit；
   `scripts/board/amp_echo_linux/rk3576_amp_echo_test.c`仅probe HELLO→callback ACK后PING。
   当前无repeat/heartbeat用户态入口。解除需独立明确决定后续验证契约/服务范围，
   本轮禁止新功能且冻结最小服务，因此不自行改固件、反复insmod或伪造计数。

两个限制不是对原AMP BOARD_PASS的推翻：原始受控一次收发与冷恢复仍成立。

Application与AMP各自既有PASS保留；VoiceRuntime+Vision+AMP全系统同载未由历史结果证明。
