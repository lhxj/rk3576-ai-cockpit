# AMP/RPMsg 当前集成裁决

2026-10-04。**BOARD_TESTED_MINIMAL_CHAIN / READY_TO_INTEGRATE / FROZEN**。

当前唯一集成交接入口：[AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md)，[机器索引](../../amp/AMP_RPMSG_INTEGRATION_TIP.json)。

1. 真实冷C、AMP签名/loader、配套Linux和只读运行核验通过。
2. Linux HELLO_ACK/PONG、M0两次共享指针接收/PONG sent、entry/link/after-pong bypass1，最小链双向通信通过。
3. 用户完整断电再上电，恢复默认#8/root p3/无stage，通过。

实板证据：[C执行记录](P030_RPMSG_C_V1_EXECUTION.md)、[JSON](P030_RPMSG_C_V1_EXECUTION.json)。源码补丁、DTS/FIT/Linux/RTOS/HAL身份、提交依赖和限定范围均已整理于tip。

用户冻结当前链，不开发RTOS业务，不合UI/Voice/Media；无需重跑C。本轮仅Host整理，无板操作或新测试。

原始CON16/17/全窗口、长期或缓存开启、热重连/用户态/业务仍无完整实测；不妨碍当前已测最小链源码集成。旧D/full-deployment准入只能解释其旧schema/旧packet，不能覆盖本次BOARD_TESTED事实。

此前裁决全文见 [历史SUMMARY](SUMMARY_HISTORY_20261004.md) 和 [历史索引](../../amp/AMP_RPMSG_HISTORY_INDEX.json)。旧JSON快照不重写成功/失败，旧指南不当当前部署入口。
