# IMX6ULL Qt reference audit update

记录日期：2026-10-01。该记录同步仓库外已完成的静态审查，不复制参考归档
或其中任何内容。

| 字段 | 结论 |
|---|---|
| Archive | `build-QTMenu-IMX6U_rsync-Debug.rar` |
| SHA256 | `92e571eaeb171be6dcd73c2db8e895223f2fdabd3ee863b8dc66de9f5e39462c` |
| Availability | `LOCAL_ARCHIVE_AVAILABLE` |
| Classification | Qt Creator/qmake `SHADOW_BUILD` |
| Source completeness | `INCOMPLETE` |
| Qt source availability | `NOT_AVAILABLE` |
| UI reference value | `USEFUL` |
| Reference Role | `UI_REFERENCE_ONLY` |
| Migration | `REIMPLEMENT` |
| License | `LICENSE_UNVERIFIED` |

## 静态审查事实

- 缺少 `QTMenu.pro`、原始 `main*.cpp/.h`、`menubutton.cpp/.h`、
  `mainwindow.ui`、原始qrc及QMusicPlayer/QVideo/senor源码。
- 包内源码形式文件是uic/moc/rcc生成物；另有ARM32 ELF、ARM32 object、
  旧Makefile、`.qmake.stash` 和演示媒体。
- 旧平台是IMX6ULL/Cortex-A7/ARMv7 hard-float/FSL Yocto/Qt 5.12.9。
- 可确认五个主页入口：传感器、音乐、视频、天气、地图；音乐/视频/传感器
  只恢复到交互和功能轮廓。
- `QTMenu` 通过QProcess启动 `./QMusicPlayer`、`./QVideo`、`./senor`。
- `senor` 读取 `/sys/class/misc/ap3216c/`，与当前MPU6050/RT-Thread/RPMsg
  方案不一致。

完整静态证据保存在仓库外
`/home/ywx/rk3576-work/reference/imx6ull-qt/analysis/`。该目录是参考资料，
不是产品源码，不加入本Git仓库。

## 当前可用价值

只参考菜单组织、大屏触控入口、音乐/视频控制项和传感器图表表现。它不证明
当前 `cockpit_ui`、media、sensor或任何板端功能已经实现，也不能用于提升
`docs/architecture/FEATURE_MATRIX.md` 的功能状态。

不复用ARM32 ELF、object、Qt生成文件、旧构建配置、FSL sysroot、设备节点、
AP3216C sysfs、QProcess子应用架构、图标或演示媒体。许可状态继续保持
`LICENSE_UNVERIFIED`；CSDN文章或Qt自身许可不自动覆盖这些内容。

## 与历史架构审查的关系

`docs/reviews/architecture-audit/` 在更早时点记录本地reference目录为空、RAR
尚未取得。那是审查当时的真实证据，历史报告保持原文。本目录记录后来取得
归档后的增量事实，不能反向改写旧报告。

后续决策见 `MIGRATION_DECISION.md`，旧功能到新模块映射见
`FEATURE_MAPPING.md`。
