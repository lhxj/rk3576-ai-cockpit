# apps/media_srv

唯一摄像头拥有者，任务P003/P007/P009。

当前已实现CAM0基础链：进程内`MediaService`、异步Preview/Snapshot/Recording操作、
`RealMediaServiceAdapter`和可选原生V4L2 MPLANE backend。支持1632x1224 NV12
单memory-plane、owned-frame复制、latest-frame preview mailbox、PPM抓拍，以及
可选Rockchip MPP H.264 encoder。Preview、Recording和RTSP共享唯一capture；
Recording与RTSP共享一个`MppH264Encoder`输出的`EncodedPacket`。录像文件和
RTSP网络各有独立有界队列，录像背压显式失败，RTSP背压计数丢包并等待IDR恢复。

Host未启用MPP时使用`FakeH264Encoder`/`FakeRtspServer`验证生命周期。板端通过
`COCKPIT_ENABLE_MPP_RECORDING=ON`和pkg-config `rockchip_mpp`构建。RTSP首版为
单CAM0、单客户端、无认证、RTSP/TCP控制加UDP unicast RTP/H.264；端口/path可配。
CAM1、音频RTSP、多客户端、鉴权、TLS、RGA、MP4、零拷贝及独立daemon仍未实现。

板端证据见`docs/bringup/media-cam0/`、`docs/bringup/media-recording/`和
`docs/bringup/media-rtsp/`；编译通过
不等于Camera节点始终可用。
