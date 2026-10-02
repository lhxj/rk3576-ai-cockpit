# Voice Runtime Orchestration

更新：2026-10-02。等级：`VOICE_RUNTIME_ORCHESTRATION_PASS`。这个等级只说明
运行时所有权、生命周期和本轮组合验证闭合，不等于实时语音控制、
`VAD_LIVE_PIPELINE_PASS`、wake word、TTS或语音助手完成。

## 所有权和边界

`VoiceRuntime`拥有一个`VadLivePipeline`和一个`VoiceIntentDispatcher`，因而负责
它们的start/stop/join以及callback存活期。enclosing application拥有并保证以下对象
比Runtime活得更久：

- `IAudioCapture`：唯一ALSA capture owner在`audio_srv`；
- `IVadBackend`与`IAsrBackend`：模型对象在多次utterance间复用；
- `VoiceSessionController`、`DeterministicIntentRouter`；
- `IVoiceSessionCommandSink`，当前实现为`VehicleCommandSinkAdapter`；
- `VehicleCore`、`RealMediaServiceAdapter`和`MediaService`。

Runtime不匹配规则、不直接构造`VehicleCommand`、不访问V4L2/MPP/RTSP，也不修改
canonical state。Core和Media的启停仍由application runtime负责。

## 运行链和线程边界

```text
ALSA capture worker -> bounded PCM queue -> VAD/ASR worker
                                         -> ASR_FINAL callback
                                         -> bounded dispatcher queue (copy only)
                                         -> dispatcher worker
                                         -> DeterministicIntentRouter
                                         -> VehicleCommandSinkAdapter
                                         -> VehicleCore -> service adapter
```

ASR callback运行在`VoiceSessionController::deliver_event`持锁区内，只检查Runtime
accepting状态、生成有限deadline并`try_push` FINAL；它不调用Router、Core或Media。
Dispatcher worker先做recent-FINAL去重和session/deadline校验，仅在规则MATCH时调用
worker侧session activation hook，然后走原有Router和sink。PARTIAL不入队；stale、
cancelled、expired、negated和NO_MATCH均不进入Core。

VAD processor在ASR输入结束后不再提前把session置为Completed。Dispatcher report
在worker提交结束后完成`Recognizing -> Understanding -> Completed`。这消除了
“FINAL刚入队就被下游视为终态”的竞态。

所有queue有固定容量。所有worker都是joinable thread，没有`detach()`。Dispatcher
停止时先禁止enqueue、关闭queue，并消费已入队项至CLOSED；Runtime先取消session，
因此停止阶段排空的FINAL不能进入Core。

## 生命周期

实际启动顺序：

1. application加载一次Sherpa ASR，并启动MediaService与VehicleCore；
2. Runtime进入STARTING并启动Intent Dispatcher；
3. `VadLivePipeline`配置/复用固定VAD模型；
4. ALSA按16 kHz、mono、S16_LE启动，创建capture与processing worker；
5. Runtime进入LISTENING。

实际停止顺序：

1. `accepting_events=false`，拒绝新的FINAL；
2. pipeline设置stopping并先关闭ALSA capture；
3. Controller和command sink取消当前token，形成同步session栅栏；
4. pipeline取消utterance、关闭PCM queue并join capture/processing worker；
5. Dispatcher禁止enqueue、关闭并排空FINAL queue、join worker；
6. Runtime进入STOPPED；application随后停止Core和Media。

析构函数执行相同有界停止过程。回调捕获的Runtime在两个worker join完成前不会销毁。
错误停止也总会关闭capture；Runtime清理后回到STOPPED，同时`stop()`返回原始错误。

## 状态和指标

整体状态为`STOPPED / STARTING / LISTENING / PROCESSING / STOPPING / ERROR`，不复制
VAD内部utterance状态机。指标包括utterance/FINAL、match/no-match/reject/duplicate、
dispatcher和audio queue peak、XRUN与audio overflow。

`inject_final_for_test()`只用于确定性Host/板端集成验收。它保留新的request、session、
generation、boot epoch、deadline和ASR sequence，并进入同一个FINAL queue。它不是产品
语音入口，也不能作为真人speech到FINAL的证据。
