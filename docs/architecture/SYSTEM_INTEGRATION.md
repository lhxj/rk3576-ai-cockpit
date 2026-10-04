# RK3576 System Integration

2026-10-04。唯一输入：Application `8585c66d27fa65ef11a6531b95656acfa3dc9e8b`
与AMP/RPMsg `ec56833276d31df1e1ce8d36741a552042e2b6ca`。
分支 `agent/system-integration`，普通merge保留双方历史。没有新功能。

本轮结果：**SYSTEM_INTEGRATION_HOST_PASS**。31/31 Host CTest、41/41 Python、
额外AMP单测/静态与harness通过；全31项ASan/UBSan、非Qt27项LSan通过。
Board只读确认默认kernel#8、无stage、RPMsg设备0；按用户要求停止后续负载。
全系统Board共存**BLOCKED_NOT_RUN**，没有部署/改boot/重启。

## 收敛边界

Linux Application：Qt单shell/VehicleCore/唯一CAM0 MediaService、Preview/Snapshot、
共享MPP H.264 encoder的Recording/RTSP、VoiceRuntime/ALSA/VAD/Sherpa/
Deterministic Intent及RKNN Vision保留原源码/测试。

AMP：BUS M0 RT-Thread/RPMsg-Lite、现有signed FIT、DTS、配套kernel、Linux echo KO
及冷恢复证据完整保留。应用控制面尚无VehicleCore↔RPMsg业务adapter。
两域当前只是源码可共存；最小echo不是传感器/控制业务协议。

M0 memory layout、reserved memory、FIT、mailbox、link4、vring、transport与
RT-Thread服务均冻结。无地址调整，无generic remoteproc替换，无MPU6050新开发。

## 运行验证门

Application基线PASS与AMP最小链BOARD_PASS分别有证据，不能合成为共存PASS。
冻结C采用`6.1.99-rk3576-m0echo-p026`，默认`6.1.99-rk3576 #8`不是同一运行环境。
冷恢复默认Debian后不自动重启M0。

最小echo有一个关键限制：
`0010-m0-bounded-runtime-evidence.patch`在PONG后break、destroy endpoint/queue并
`rpmsg_lite_deinit`；Linux`rk3576_amp_echo_test.c`只在probe发送HELLO并在ACK后
发送PING。没有周期请求或用户态重复echo接口。因此持续300秒请求/应用退出后再echo
尚无可执行入口。保留该限制，不修改已冻结服务来达成本轮指标。

Host通过后先只读检查当前kernel、chosen stage、RPMsg设备/driver/模块。
如未处于既有AMP运行状态，或者重复echo必须改变固件，则停止Board共存测试并
记录BLOCKED。禁止无必要改boot/FIT/DT、重刷或重启。

只有逐级应用、M0、echo以及300秒全系统同载与shutdown/退出后echo均有实测，
才标`SYSTEM_INTEGRATION_BOARD_PASS`。仅源码/Host收敛通过则标
`SYSTEM_INTEGRATION_HOST_PASS`。

## 未实现范围

MPU6050 over RPMsg、VehicleCore↔RTOS业务、RTOS sensor/control、CAM1、TTS、RKLLM
均NOT_IMPLEMENTED。VoiceRuntime安静真实采集不等于真人语音控制；synthetic FINAL
不计实时语音链完成。此merge不改变以上状态。

[构建入口](SYSTEM_BUILD_MATRIX.md) · [合并来源](../bringup/system-integration/MERGE_PROVENANCE.md)
· [板端结果](../bringup/system-integration/BOARD_COEXISTENCE_RESULT.md)
· [资源/指标](../bringup/system-integration/RESOURCE_RESULT.md)
