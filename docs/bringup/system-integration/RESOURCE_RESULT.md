# Full-system resource results

2026-10-04。FULL_SYSTEM_NOT_MEASURED：本轮全系统共存因前提阻塞未运行，不沿用独立Application负载
的数据作为M0/paired kernel同载指标。

| 组 | 必须记录的指标 | 本轮 |
|---|---|---|
| Media | capture fps、V4L2 DQBUF/QBUF/sequence gap、recording overflow、encoded fps、RTP drop | NOT_MEASURED |
| Vision | current epoch/result、inference fps/error/drop | NOT_MEASURED |
| VoiceRuntime | captured frames、VAD/ASR运行、XRUN、audio queue overflow | NOT_MEASURED |
| Linux | CPU、RSS/PSS、MemAvailable、temperature、thread/fd count | NOT_MEASURED |
| AMP/RPMsg | M0运行、request/reply、timeout/error、endpoint/driver状态 | RPMsg设备0、driver仅rpmsg_ns、无echo KO；M0持续运行UNVERIFIED；request/reply/timeout未执行不能填0 |
| Shutdown | CAM0/ALSA/8554/worker/recording/process资源释放、退出后echo | NOT_TESTED |

本轮无持续采流/录音文件；原证据仅存忽略目录，不上传原音视频或完整板日志。

只读时空闲/桌面系统快照（不是负载资源成绩）：`/proc/meminfo`
MemTotal=3,988,492 KiB，MemAvailable=3,243,812 KiB，CmaTotal=16,384 KiB，
CmaFree=12,192 KiB。CPU、RSS/PSS、温度、thread/fd和所有业务计数未测。
没有本轮Application启动/停止；未见项目进程和8554/8556监听仅是preflight快照。
