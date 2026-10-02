# Voice Runtime Host Validation

日期：2026-10-02；环境：WSL Ubuntu 22.04，GCC 11.4。

`bash scripts/dev/host_ci.sh`退出0：30/30 CTest和6/6 Python通过。新增
`voice_runtime_integration_test`使用restartable FakeAudio、受控VAD/ASR和真实
`DeterministicIntentRouter -> VoiceIntentDispatcher -> VehicleCommandSinkAdapter ->
VehicleCore -> MockMediaAdapter`，覆盖：

- open/close camera、start/stop recording；
- PARTIAL、NO_MATCH、negation；
- duplicate FINAL只执行一次；
- cancel后的迟到backend FINAL不产生report或Core命令；
- stop并发FINAL与停止后的FINAL拒绝；
- 同一Runtime对象50轮start/stop，capture 50/50，VAD configure一次。

独立`build/voice-runtime-sanitizer`使用`-fsanitize=address,undefined`。以下程序均
exit 0且无sanitizer报告：

```text
voice_runtime_integration_test
vad_pipeline_test
voice_intent_core_integration_test
```

Host结果不包含ALSA、Sherpa真实模型、Camera或MPP硬件。
