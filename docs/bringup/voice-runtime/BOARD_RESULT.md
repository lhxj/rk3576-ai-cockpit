# RK3576 Voice Runtime Board Result

日期：2026-10-02；设备：LubanCat-3 v2，Debian 12，kernel 6.1.99-rk3576；连接地址
为当次用户提供的`10.232.249.223`，认证继续复用`lubancat`别名。部署目录：
`/home/cat/cockpit/voice-runtime-20261002-01/`。

## T0 inventory

开始前无项目voice/media进程、`/dev/snd/pcmC0D0c`或CAM0 holder、8554 listener。
media graph重新解析为OV8858 `3-0036`经rkisp mainpath到`/dev/video11`；alias当次也
指向video11，但命令使用解析出的显式节点。板端已有Sherpa v1.11.3资产、ONNX Runtime
1.17.1、Silero v5模型、`libasound2-dev:arm64 1.2.8-1+b1`和MPP运行环境；未安装包。

初始源包SHA256为
`e305d1259abb056b9e04775d894947829ffa9e6ed673ad40eab3d91405de60fc`；最终代码快照
`source-code-final-v2.tar.gz`在Host和板端均为
`8db8db7d40714199a0b20f7fb3387ea181629af17bb30de2071e73bc050ab972`。该快照不含
Git元数据、构建产物、artifacts和docs。
GCC 12.2原生全量构建成功，`LD_LIBRARY_PATH=/home/cat/cockpit/asr-target/lib
ctest --test-dir .../build --output-on-failure`为24/24通过。

## T1 live runtime and synthetic control

程序使用120秒外层timeout；实际自然退出。真实链启动并持续运行，其后在同一进程
注入四个synthetic FINAL：

```text
VOICE_RUNTIME state=LISTENING live_seconds=5 asr_loaded=1 vad_load_count=1
sample_rate=16000 channels=1 period_frames=320
打开摄像头  ACK 1 -> RESULT 2
关闭摄像头  ACK 3 -> RESULT 4
开始录像    ACK 5 -> RESULT 6
停止录像    ACK 7 -> RESULT 8
preview_frames=3 recording_packets=91 recording_bytes=2905668
captured_frames=136960 xrun_count=0 audio_overflow_count=0
dispatch_queue_peak=1 audio_queue_peak=1 threads_before=1 threads_after=1
CAMERA_REOPEN=PASS
AUDIO_REOPEN=PASS
```

synthetic命令生成的`recordings-final2/recording_1.h264`由ffprobe识别为H.264 High、
1632x1224、30/1；文件已关闭。测试后无精确匹配项目进程、Camera/ALSA holder或8554
监听。Camera和ALSA都由测试程序重新打开并正常关闭。

内核日志仍出现已知`vblank need >= 1000us ... cur 693 us`警告；本次没有观察到新的
MIPI ERR2或设备占用错误。该警告不因本次功能通过而关闭。

## Boundary

安静运行期间`utterances=0`，四个FINAL全部是明确的synthetic injection。因此本证据
证明真实ALSA/VAD/ASR runtime可启动、采集、停止和释放，并证明同一application中的
synthetic FINAL可驱动真实CAM0/MPP；它不证明真人speech end、实时FINAL或实时语音
控制，也不证明wake word、TTS、RKLLM、CAM1、RTSP语音控制或长期稳定性。
