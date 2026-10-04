# Board coexistence result

下方12:42Z是历史预检查；后续获批实测见本文件末尾更新，不将旧阻塞记录抹除。

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

## 2026-10-04 14:21–14:29Z：获批SI_HEALTH_V1共存实测

**最终：SYSTEM_INTEGRATION_BOARD_PASS（本次302.24秒有界窗口）。**
环境：Debian12，`6.1.99-rk3576-m0echo-p026`，stage C + SI_HEALTH_V1，
实际cat GNOME X11会话（从运行进程确认），Qt5.15.8。没有改变默认启动。
用户分别批准完整7文件独立部署/冷启动/负载/恢复窗口，并确认双UART日志就绪。
M0 test-only变体仅延长echo窗口和重复PING/PONG，冻结产物、HAL/布局/DTS不变。

执行：持`board_lock`，`ssh -o HostName=10.232.249.223 lubancat`
运行有界`run_coexistence.py`（外层650秒/板内620秒），最终退出0。
Application构建/执行仅链接原有组件，test-only驱动不新增产品业务；源码包SHA
`660336b6a063f4ee97b6ec6e53948d7c6e11fc9084b4cd02e37cc243b788e6ef`。

| 门 | 本次实际结果 |
|---|---|
| T0 Linux / T1 M0 / T2最小通信 | paired kernel + SI_HEALTH_V1；真实HELLO_ACK，PING/PONG持续32→101；timeout/error=0 |
| 现有板端CTest | 35/35，61.21秒；期间PONG继续，测试数量未减少 |
| Qt/Core | PASS；实际X11全屏shell；RTOS业务和Voice UI adapter仍MOCK，不冒充业务接通 |
| CAM0 Preview | PASS；CIF sensor `m00_b_ov8858 3-0036`启用链→ISP0 mainpath `/dev/video11`，实际采集与Qt预览 |
| RKNN Vision | PASS；实际MobileNetV1 RK3576模型与已有runtime，owned frame共享 |
| RTSP | PASS；8554/cam0，板内UDP回环ffmpeg真实解码9384帧，decoder日志0字节；不计无线客户端验证 |
| Recording | PASS；一个MPP编码器同时供RTSP/文件；H.264 1632×1224 30/1，文件正常关闭 |
| VoiceRuntime | PASS；实际ALSA/VAD/Sherpa启动、采集、运行、停止；synthetic FINAL=0；不宣称真人控制 |
| 五分钟全部共存 | PASS：302240ms；64次资源采样，其中61次全部负载样本 |
| 负载期间RPMsg | PASS；全部负载采样PONG179→468，timeout/error=0，endpoint绑定未异常消失 |
| clean Application shutdown | PASS；正常命令/worker退出，进程退出0；CAM0/ALSA/8554释放，root fuser退出1且无输出（无持有者） |
| 退出后RPMsg | PASS；控制器471→474，独立只读492→495；最终恢复关机前557/557、零timeout/error |
| 冷恢复 | PASS；14:29:36Z批准的正常关机，用户冷恢复；14:33:06Z只读核实默认#8、无stage、RPMsg设备0，18项默认/冻结文件大小/hash与窗口前相同 |
| 本次持续echo双UART归档 | 用户确认双日志已开始及PONG持续/零错误；完整文件尚待用户上传。实时Linux counter、模块绑定、实际PONG已独立保存，不将待归档日志写成已收到 |

首次控制器在任何应用负载前因错误的“sensor和ISP必须同一张graph”检查退出1。
保存`coexistence-controller.log`与`paired-media-graph.log`；修正仅涉及测试守卫，
第二次控制器退出0。没有为绕过失败修改camera配置/产品源码，也未重新启动M0。

内核有界读回265855字节，片段从uptime约250秒起，含2条vblank 693us<1000us警告；
不能据此推断整个启动无警告。应用seq gap/DQBUF/QBUF/MPP/record/audio计数为0。
该警告保留为已知问题，不直接归类无害；完整启动证据以双串口归档补充。

原始证据：忽略目录`artifacts/local/system-board-validation-20261004/`。
`coexistence-board-evidence/application.log` 28330字节，SHA256
`a56ceff5c3f24da386c54b5f1e251bd22e09fa751855ef92337b96abc1a73937`；
`native-ctest.log` SHA256 `f577d4ecf767712daecc0568fc0c046f722845742c9aa9b2a748e621c8dd055f`。
所有采集、录音与模型留板端/外部忽略目录，未进入Git。

### 最终验收范围

原冻结C已先用双UART复测通过；持续窗口采用用户特批、独立签名及独立目录的
test-only persistent echo / Linux health KO。没有覆盖冻结artifact或改变布局/transport。
M0活性由持续真实回包与固定endpoint来源证明；退出后健康不能只由sysfs对象存在推断。
本次包含实际Qt显示/预览路径，但不新增人工触摸验收；VoiceRuntime真实运行不等于真人语音控制。
全部35板端CTest及31 Host CTest、47 Python通过；本次覆盖的旧功能没有发现关键回归。
vblank警告、约6.82fps Qt显示率、约73°C峰值均保留，不给长期性能/热余量保证。
MPU6050、sensor/control任务、VehicleCore↔RTOS业务、CAM1、TTS/RKLLM仍NOT_IMPLEMENTED。
