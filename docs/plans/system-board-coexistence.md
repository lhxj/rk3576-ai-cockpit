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
4. [ ] 用户明确批准一次冻结C基础复测，双UART与正常关机/人工冷断电窗口就绪。
5. [ ] 按原已验证指南一次C、preflight、一次KO，T0/T1/T2实证；失败立即结束。
6. [ ] test-only persistent变体独立Host源码/构建/hash/回归和部署审批，不覆盖冻结包。
7. [ ] 有效健康链下逐级Qt/Core→Preview→Vision→RTSP→Recording→VoiceRuntime。
8. [ ] 至少300秒同载，真实counter及CPU/RSS/PSS/MemAvailable/温度/thread/fd采样。
9. [ ] 正常Application退出，设备/端口/worker释放，并再次实际PING/PONG。
10. [ ] 保存有界证据，按指南冷恢复；逐项裁定，不默认BOARD_PASS。

## 文件与构建/恢复边界

当前仅新增本计划与BOARD_CHANGE_APPROVAL_20261004.md；原报告/事实保留。
未改源码、RTOS布局/邮箱/vring/DTS或原冻结产物；新测试变体后续范围单独明确。
无Host依赖安装、原始媒体进入Git或新用户态业务。原始SSH日志仅忽略目录。
不warm reboot、saveenv、重刷或同次会话重启M0；用户物理冷恢复默认入口。

## 当前实际结果与交接

WAITING_USER_APPROVAL。Git推送退出0；只读盘点最终退出0，首次权限拒绝也记录。
Windows目前发现CH340 COM5，未见M0历史COM6，UART对应/连接需用户确认。
具体文件/hash/命令/风险见[审批包](../bringup/system-integration/BOARD_CHANGE_APPROVAL_20261004.md)。
最终等级保持SYSTEM_INTEGRATION_HOST_PASS；本轮尚无应用负载或AMP启动测试。
