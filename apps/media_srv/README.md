# apps/media_srv

唯一摄像头拥有者，任务P003/P007/P009。

当前已实现P003的CAM0基础链：进程内`MediaService`、异步Preview/Snapshot操作、
`RealMediaServiceAdapter`和可选原生V4L2 MPLANE backend。首版只支持
1632x1224 NV12单memory-plane、owned-frame复制、latest-frame mailbox、PPM抓拍和
干净停止/重启。Recording、RTSP、CAM1、MPP/RGA及独立daemon仍未实现。

板端证据见`docs/bringup/media-cam0/`；编译通过不等于Camera节点始终可用。
