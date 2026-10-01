# apps/cockpit_ui

`cockpit_ui` is a new RK3576 implementation. The IMX6ULL project is
reference-only (`UI_REFERENCE_ONLY`); the migration strategy is `REIMPLEMENT`.

目标页面：

```text
Home
Camera
Media / Music / Video
AI
Vehicle / Sensor
Monitor
Settings
```

应用采用单一主Qt shell，使用 `QStackedWidget` 或项目统一页面路由。实现遵循
当前 `docs/architecture/INTERFACES.md`，通过服务IPC消费数据和发出请求。

Do not copy:

- generated Qt files (`moc_*`, `ui_*`, `qrc_*`);
- ARM32 binaries or object files;
- IMX6ULL/FSL build configuration and sysroot paths;
- platform-specific device access such as AP3216C sysfs;
- the legacy QProcess child-application architecture;
- icons, music, or video whose license remains unverified.

Qt只负责display、interaction和state presentation。GUI线程不得：

- capture V4L2 frames；
- run inference；
- block on audio；
- block on RPMsg；
- perform long file or network operations。

数据路径：

```text
Camera: media_srv -> frame delivery -> cockpit_ui
Media:  cockpit_ui -> service IPC -> media_srv / audio_srv
Sensor: RT-Thread -> RPMsg -> vehicle_core -> cockpit_ui
AI:     infer_srv -> vehicle_core / IPC -> cockpit_ui
```

状态：NOT_IMPLEMENTED / CONTRACT_ONLY。参见根AGENTS.md和docs/tasks/ROADMAP.md。
