# apps/voice_srv

VAD/ASR、意图、会话和TTS边界；未来调用infer_srv的LLM。任务P005。

`VoiceRuntime`现在拥有VAD live pipeline和`VoiceIntentDispatcher`的启停顺序，外部
application继续拥有ALSA/VAD/ASR backend、`VehicleCore`和`MediaService`。FINAL回调
只复制到有界queue；worker完成确定性路由和Core提交。板端验证限定为安静环境下真实
ALSA/VAD/ASR运行时启停，加同进程synthetic FINAL控制真实CAM0/MPP；不属于实时语音
控制、wake word、TTS或完整语音助手。
