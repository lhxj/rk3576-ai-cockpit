# Full-system resource results

下方未测表属于12:42Z历史快照；本次获批实际负载结果见末尾更新。

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

## 2026-10-04获批302.24秒共存实测（BOARD_TESTED）

采样周期5秒。CPU基于应用`/proc/PID/stat`的delta，100%=一个核心，
RSS/PSS读取smaps_rollup；内存/温度/线程/FD来自内核实际值。
表内应用CPU/RSS/PSS不包含另一个ffmpeg客户端进程，也不是整个系统CPU。

| 指标 | 实测 |
|---|---|
| CAM0 | 9549 frames，29.8747fps；sequence gap/DQBUF/QBUF=0 |
| MPP / Recording | encoder instances=1；encoded frames=9395；encoder error/overflow=0；record packets=9335、bytes=313516338、overflow=0 |
| RTSP | RTP packets=266202、drop=0；ffmpeg实际UDP解码9384帧，decoder日志0字节 |
| RKNN | 2365 inference frames，7.46875fps；rate-limit drop=7092、queue drop=0、queue peak=1；drop不是零，按现有latest-frame策略保留 |
| Qt preview | converted/delivered=2195/2195；displayed fps=6.81741，不冒充30fps显示 |
| Voice | 实际audio frames=4872640；Audio XRUN=0、audio overflow=0、queue peak=1；无真人识别验收 |
| CPU全部负载 | 61样本，151.14%–206.31%，平均155.25%；加载阶段最大256.51% |
| RSS / PSS全部负载 | RSS 279760–280044KiB（约273.2–273.5MiB）；PSS 265572–265852KiB（约259.3–259.6MiB） |
| MemAvailable | 全负载最低2875228KiB（约2.742GiB），最高2882356KiB |
| CMA | CmaFree=12192KiB，全程采样不变；不等于普通DDR容量 |
| 温度 | 所有thermal zone采样峰值72.999°C；约73°C是实测，不承诺长稳/夏季余量 |
| 线程 / FD | 全负载26 / 50，61次采样均不变 |
| RPMsg | 全负载179→468 PONG，timeout/error=0；Linux往返RTT采样1–2ms，不是单向跨域延迟 |
| 退出后 | PONG492→495；root fuser无CAM0/ALSA持有者、8554无监听、无项目残留进程 |

录像ffprobe：`codec_name=h264,width=1632,height=1224,r_frame_rate=30/1`。
编码独立时钟fps未单独暴露，不能将输出标称30/1视为全部链路实测编码fps。
本次不覆盖无线外部RTSP、多客户端、真人语音准确率或长期稳定性。
有界dmesg片段仍有vblank警告；采样日志未隐藏错误，详情见Board结果。

资源原始`resources.jsonl`35500字节，SHA256
`d09d7b1388122b7280878f878f413e8a4cea30e9b0a77a856383ee3c6dcc50c3`；
仅存Git忽略目录。汇总计算见同目录`measured-summary.json`。
