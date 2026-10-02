# RK3576 AI Cockpit Architecture Documents

本目录从十个视角描述《基于鲁班猫3 RK3576 的 AMP AI 车载多媒体座舱》：

1. `SYSTEM.md`（仓库已有）：系统边界、运行域、模块与总体数据流。
2. `INTERFACES.md`（仓库已有）：控制、图像、音频、RPMsg 等接口约束。
3. `PRODUCT_FUNCTIONS.md`：**完整产品功能定义**，回答“最终系统到底要做什么”。
4. `RUNTIME_SCENARIOS.md`：典型运行流程、模块协作与降级路径。
5. `FEATURE_MATRIX.md`：功能 → 模块 → 硬件 → 验证方式 → 当前状态的追踪矩阵。
6. `VOICE_AI_FOUNDATION.md`：Voice/AI Host接口、协议、所有权和后续backend边界。
7. `VEHICLE_CORE.md`：Vehicle Core Host控制面、状态与生命周期契约。
8. `COCKPIT_UI.md`：当前自研Qt shell、状态模型、Mock backend与GUI线程边界。
9. `UI_VEHICLE_CORE_INTEGRATION.md`：Qt与Vehicle Core的命令、ACK/RESULT、状态和线程边界。
10. `VOICE_RUNTIME_ORCHESTRATION.md`：ALSA/VAD/ASR、Intent Dispatcher与Core之间的运行时所有权和停止栅栏。

## 文档职责

- 架构文档描述“目标设计”和“职责边界”，不应把目标当成已经完成。
- `../STATUS.md` 是当前实板事实的唯一主要状态来源。
- `../bringup/` 保存 Bring-up 证据与已知问题。
- `../tasks/` 和 `../plans/` 描述下一步开发任务，不替代功能规格。

功能实现后，应同步更新 `FEATURE_MATRIX.md` 与 `../STATUS.md`；仅修改 README 或架构图不能视为功能完成。

## cockpit_ui 与 IMX6ULL 参考边界

`cockpit_ui` 是面向 RK3576 的新实现。2026-10-01 静态审查确认 IMX6ULL RAR
只是 qmake Shadow Build，正式角色为 `UI_REFERENCE_ONLY`，迁移策略为
`REIMPLEMENT`。它只提供页面组织、触控交互、媒体控制项和传感器展示参考，
不提供可迁移源码基线。

目标页面结构为：

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

采用单一主 Qt shell，通过 `QStackedWidget` 或统一页面路由切换页面。禁止把
旧工程 `QProcess` 启动三个 ARM32 GUI ELF 的方式作为最终架构。参考审查记录
位于 `../reviews/reference-audit/imx6ull-qt/`。
