# Voice/AI Foundation（Host接口基线）

本文件记录 VOICE-00/01 与 VOICE-02/03/04 接口骨架。它不表示 ASR、TTS、RKLLM、RKNN、ALSA 或车控业务已实现。LLM_Voice_Flow 仅为 `REFERENCE_ONLY`，固定审查 commit `be82e87cc334ae6e222f83f7555531d1ddebaa8b`；其源码和二进制没有进入本实现。

## 职责与依赖

| 模块 | 当前实现 | 后续所有权 |
|---|---|---|
| `libs/protocol` | C++ domain types、显式 big-endian 消息编解码、接收端 RequestFence | 跨服务小控制消息；payload schema 后续按类型收紧 |
| `libs/ipc` | 有界可关闭队列、单 worker 的 Host InMemoryTransport | ZeroMQ/UDS 传输适配择一；socket 由单 owner 线程使用 |
| `audio_srv` | PCM格式、采集/播放接口和有界 Host Mock | 唯一允许真实 ALSA backend 拥有 PCM device；协商实际采样率和播放仲裁 |
| `voice_srv` | ASR/TTS/VAD/wake 接口、session controller、测试用精确关键词 router、候选动作 sink | 语音会话、确定性意图、ASR/TTS backend；不打开 ALSA |
| `infer_srv` | 语言/视觉 backend 接口、串行资源槽、可取消 Mock LLM | RKLLM/RKNN 生命周期与4GB预算调度；不打开 ALSA |
| `vehicle_core` | 本轮仅抽象 sink 边界 | 白名单、状态、参数、权限、时效校验及执行路由 |

依赖方向为 `protocol → ipc → audio_srv → voice_srv`（箭头表示被上层依赖），`infer_srv → protocol`。没有 UI 依赖或循环。`voice_srv` 与 `infer_srv` 以 protocol id/session 概念交互；正式跨进程绑定后使用 `libs/ipc`。

## 控制协议 v1

Host wire header 固定 44 bytes，网络字节序，顺序为 magic `VAI1` (4)、version (2)、message type (2)、payload length (4)、request id (8)、session id (8)、boot epoch (8)、deadline Unix ms (8)，后接原样 payload。`MAX_CONTROL_MESSAGE_SIZE = 65536` bytes，包含 header。编解码逐字段移位，不序列化 C++ struct 内存。错误 magic、版本、type、长度、截断和超限全部显式拒绝。当前 payload 是 opaque bytes，后续每个业务 type 需定义独立 schema、UTF-8/参数界限和授权规则；这个通用包不等于已完成的车控协议。

`RequestFence` 在同一接收 owner 内检查 boot epoch、当前 session、deadline 和发起请求 ID。只有 `*_REQUEST`、`ASR_START`、capture start/stop 消耗重复请求记录；`LLM_CHUNK`/`RESULT` 可共享同一 request id。`CANCEL` 引用原 request id，重复取消允许并由业务层幂等处理。最近 ID 记录有固定容量，进程重启后需由持久业务层或更高层幂等键处理跨容量/跨重启重放。`deadline_ms = 0` 表示协议层无期限，产品控制请求应设置有限期限；同机 Unix 时钟用于本阶段，未来跨域时必须转换时间域。boot epoch 应由服务启动实例分配，不能硬编码为常数。

消息类型包括服务 HELLO/READY/HEARTBEAT、音频采集、ASR、INTENT、LLM、TTS、CANCEL、ACK、RESULT、ERROR，以及后续追加的 VEHICLE_COMMAND/STATE_SNAPSHOT/STATE_CHANGED。**ACK = 已接收/已受理；RESULT = 实际操作完成或明确失败。** ACK 不触发“执行成功”的播报；播放完成也只能由未来真实 audio_srv backend 确认。每条业务响应关联 request/session/epoch，错误以 StatusCode 区分非法参数、非法状态、超时、取消、不可用、内部错误、协议错误和过期。当前 Mock 没有实现生产级持久去重；VehicleCommand 已有独立显式 payload schema，其余消息仍需逐类型收紧。

## 队列、线程与停机

`BoundedQueue<T>` 固定条目容量，提供 try/timed push/pop、close、size 和 overflow 统计。close 唤醒等待生产者/消费者，消费者可排空已有条目后收到 CLOSED。InMemoryTransport 消息先编成受上限约束的 wire bytes，送入有界队列；其单个 worker 解码并交付回调，stop 关闭队列并 join。该 Host transport 为 single-use，不允许 callback 自身调用 stop。未来进程 transport 必须由一个线程拥有 socket，并在停机时打断阻塞 I/O；不能使用无限 -1 等待或 detached thread。

## Session、取消与候选动作

`VoiceSessionController` 由服务启动时分配的非零 boot epoch 构造，为每个新会话分配递增 session id 与 generation。新会话使旧 token 失效。cancel 进入 Cancelling，后端停稳后 `complete_cancel` 进入 Completed；旧 token 的 LLM chunk/result、TTS started/finished 和 CandidateAction 均被拒绝。ASR、LLM、TTS 事件还须符合当前阶段。进入 controller 的 event callback 和 `IVehicleCommandSink` 在锁内执行，以保证 `cancel()` 返回后旧交付不再进行；它们必须快速、不可重入，后续实际服务需要在 owner worker 中完成有界交付，避免在回调中阻塞推理或设备线程。

`IAsrBackend` 的事件带 session/request/sequence 和 PARTIAL/FINAL/ERROR；`ITtsBackend` 返回带真实 PCM 元数据的缓冲，不直接播放。新会话启动时编排层还必须调用旧 ASR/TTS/LLM backend cancel 与 `audio_srv` playback cancel；本轮 controller 只提供隔离栅栏，未接真实编排或模型。固定命令的测试 router 仅精确识别“打开摄像头”“开始录像”“停止录像”，其余非空文本走一般问答。不是最终自然语言规则。

`CandidateAction` 只含 action type、参数、来源与 session token，唯一出口是 `IVehicleCommandSink::submit_candidate`。该接口没有 camera、RPMsg、shell 或硬件操作。将来 `vehicle_core` 必须再次验证白名单、状态、参数、权限、deadline 和 session 后才可执行；LLM 只提供候选。

## 模型与音频后端预留

- Sherpa：实现 `IAsrBackend`，由 audio_srv 提供与实际采样率一致的 PCM；先文件输入测试。
- TTS：实现 `ITtsBackend` 输出 PCM buffer，交 audio_srv 仲裁和播放，不直接开 ALSA。
- RKLLM：实现 `ILanguageModelBackend` 的 load/unload/generate/cancel/state；回调携带 session/generation，旧结果由 voice 栅栏丢弃。
- RKNN：实现 `IVisionBackend`，与语言后端共用 `IInferenceScheduler`；4GB板先受控串行调度，不预设 NPU 抢占。
- 真实设备：仅 audio_srv 的后续 backend 可管理 ALSA。当前 Mock 从内存 fixture 读取/记录 PCM，未访问设备。

当前边界仅经过 Host 测试。后续每个真实 backend 独立完成许可、依赖、模型资产、资源预算和板端验证。

后续独立分支已增加正式的确定性文本路由，替代本文件描述的三词测试 router 作为未来意图接口；见 [DETERMINISTIC_INTENT_ROUTER.md](DETERMINISTIC_INTENT_ROUTER.md)。其Host候选动作测试并不表示真实 `vehicle_core` 命令执行已经完成。
