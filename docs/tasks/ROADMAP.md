# 开发队列

Pxxx是本地任务ID，不是GitHub真实Issue编号。初始状态只表示依赖与计划，非完工。
READINESS以实际环境/用户授权为准。主控更新此表与manifest，避免子Agent竞争。

| ID | 工作 | 初始状态 | 关键依赖 |
|---|---|---|---|
| [P000](P000.md) | 主机与仓库启动包验收 | READY | — |
| [P001](P001.md) | 板端只读盘点与安全测试入口 | READY | — |
| [P002](P002.md) | RK3576 AMP/RPMsg 实际SDK调查 | READY | — |
| [P003](P003.md) | CameraCapture 与 Frame 生命周期 | PLANNED | P000, P001 |
| [P004](P004.md) | 基于功能需求与参考交互重新实现RK3576 cockpit_ui | PLANNED | P000, P001 |
| [P005](P005.md) | Voice Flow 与模型依赖盘点 | PLANNED | P000 |
| [P006](P006.md) | vehicle_core与消息契约 | PLANNED | P000 |
| [P007](P007.md) | 单摄MPP编码与Wi-Fi RTSP | PLANNED | P003, P006 |
| [P008](P008.md) | AMP最小构建与受控实机验证 | BLOCKED | P002 |
| [P009](P009.md) | 新线缆与双摄并发验收 | BLOCKED | P001, P003 |
| [P010](P010.md) | MPU6050真采样与模拟控制 | BLOCKED | P006, P008 |
| [P011](P011.md) | 整机联动与性能/稳定性 | PLANNED | P003, P004, P005, P006 |

## 顺序

第一轮P000/P001，并行开始P002的文档/本地SDK盘点；不要先装大量依赖或碰boot。
之后在host层并行P003/P004/P005/P006；每轮最多3个任务、物理板只有1个使用者。
P008需要部署审批；P009等新线；P010等真实AMP与外设归属；这些不阻塞其他host工作。

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
工作包存在不表示已实现，当前均为PLANNED。

## 完成等级

PLANNED → IN_PROGRESS → HOST_TESTED / BOARD_TESTED → REVIEW_READY → 用户合并。
BLOCKED记录原因与解除条件。Mock只标MOCK_TESTED，不能替代BOARD_TESTED。
