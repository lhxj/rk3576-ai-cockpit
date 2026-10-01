# libs/media

CameraCapture、owned frame、latest-frame mailbox与纯C++ NV12转换。

P003当前实现`ICameraCapture`、Host synthetic backend、可选Linux
`V4l2MplaneCameraCapture`、`CapturedFrame`、epoch/sequence统计及stride-aware
NV12->RGB888。V4L2 backend在DQBUF后深拷贝并在发布前QBUF，不向Qt暴露MMAP地址。
DMA-BUF/RGA/zero-copy仍为后续性能工作。
