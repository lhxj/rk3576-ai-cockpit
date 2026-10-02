# Voice Runtime Orchestration ExecPlan

## Goal

从RTSP稳定提交`2af5303`建立`agent/voice-runtime-orchestration`，把既有ALSA、VAD、
Sherpa ASR、Intent Dispatcher、Vehicle Core和真实MediaService组织成一个有明确所有权
和停止栅栏的Runtime。目标等级`VOICE_RUNTIME_ORCHESTRATION_PASS`。

## Scope and permissions

仓库代码、Host构建/测试和文档属于L0。板端先做L1占用/依赖盘点；随后按用户本任务
明确要求执行一次有界L2音频采集、CAM0和MPP测试。部署只写
`/home/cat/cockpit/voice-runtime-20261002-01/`，不安装包、不改系统服务、boot、设备树
或网络配置。CAM1、RTSP语音规则、wake、TTS、RKLLM、AMP均不在范围内。

## Implementation

1. 保留`2af5303`完整历史，新增`VoiceRuntime`与`IVoiceSessionCommandSink`。
2. FINAL callback只进有界Dispatcher queue；worker激活session并调用既有Router/Core。
3. 把VAD的session completion移到Dispatcher report，防止异步FINAL成为stale。
4. Dispatcher停止时关闭并排空queue；新增queue peak；VAD error stop也必须关闭capture。
5. 加入可重复启动的相同VAD配置路径，不重载模型，不允许运行中更改参数。
6. Host测试四条命令、PARTIAL/NO_MATCH/否定/duplicate/cancel/stop race与50轮启停。
7. 板端在同一进程启动真实ALSA/VAD/ASR，并以synthetic FINAL验证真实CAM0/MPP。

## Validation and result

- `bash scripts/dev/host_ci.sh`：30/30 CTest、6/6 Python，exit 0。
- ASan/UBSan：`voice_runtime_integration_test`、`vad_pipeline_test`、
  `voice_intent_core_integration_test`均exit 0；Runtime测试内部50轮start/stop。
- RK3576 GCC 12.2 native build；CTest 24/24。
- 实板真实runtime协商16000 Hz/mono/320-frame period，采集136960 frames；VAD和ASR
  各加载一次，XRUN/overflow为0，audio/dispatch queue peak均为1。
- 同一进程synthetic FINAL完成CAM0 open/close和MPP recording start/stop；录像
  2,905,668 bytes，91 packets，ffprobe为H.264 High 1632x1224 30/1。
- 退出前后thread为1/1；随后Camera与ALSA均重开成功，无设备holder、无8554监听。

结论为`VOICE_RUNTIME_ORCHESTRATION_PASS`。真实麦克风期间没有要求用户说命令，
utterance为0，因此`VAD_LIVE_PIPELINE_PASS`和`LIVE_VOICE_CONTROL_PASS`仍未成立。

## Recovery

测试有120秒外层timeout且只管理本程序。失败时停止Runtime，再停止Core/Media；不杀
无关进程。板端输出文件保留在独立目录，不覆盖前序RTSP/Recording证据。
