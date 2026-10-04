# System Integration Board Coexistence ExecPlan

日期：2026-10-04。基线：b172c876 / SYSTEM_INTEGRATION_HOST_PASS。
负责人：主控；单板串行，无子Agent、无业务新功能。

## 目标与范围

同次运行闭合Application + BUS M0/RPMsg的逐级/300秒/clean shutdown/退出后通信。
保留Application8585c66与AMPec568332冻结tip；不做MPU6050、sensor/control任务、
业务协议、CAM1、TTS或RKLLM。

## 证据、未知项与权限

先核Git/状态及冻结指南；只读SSH确认当前默认boot和冻结包hash。
L0/L1可执行；本轮用户明确要求L3启动链/reboot门等待人工批准。
L2 Application负载仅T0/T1/T2基础正常且健康检查路径成立后允许，逐级异常停止。
RTOS原echo是一次性且180秒有界；MSH可能证明RTOS线程活着，但不是RPMsg健康证据。
需要test-only persistent echo独立变体，当前仅列修改范围/风险，没有写固件。

## 步骤与门

1. [x] 干净system分支/merge双亲检查；重试push成功，远端确认b172c876。
2. [x] 持板锁有限只读确认默认#8、无stage、RPMsg设备0，冻结Image/initrd/DT/FIT/
   SCRIPT/KO/preflight/配置hash匹配；无部署/修改/重启。
3. [x] 形成精确审批包：当前默认与目标hash、volatile boot切换、冷恢复及双UART观察。
4. [x] 用户明确批准一次冻结C基础复测，并确认双UART日志就绪；执行一次正常关机命令。
5. [x] 一次C、preflight、一次KO，用户双串口实证T0/T1/T2 PASS；回退默认状态待确认。
6. [x] test-only persistent变体独立Host源码/构建/hash/回归和部署审批，不覆盖冻结包。
7. [x] 有效健康链下逐级Qt/Core→Preview→Vision→RTSP→Recording→VoiceRuntime。
8. [x] 至少300秒同载，真实counter及CPU/RSS/PSS/MemAvailable/温度/thread/fd采样。
9. [x] 正常Application退出，设备/端口/worker释放，并再次实际PING/PONG。
10. [x] 保存有界运行证据，按指南冷恢复；逐项裁定。持续窗口双UART完整文件待用户归档，未冒称收到。

## 文件与构建/恢复边界

当前仅新增本计划与BOARD_CHANGE_APPROVAL_20261004.md；原报告/事实保留。
未改源码、RTOS布局/邮箱/vring/DTS或原冻结产物；新测试变体后续范围单独明确。
无Host依赖安装、原始媒体进入Git或新用户态业务。原始SSH日志仅忽略目录。
不warm reboot、saveenv、重刷或同次会话重启M0；用户物理冷恢复默认入口。

## 当前实际结果与交接

审批前快照为WAITING_USER_APPROVAL。Git推送退出0；只读盘点最终退出0，首次权限拒绝也记录。
后续用户批准冻结C窗口、确认双UART日志开始；COM5/COM6现均已枚举。
13:24:42Z发出一次正常关机，SSH被板端关闭；等待串口关机完成和用户冷启动身份确认。
未自动重启、未编辑boot、未加载KO，persistent变体部署仍需独立精确审批。
后续用户手动C/preflight/一次KO双串口证明基础复测PASS，详见
[复测记录](../bringup/system-integration/FROZEN_C_RETEST_20261004.md)。
主控未自行加载KO。13:30:46Z按已批准窗口发出恢复用正常关机，等待用户冷恢复。
独立M0 Host测试变体编译成功；不是冻结产物，未签名/部署；Linux健康KO待准备。
具体文件/hash/命令/风险见[审批包](../bringup/system-integration/BOARD_CHANGE_APPROVAL_20261004.md)。
最终等级保持SYSTEM_INTEGRATION_HOST_PASS；本轮尚无应用负载或AMP启动测试。

### 2026-10-04 14:17Z 更新（保留以上过程记录）

冻结C基础复测与默认Debian冷恢复均已通过，18项默认/冻结文件hash前后相同。
独立SI_HEALTH_V1已完成Host构建、签名核验及用户完整测试窗口批准；
14:03:41Z仅在新目录安装7文件并读回核验，未覆盖冻结产物或改变默认启动。
现有Application及test-only共存驱动在RK3576原生构建通过，35项CTest保留，尚未执行。
测试驱动位于scripts/board/system_coexistence/，不改产品apps/libs或冻结RTOS源树。
Host CI再次通过31/31 CTest、47/47 Python；持续健康状态机另通过ASan/UBSan/LSan。
控制器已上传，板端已有ffmpeg/ffprobe，无新增依赖安装。
用户确认新双路串口日志就绪后，14:17:11Z发出本窗口批准的正常关机；SSH关闭。
等待用户按Windows《05-PERSISTENT-HEALTH-START.txt》冷启动与一次健康KO加载。
进入基础健康门前不运行应用；五分钟共存与shutdown尚未执行，不能标BOARD_PASS。

### 2026-10-04 14:29Z 更新

已按批准窗口冷启动SI_HEALTH_V1；实时32/32 PING/PONG、零timeout/error后进入测试。
首次控制器错误地要求sensor和ISP同属一张media graph，在负载前退出；保存失败。
只读复核两张CIF/ISP graph及启用链后，仅修正测试工具核验；未采集重试、改硬件或产品代码。
14:21:41Z开始控制器v2；paired kernel 35/35 CTest通过（61.21秒），
Qt/Core、CAM0、RKNN、RTSP、Recording、真实VoiceRuntime逐级通过。
全负载302240ms，最终capture 29.8747fps、Vision 7.46875fps、单MPP编码器，
record overflow、RTP drop、Audio XRUN/overflow均0。真实RTSP回环UDP解码9384帧、错误日志0字节。
正常Application退出且root fuser确认CAM0/ALSA无占用、8554无监听、无项目进程。
退出后真实PONG 492→495；Linux endpoint/driver仍在。原始证据仅忽略目录。
14:29:36Z恢复用正常关机前557/557 PONG且零错误，随后SSH由板端关闭。
等待用户冷恢复默认Debian与本次双串口归档；最终等级暂不提前升级。

### 2026-10-04 14:33Z 最终裁定

用户确认完整冷恢复；14:33:06Z只读核实默认kernel#8、无stage、RPMsg设备0；
18项默认/冻结文件大小/hash与窗口前一致。所有运行门有实时证据，最终等级
SYSTEM_INTEGRATION_BOARD_PASS，仅覆盖本次302.24秒SI_HEALTH_V1窗口。
完整持续窗口双UART尚未上传；已保存冻结C双UART、此次真实Linux回包计数、
Application/CTest/解码/资源/退出及冷恢复证据，不将“日志就绪”当“文件已归档”。
没有开始MPU6050、正式RPMsg业务、CAM1、TTS/RKLLM等下一阶段工作。
