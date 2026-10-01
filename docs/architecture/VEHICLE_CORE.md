# Vehicle Core Host Foundation

本文冻结 P006 的 Host 控制面契约。它不证明 Camera、录像、Voice、RTOS、MPU、
RPMsg 或开发板功能可用；当前 adapter 和状态来源均为 `MOCK`。

## 职责与边界

`vehicle_core` 接收结构化命令，校验版本、消息类型、参数、来源、request id、
boot epoch、deadline、重复请求和目标服务健康，随后路由到领域 adapter。它维护
唯一 canonical `VehicleState`，发布 revisioned 全量 snapshot event，并为未来 UI
提供 `IVehicleCoreClient`。

它不打开 V4L2、ALSA、RPMsg、GPIO、I2C、模型或 Qt，不处理视频帧、PCM、编码、
推理和界面绘制。真实服务只可通过后续 adapter 接入。

## Command 与 wire

`VehicleCommand` 包含：

- `protocol_version`、`message_type=VEHICLE_COMMAND`；
- `request_id`、`session_id`、`boot_epoch`、有限 `deadline_ms`；
- `source`：UI、VOICE、SYSTEM、REMOTE、TEST；
- 白名单 `CommandType` 与受限 key/value parameters。

首版白名单是 QUERY_STATE、Camera select/snapshot、Recording start/stop、RTSP
start/stop、Media play/pause/stop、Voice session start/cancel、模拟 LED/Buzzer。
REMOTE 首版只能 QUERY_STATE。VOICE 必须带非零 session id。Camera 和 bool 参数
采用逐命令 schema；未知 enum、重复/额外参数和任意 `RUN_SHELL`、`WRITE_FILE`、
`REBOOT` 字符串均没有执行路径。

Host loopback 使用 `libs/ipc/InMemoryTransport`。Vehicle payload 逐字段使用 big-endian
长度前缀编码，不复制 C++ struct 内存；外层复用 `libs/protocol` 44-byte header。

## 生命周期：ACK 不等于 RESULT

```text
sendCommand
 -> synchronous validation
 -> reserve request id
 -> bounded command queue
 -> ACK(ACCEPTED)
 -> core event loop
 -> domain adapter
 -> completion event
 -> terminal RESULT/ERROR
 -> state commit (only when applicable)
```

非法、过期、旧 epoch、未授权、服务 Offline 或 command queue 满时直接返回错误，
`ack_emitted=false`。新请求成功入队才产生 ACK；ACK 只表示 accepted。ACK 和 RESULT
带同一 request/session/epoch，并带单调 `lifecycle_sequence`，因此可验证 ACK 先于
RESULT。Recording ACK 后可进入 `Starting`，只有 adapter SUCCESS 才进入
`Recording`。业务 FAILURE/TIMEOUT 进入 `Error`，不会误报成功。

重复 request id 的语义：有限窗口内若 fingerprint 完全相同，返回原
`shared_future`，`disposition=REPLAYED`、不再发 ACK、不调用 adapter；相同 id 配不同
命令返回 `DUPLICATE_REQUEST`。默认只保留最近 128 个已完成请求；窗口外不提供跨
重启持久幂等。Recording 已处于 Recording 时再次 START 返回幂等 SUCCESS，不再次
调用 media adapter；STOPPED 时重复 STOP 同理。

## Deadline、late result 与 cancel

入站时 `now > deadline` 返回 EXPIRED。已受理请求由 core loop 检查 deadline；超时
产生 TIMEOUT terminal result。完成记录随后形成栅栏，任何晚到 SUCCESS 只增加
`ignored_late_results`，不能更新 canonical state。

`VOICE_SESSION_CANCEL` 复用现有 session id 语义：core 取消同 session 的 pending
VOICE_SESSION_START，先令原请求得到 CANCELLED，再路由 cancel 命令。原 adapter 的
晚到结果同样被终态栅栏丢弃。本版本没有另造通用 cancel wire type。

## Canonical State 与 revision

`VehicleState` 包含 Front/Rear availability、selected camera、Recording、RTSP、
Media、Audio、Voice、Vision、Language Model、RTOS、Sensor、模拟 LED/Buzzer、Wi-Fi
以及各服务 health。每个字段区分 value、condition、source 和 sequence；Host 默认
来源为 MOCK。Rear 默认 UNAVAILABLE；RTOS、Sensor、Vision、LLM 等默认 OFFLINE，
即使 MockRtosAdapter 可接收模拟命令也不表示 RTOS 在线。

`StateStore` 只由 core event loop 写。有效变化令全局 `revision` 严格递增，并发布
携带完整 snapshot 的 `StateChangedEvent` 回调；无变化、失败的非状态命令和 late
result 不增加 revision。首次连接调用 `getSnapshot()`，再订阅后续全量 event。
后续 UI 必须丢弃低于其已应用 revision 的 snapshot。

## Service Registry 与 adapter

Service health 独立于业务状态，支持 UNKNOWN、STARTING、ONLINE、DEGRADED、OFFLINE、
ERROR。只有 ONLINE/DEGRADED 可受理目标命令；例如 Media Offline 时 Recording Start
直接 UNAVAILABLE，不产生 ACK。Media Online 也不等价于 Recording Active。

领域 adapter 为 Media、Voice、RTOS、System。当前 Mock 可按 command 配置 SUCCESS、
FAILURE、TIMEOUT；TIMEOUT 保留可控 completion，便于无 sleep 测试 late result。
MockRtosAdapter 的成功 result 显式 `simulated=true`，state condition 为 SIMULATED。

## 线程、队列与停机

一个 joinable control worker 消费固定容量 command queue，并优先回收固定容量
completion queue。adapter 异步 callback 只能投递 completion，不能直接写 StateStore。
没有 detached thread。`start()` 创建本轮 queue/worker；`stop()` 停止受理、取消
adapter pending、关闭 queue、唤醒 worker、join，并以 CANCELLED 完成尚未结束的
future。`stop()` 幂等，同一对象可再次 start。subscriber callback 在 core worker
运行，必须快速且不可重入阻塞。

## Voice CandidateAction

```text
VoiceSessionController
 -> CandidateAction
 -> IVehicleCommandSink / VehicleCommandSinkAdapter
 -> token + deadline + whitelist validation
 -> VehicleCommand(source=VOICE)
 -> vehicle_core
```

桥只映射现有 OPEN_CAMERA、START_RECORDING、STOP_RECORDING。session 必须先以有限
deadline 激活；旧、过期、取消 token 和额外参数被拒绝。确定性命令从 router 直接
进入该桥，不经过 LLM。LLM 即使将来产生候选，也只拥有同一白名单入口，不能调用
shell、service adapter 或硬件。

## Client 与后续 UI 集成

`IVehicleCoreClient` 提供 `send_command()`、`get_snapshot()`、`subscribe_state()`和
`boot_epoch()`。
当前有 direct in-process client 和通过 `InMemoryTransport` 的 Host loopback client。
独立integration分支已经实现 `VehicleCoreUiBackend -> IVehicleCoreClient` 的Host/Mock
控制闭环；它使用in-process client，不是跨进程IPC。后续进程化仍须单独选择并实现
真实transport，不能把本轮结果写成UDS/ZeroMQ或daemon已完成。
