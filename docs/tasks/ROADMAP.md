# 开发队列

Pxxx是本地任务ID，不是GitHub真实Issue编号。初始状态只表示依赖与计划，非完工。
READINESS以实际环境/用户授权为准。主控更新此表与manifest，避免子Agent竞争。

| ID | 工作 | 初始状态 | 关键依赖 |
|---|---|---|---|
| [P000](P000.md) | 主机与仓库启动包验收 | READY | — |
| [P001](P001.md) | 板端只读盘点与安全测试入口 | READY | — |
| [P002](P002.md) | RK3576 AMP/RPMsg 实际SDK调查 | READY | — |
| [P003](P003.md) | CameraCapture 与 Frame 生命周期 | PLANNED | P000, P001 |
| [P004](P004.md) | 基于功能需求与参考交互重新实现RK3576 cockpit_ui | IN_PROGRESS（UI foundation已板端验证） | P000, P001 |
| [P005](P005.md) | 基于已审查参考实现，建立自有 Voice/AI 架构 | IN_PROGRESS（Host骨架） | P000；参考审查已完成 |
| [P006](P006.md) | vehicle_core与消息契约 | HOST_TESTED_FOUNDATION | P000 |
| [P007](P007.md) | 单摄MPP编码与Wi-Fi RTSP | PLANNED | P003, P006 |
| [P008](P008.md) | AMP最小构建与受控实机验证 | BLOCKED | P002 |
| [P009](P009.md) | 新线缆与双摄并发验收 | BLOCKED | P001, P003 |
| [P010](P010.md) | MPU6050真采样与模拟控制 | BLOCKED | P006, P008 |
| [P011](P011.md) | 整机联动与性能/稳定性 | PLANNED | P003, P004, P005, P006 |

## 顺序

第一轮P000/P001，并行开始P002的文档/本地SDK盘点；不要先装大量依赖或碰boot。
之后在host层并行P003/P004/P005/P006；每轮最多3个任务、物理板只有1个使用者。
P008需要部署审批；P009等新线；P010等真实AMP与外设归属；这些不阻塞其他host工作。

### P005 Voice/AI 工作包

LLM_Voice_Flow 审查commit `be82e87cc334ae6e222f83f7555531d1ddebaa8b` 的正式定位是
`SOURCE_REFERENCE / REFERENCE_ONLY`。本轮只实现前两项和后三个服务的接口骨架；
历史参考源码存在不代表产品 ASR、TTS、RKLLM 已实现。

| 顺序 | 工作包 | 内容 | 状态 |
|---|---|---|---|
| 1 | VOICE-00 | Voice/AI foundation interfaces | HOST_TESTED |
| 2 | VOICE-01 | IPC/session protocol | HOST_TESTED |
| 3 | VOICE-02 | audio_srv device abstraction | HOST_TESTED_INTERFACE_ONLY |
| 4 | VOICE-03 | voice_srv ASR/TTS session abstraction | HOST_TESTED_INTERFACE_ONLY |
| 5 | VOICE-04 | infer_srv language/vision backend abstraction | HOST_TESTED_INTERFACE_ONLY |
| 6 | VOICE-05 | file-based ASR backend integration | PLANNED |
| 7 | VOICE-06 | file-based TTS backend integration | PLANNED |
| 8 | VOICE-07 | RKLLM backend integration | PLANNED |
| 9 | VOICE-08 | deterministic vehicle intent routing | PLANNED |
| 10 | VOICE-09 | live microphone/playback integration | PLANNED |
| 11 | VOICE-10 | voice + vision combined resource validation | PLANNED |

### P004 UI工作包

P004不再包含“直接移植IMX6ULL Qt工程”。参考归档固定为
`UI_REFERENCE_ONLY`，所有页面按当前接口 `REIMPLEMENT`：

| 顺序 | 工作包 | 内容 |
|---|---|---|
| 1 | UI-01 | Qt shell / 800x480基本框架 |
| 2 | UI-02 | Home / Navigation |
| 3 | UI-03 | System status model |
| 4 | UI-04 | Camera page |
| 5 | UI-05 | Media / Music / Video page |
| 6 | UI-06 | Vehicle / Sensor page |
| 7 | UI-07 | AI interaction page |
| 8 | UI-08 | Monitor page |
| 9 | UI-09 | Settings page |
| 10 | UI-10 | Touch / full-screen board verification |

UI-01至UI-09允许按依赖逐步Host推进；UI-10需要板端测试条件和相应授权。
2026-10-01 foundation进展：UI-01为`BOARD_STARTUP_TESTED`；UI-03为
`MOCK_TESTED`；UI-02、UI-04至UI-09为`PARTIAL`。UI-10已证明X11全屏进程
启动/退出，但视觉与实体触摸未验收，仍为`PARTIAL`。页面骨架不提升
Camera/Media/AI/Sensor业务功能状态。

### P006 Vehicle Core工作包

| 顺序 | 工作包 | 内容 | 状态 |
|---|---|---|---|
| 1 | CORE-01 | Command model | PASS（HOST_TESTED） |
| 2 | CORE-02 | Command validation | PASS（HOST_TESTED） |
| 3 | CORE-03 | Command routing | PASS（HOST Mock） |
| 4 | CORE-04 | ACK/RESULT lifecycle | PASS（HOST_TESTED） |
| 5 | CORE-05 | Canonical state | PASS（HOST_TESTED） |
| 6 | CORE-06 | Service registry | PASS（HOST Mock） |
| 7 | CORE-07 | Idempotency/deadline | PASS（HOST_TESTED） |
| 8 | CORE-08 | Voice CandidateAction bridge | PASS（HOST_TESTED） |
| 9 | CORE-09 | Host IPC client | PASS（InMemory loopback） |
| 10 | CORE-10 | Integration with cockpit_ui | PLANNED（独立integration branch） |

P006 的 PASS 只表示控制面 Host foundation，不能提升 Camera、Recording、Voice、
RTOS 或整车功能状态。真实 adapter、跨进程 transport 和 Qt 连接仍为后续任务。

## 完成等级

PLANNED → IN_PROGRESS → HOST_TESTED / BOARD_TESTED → REVIEW_READY → 用户合并。
BLOCKED记录原因与解除条件。Mock只标MOCK_TESTED，不能替代BOARD_TESTED。
