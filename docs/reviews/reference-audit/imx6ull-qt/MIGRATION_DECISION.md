# IMX6ULL Qt reference migration decision

状态：ACCEPTED，2026-10-01。

## 决策

```text
Reference Role: UI_REFERENCE_ONLY
Migration Strategy: REIMPLEMENT
License: LICENSE_UNVERIFIED
```

不以该RAR作为源码基础，不做ARM32二进制移植，不恢复旧IMX6ULL BSP或FSL
sysroot，不复用AP3216C sysfs，不采用QProcess启动三个旧GUI ELF作为最终
架构。RK3576 `cockpit_ui` 根据当前产品功能和系统接口重新实现。

## 允许参考

- 页面和功能入口组织；
- 大触控按钮的交互意图；
- 音乐播放器的播放、上一首、下一首、列表、进度等控制项；
- 视频播放器的播放、音量、全屏、列表、进度等控制项；
- 传感器图表和状态呈现方式。

## 禁止复用

- `QTMenu`、`QMusicPlayer`、`QVideo`、`senor` ARM32 ELF；
- 所有 `.o`；
- `moc_*`、`ui_*`、`qrc_*` 等Qt生成文件；
- 旧Makefile、`.qmake.stash`、Cortex-A7参数及FSL sysroot路径；
- `/sys/class/misc/ap3216c/` 数据访问；
- `./QMusicPlayer`、`./QVideo`、`./senor` 的QProcess启动方式；
- 未经单独授权的图标、歌曲和视频。

## 目标结构

```text
cockpit_ui
├── Home
├── Camera
├── Media
│   ├── Music
│   └── Video
├── AI
├── Vehicle / Sensor
├── Monitor
└── Settings
```

应用采用一个主Qt shell，以 `QStackedWidget` 或统一页面路由切换页面。
Qt只负责display、interaction和state presentation：

```text
Camera: media_srv -> frame delivery -> cockpit_ui
Media:  cockpit_ui -> service IPC -> media_srv / audio_srv
Sensor: RT-Thread -> RPMsg -> vehicle_core -> cockpit_ui
AI:     infer_srv -> vehicle_core / IPC -> cockpit_ui
```

GUI线程不得采集V4L2帧、运行推理、阻塞音频或RPMsg，亦不得执行长文件和网络
操作。服务不可用时页面仍启动，并显示UNKNOWN/OFFLINE/ERROR，而不是默认ONLINE。

## 影响

- P004由“旧项目迁移盘点”改为RK3576新实现，细分UI-01至UI-10。
- 旧参考不能缩短Camera、Media、AI、AMP或Sensor的真实集成验收。
- 获取完整旧源码不是cockpit_ui开工门；Qt/session/platform plugin盘点与当前
  服务接口才是实现前提。
- 未来如获得源码或资源授权，只能重新评估个别自有权利明确的设计/素材；本
  决策不会自动变为二进制或旧BSP移植。
