# apps/media_srv

唯一摄像头拥有者，任务P003/P007/P009。

当前已实现CAM0基础链：进程内`MediaService`、异步Preview/Snapshot/Recording操作、
`RealMediaServiceAdapter`和可选原生V4L2 MPLANE backend。支持1632x1224 NV12
单memory-plane、owned-frame复制、latest-frame preview mailbox、PPM抓拍，以及
可选Rockchip MPP H.264 Annex-B recorder。Preview与Recording共享唯一capture；
录像使用容量12的非阻塞有界队列，背压显式失败并进入canonical Error。

Host未启用MPP时使用`FakeMediaRecorder`验证生命周期。板端通过
`COCKPIT_ENABLE_MPP_RECORDING=ON`和pkg-config `rockchip_mpp`构建。RTSP、CAM1、
RGA、MP4、零拷贝及独立daemon仍未实现。

板端证据见`docs/bringup/media-cam0/`和`docs/bringup/media-recording/`；编译通过
不等于Camera节点始终可用。
