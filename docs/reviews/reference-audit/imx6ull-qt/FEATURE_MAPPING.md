# IMX6ULL参考功能到RK3576架构映射

下表是需求与职责映射，不是完成状态。除可选项外，当前实现状态均为
`NOT_IMPLEMENTED`，须按各自任务和实板证据验收。

| 旧功能/模式 | RK3576新功能 | 新负责模块 | 数据来源/路径 | 处理方式 | 验收方式 |
|---|---|---|---|---|---|
| 主菜单/大触控按钮 | Home / Navigation | cockpit_ui | vehicle_core与各服务状态 | `REIMPLEMENT` | 单shell导航Host测试；服务离线可进入；板端全屏触控 |
| 传感器 | Vehicle / Sensor | RT-Thread sensor_task、rpmsg_srv、vehicle_core、cockpit_ui | `MPU6050 -> RT-Thread -> RPMsg -> vehicle_core -> cockpit_ui` | `REIMPLEMENT` | 真采样序列、协议校验、UI值/离线状态与板端证据 |
| 音乐 | Media / Music | cockpit_ui、media_srv、audio_srv | `cockpit_ui -> service IPC -> media_srv/audio_srv` | `REIMPLEMENT` | 自有授权媒体；播放/暂停/曲目/进度/错误状态；声卡不竞争 |
| 视频 | Media / Video | cockpit_ui、media_srv | `cockpit_ui -> service IPC -> media_srv/decoder` | `REIMPLEMENT` | 自有授权媒体；播放/暂停/进度/全屏；与Camera资源策略明确 |
| 天气 | 可选联网功能 | cockpit_ui与未来network/data adapter | 经过配置的天气服务或缓存 | `OPTIONAL / LATER` | 无网时明确不可用/缓存时间；未实现时不显示在线数据 |
| 地图 | 模拟导航/地图展示 | cockpit_ui与未来map adapter | 无GNSS时仅模拟路线或静态地图 | `OPTIONAL / SIMULATED` | 页面显著标记SIMULATED；不声称实时定位导航 |
| QProcess启动三个子程序 | 单shell页面路由与服务IPC | cockpit_ui、vehicle_core、media_srv、audio_srv | `cockpit_ui -> service IPC -> service` | `DO_NOT_REUSE` | 切页不启动旧ELF；服务失败有RESULT/状态；退出可控 |
| AP3216C sysfs | MPU6050跨域数据链 | RT-Thread、rpmsg_srv、vehicle_core | `MPU6050 -> RT-Thread -> RPMsg -> Linux` | `DO_NOT_REUSE` | 不访问`/sys/class/misc/ap3216c/`；实际I2C/IRQ归属经板级验证 |
| 旧音乐/视频控制图标 | RK3576自有视觉资源 | cockpit_ui | 自有制作或许可明确的资源包 | 当前`DO_NOT_REUSE` | 每项资源有来源、许可和可分发记录后才可入库 |
| 旧ARM32应用与生成文件 | RK3576 AArch64新构建 | cockpit_ui及目标服务 | 当前AArch64工具链/sysroot/Qt | `DO_NOT_REUSE` | 源码可审查；目标ABI、Qt平台插件和板端启动分别验证 |

Camera、AI、Monitor和Settings是当前产品架构新增/扩展页面，不从旧RAR推导：

| 新页面 | 主要模块 | 核心接口 | 当前状态 |
|---|---|---|---|
| Camera | cockpit_ui + media_srv | frame delivery与控制RESULT | `NOT_IMPLEMENTED` |
| AI | cockpit_ui + infer_srv + vehicle_core | 结构化推理/会话结果 | `NOT_IMPLEMENTED` |
| Monitor | cockpit_ui + monitor/vehicle_core | 有来源和时间戳的状态指标 | `NOT_IMPLEMENTED` |
| Settings | cockpit_ui + config/state API | 异步校验、应用结果与错误 | `NOT_IMPLEMENTED` |
