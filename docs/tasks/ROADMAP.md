# 开发队列

Pxxx是本地任务ID，不是GitHub真实Issue编号。初始状态只表示依赖与计划，非完工。
READINESS以实际环境/用户授权为准。主控更新此表与manifest，避免子Agent竞争。

| ID | 工作 | 初始状态 | 关键依赖 |
|---|---|---|---|
| [P000](P000.md) | 主机与仓库启动包验收 | READY | — |
| [P001](P001.md) | 板端只读盘点与安全测试入口 | READY | — |
| [P002](P002.md) | RK3576 AMP/RPMsg 实际SDK调查 | SOURCE_REVIEWED_MINIMAL_CHAIN | — |
| [P003](P003.md) | CameraCapture 与 Frame 生命周期 | BOARD_TESTED_CAM0_INTEGRATION（T1-T8通过） | P000, P001 |
| [P004](P004.md) | 基于功能需求与参考交互重新实现RK3576 cockpit_ui | IN_PROGRESS（UI/Core Host Mock集成通过） | P000, P001 |
| [P005](P005.md) | 基于已审查参考实现，建立自有 Voice/AI 架构 | IN_PROGRESS（Voice Runtime板端编排PASS；TTS/RKLLM等未完成） | P000；参考审查已完成 |
| [P006](P006.md) | vehicle_core与消息契约 | HOST_TESTED_UI_INTEGRATION | P000 |
| [P007](P007.md) | 单摄MPP编码与Wi-Fi RTSP | MEDIA_CAM0_RTSP_PASS | P003, P006 |
| [P008](P008.md) | 已实测AMP/RPMsg最小链集成整理 | BOARD_PASS_MINIMAL / FROZEN | P002 |
| [P009](P009.md) | 新线缆与双摄并发验收 | BLOCKED | P001, P003 |
| [P010](P010.md) | MPU6050 RTOS / RPMsg业务集成 | HOST_PASS / RTOS_SENSOR_PASS / RPMSG_SENSOR_PASS（T4退出/默认冷恢复PASS；UI/共存NOT_RUN） | P006, P008 |
| [P011](P011.md) | 整机联动与性能/稳定性 | PLANNED | P003, P004, P005, P006 |

## 顺序

第一轮P000/P001，并行开始P002的文档/本地SDK盘点；不要先装大量依赖或碰boot。
之后在host层并行P003/P004/P005/P006；每轮最多3个任务、物理板只有1个使用者。
P008最小链已实测并冻结；P010业务Host闭环已通过，真实RTOS采样/RPMsg/UI与共存验收仍NOT_RUN，不能由echo或Fake证据替代。
P011本轮仅收敛两个权威tip并核验共存条件，禁止新增业务或自动修改启动配置。

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
| 6 | VOICE-05 | file-based ASR backend integration | BOARD_FILE_RECOGNITION_PASS（RK3576文件输入；模型发行许可待核） |
| 7 | VOICE-06 | file-based TTS backend integration | PLANNED |
| 8 | VOICE-07 | RKLLM backend integration | PLANNED |
| 9 | VOICE-08 | deterministic vehicle intent routing | BOARD_TESTED_SYNTHETIC_ASR_FINAL_CAM0：合成FINAL→Core→真实CAM0；非实时语音 |
| 10 | VOICE-09 | live microphone/playback integration | LIVE_MIC_ASR_PASS；playback仍为PLANNED |
| 11 | VOICE-10 | voice + vision combined resource validation | PLANNED |
| 12 | VOICE-11 | Voice Runtime orchestration | VOICE_RUNTIME_ORCHESTRATION_PASS（真实runtime启停 + synthetic CAM0/MPP；非实时语音控制） |

### RKNN Vision工作包

| ID | 内容 | 状态 |
|---|---|---|
| VISION-01 | Runtime/driver/model/许可闭合 | PASS（外部Rockchip受限交付；不进Git） |
| VISION-02 | 官方固定图NPU推理 | PASS |
| VISION-03 | 有界latest-frame-wins运行时与epoch栅栏 | HOST_SANITIZED |
| VISION-04 | 单CAM0真实连续推理 | PASS |
| VISION-05 | 五分钟CAM0+RKNN稳定性 | PASS |
| VISION-06 | Preview+Vision共享frame | PASS |
| VISION-07 | Preview+Recording+RTSP+Vision | PASS（单CAM0/单MPP encoder） |
| VISION-08 | 真实RKNN状态到Qt AI页 | BOARD_RUNTIME_TESTED；未新增人工触摸验收 |

当前等级`VISION_RKNN_CAM0_PASS`只覆盖MobileNetV1分类基线。VOICE-10仍为PLANNED，
因为本轮组合负载没有同时运行真实Voice Runtime。

VOICE-05 板端阶段的五个验收项见 `docs/bringup/asr/`：

| ID | 内容 | 状态 |
|---|---|---|
| ASR-T1 | v1.11.3 AArch64依赖及ABI闭合 | PASS |
| ASR-T2 | RK3576原生构建、ELF启动 | PASS |
| ASR-T3 | 相同模型/WAV的板端文件识别 | PASS |
| ASR-T4 | 板端取消、重复识别与异常退出 | PASS |
| ASR-T5 | 板端CPU/内存/温度资源基线 | PASS（短时文件测试） |

VOICE-09 的实时麦克风 ASR 独立验收如下。它不代表播放、VAD、唤醒词、命令执行或语音助手完成。

| ID | 内容 | 状态 |
|---|---|---|
| ASR-L1 | `audio_srv` ALSA capture，实际格式协商 | PASS（T0/T1） |
| ASR-L2 | 有界 PCM 队列及溢出显式取消 | HOST_TESTED；板端正常路径无溢出 |
| ASR-L3 | 板载麦克风 → Sherpa → 非空 `ASR_FINAL` | PASS（T2） |
| ASR-L4 | 实板取消及新session恢复 | PASS（T3/T4） |
| ASR-L5 | 同进程3次session和短时资源基线 | PASS（T5） |
| ASR-L6 | 5句×3次人工命令样本 | PLANNED（本轮未测） |

VOICE-09 后续 VAD 自动分句任务仅识别语音边界与文本，不触发命令；本轮不要求新的真人准确率测试。

| ID | 内容 | 状态 |
|---|---|---|
| VAD-01 | v1.11.3 API、模型版本和许可审查 | PASS |
| VAD-02 | `IVadBackend` 结构化事件 | HOST_TESTED |
| VAD-03 | 有界 300 ms pre-roll | HOST_TESTED |
| VAD-04 | utterance 状态机 | HOST_TESTED |
| VAD-05 | 每句ASR stream / FINAL | BOARD_FILE_FIXTURE_PASS |
| VAD-06 | cancel后Listening与新句 | HOST_TESTED；板端实时取消未单独实测 |
| VAD-07 | 单句/双句/噪声/短停顿 fixture | HOST_TESTED；板端单句/双句PASS |
| VAD-08 | RK3576 VAD 链路 | BOARD_FILE_FIXTURE_PASS；实时自动结束待验证 |
| VAD-09 | 数百utterance稳定性 | BOARD_200_FIXTURE_PASS；实时长稳待验证 |

VOICE-08 已将确定性文本路由与 `vehicle_core`、真实MediaService和CAM0合并验证。
它只使用合成 `ASR_FINAL`；实时麦克风/VAD尚未接入该控制链。

| ID | 内容 | 状态 |
|---|---|---|
| INTENT-01 | 保守UTF-8文本规范化 | HOST_TESTED |
| INTENT-02 | 显式白名单与冲突检查 | HOST_TESTED（10规则、23 alias、0冲突） |
| INTENT-03 | 否定拒绝 | HOST_TESTED |
| INTENT-04 | 多意图拒绝 | HOST_TESTED |
| INTENT-05 | typed CandidateAction映射 | HOST_TESTED |
| INTENT-06 | session/generation/deadline校验 | HOST_TESTED |
| INTENT-07 | `IVehicleCommandSink` Mock桥接 | HOST_TESTED；不执行硬件 |
| INTENT-08 | Host单元、fixture与回归 | HOST_TESTED |
| INTENT-09 | 与Vehicle Core控制面适配 | HOST_TESTED_MOCK（独立整合分支；无真实服务） |
| INTENT-10 | OPEN/CLOSE经Core控制真实CAM0 | BOARD_TESTED_SYNTHETIC_ASR_FINAL_CAM0 |

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
`HOST_TESTED_MOCK_INTEGRATION`；UI-02、UI-04至UI-09仍只完成页面与Mock控制链。
用户已确认foundation二进制在800x480实屏上的布局和完整触摸导航，因此UI-10记为
`BOARD_TOUCH_TESTED_FOUNDATION`；新的UI/Core integration二进制仍需单独板端回归。
页面和Mock控制链不提升Camera/Media/AI/Sensor真实业务功能状态。

### P003 CAM0 Media工作包

| 顺序 | 工作包 | 内容 | 状态 |
|---|---|---|---|
| 1 | MEDIA-01 | CAM0 device resolution | PASS（BOARD_READONLY） |
| 2 | MEDIA-02 | V4L2 MPLANE capture backend | PASS（BOARD_TESTED） |
| 3 | MEDIA-03 | owned frame / MMAP lifetime | PASS（HOST_SANITIZED + BOARD_CAPTURE） |
| 4 | MEDIA-04 | bounded preview path | PASS（HOST + BOARD_RUNTIME） |
| 5 | MEDIA-05 | snapshot | PASS（BOARD_TESTED_PPM） |
| 6 | MEDIA-06 | RealMediaServiceAdapter | PASS（HOST + BOARD_TESTED） |
| 7 | MEDIA-07 | Qt preview | PASS（BOARD_TOUCH_TESTED） |
| 8 | MEDIA-08 | board touch integration | PASS（USER_CONFIRMED） |
| 9 | MEDIA-09 | five-minute stability | PASS（BOARD_BOUNDED_300S） |
| 10 | MEDIA-10 | Annex-B H.264 recording | PASS（MEDIA_CAM0_RECORDING_PASS） |
| 11 | MEDIA-11 | RTSP | PASS（MEDIA_CAM0_RTSP_PASS） |
| 12 | MEDIA-12 | CAM1 | BLOCKED / OUT_OF_SCOPE |

MEDIA-10已完成真实MPP录像验收。MEDIA-11完成Host协议/sanitizer、RK3576
原生构建、实际客户端识别/解码、重连、五分钟Preview+RTSP、五分钟
Preview+Recording+RTSP及20轮启停，等级为`MEDIA_CAM0_RTSP_PASS`。
MEDIA-12 CAM1仍为BLOCKED/OUT_OF_SCOPE，不因CAM0 RTSP通过而提升状态。

### UI + Vehicle Core integration工作包

| 顺序 | 工作包 | 内容 | 状态 |
|---|---|---|---|
| 1 | UI-CORE-01 | VehicleCoreUiBackend | HOST_TESTED |
| 2 | UI-CORE-02 | Command mapping | HOST_TESTED |
| 3 | UI-CORE-03 | ACK/RESULT UI lifecycle | HOST_TESTED |
| 4 | UI-CORE-04 | Snapshot mapping | HOST_TESTED |
| 5 | UI-CORE-05 | Revision filtering | HOST_TESTED |
| 6 | UI-CORE-06 | Qt thread handoff | HOST_TESTED |
| 7 | UI-CORE-07 | Host integration tests | HOST_TESTED |
| 8 | UI-CORE-08 | RK3576 board UI/core validation | PASS（BOARD_TOUCH_TESTED_MOCK_INTEGRATION） |
| 9 | UI-CORE-09 | IPC process separation | PLANNED |

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
| 10 | CORE-10 | Integration with cockpit_ui | HOST_TESTED_MOCK_INTEGRATION |

P006 的 PASS 只表示控制面 Host foundation，不能提升 Camera、Recording、Voice、
RTOS 或整车功能状态。真实 adapter、跨进程 transport 和 Qt 连接仍为后续任务。

## 完成等级

PLANNED → IN_PROGRESS → HOST_TESTED / BOARD_TESTED → REVIEW_READY → 用户合并。
BLOCKED记录原因与解除条件。Mock只标MOCK_TESTED，不能替代BOARD_TESTED。

## 2026-10-04 System integration convergence

唯一输入为 Application `8585c66d27fa65ef11a6531b95656acfa3dc9e8b` 和 AMP/RPMsg
`ec56833276d31df1e1ce8d36741a552042e2b6ca`；分支 `agent/system-integration`。
普通双亲 merge 保留历史，不重新逐个合并旧开发分支。
P011原完整目标仍为PLANNED；本轮子里程碑/Host与Board结果见
[SYSTEM_INTEGRATION.md](../architecture/SYSTEM_INTEGRATION.md)。
持续echo、MPU6050及VehicleCore/RTOS业务不通过此merge升级状态。

本轮最终子里程碑：**SYSTEM_INTEGRATION_HOST_PASS**。Board只读确认默认#8/无stage/无RPMsg设备，
全系统共存BLOCKED_NOT_RUN；没有新功能或启动配置修改。P011完整目标仍PLANNED。

## 2026-10-05 MPU6050正式业务首轮

P010从最新system tip6e0aa7c派生，第一轮审查/计划完成，候选I2C9_M1；
接线电平/Pin1与ownership尚未闭合，新业务未实现。基线Host CI通过只算回归，
不授予HOST_PASS/RTOS_SENSOR_PASS/RPMSG_SENSOR_PASS/UI_SENSOR_PASS。
后续按[ExecPlan](../plans/mpu6050-rtos-rpmsg.md)进入Host实现→独立构建→精确审批→T0–T6，
保持原AMP/health/默认恢复路径，达到本任务结果后停止，不扩展其他业务。

2026-10-05续步：P010 Host ownership配置修复/诊断包构建完成；独立I2C_RESOURCE_PROBE_V1经主控审核并被动安装，尚未执行M0定向读。用户最新授权不再额外审批，执行仍由主控持锁/有界/人工coldcycle/恢复核验，不把此准备升级传感器PASS。

2026-10-05历史记录：P010接线前里程碑完成。attempt4取得M0十地址白名单只读访问与派生Linux ownership实测证据、health正常/KO卸载/默认冷恢复25hash PASS；WIRING_READY_FOR_USER，等待用户断电接线确认。寄存器写、I2C交易、IRQ、WHO_AM_I及四项sensor等级仍NOT_RUN；不继续额外诊断/重启。见[实际结果](../bringup/mpu6050/RESOURCE_PROBE_BOARD_RESULT.json)。

2026-10-05历史记录：用户断电接线USER_CONFIRMED；driver/sampler Host与独立原生/FIT子里程碑通过但NOT_DEPLOYABLE。下一独立里程碑实现sensor service/codec/epoch/订阅，完整业务四等级保持NOT_RUN。


### 2026-10-05 Sensor service Host子里程碑（历史记录）

公共BE codec、独立0x3005 sensor endpoint、单订阅/租约/latest背压与READY worker、同health transport双owner有界退出完成；实际fixture sanitizer/native SCons/FIT验签及host_ci31/77/5通过。详见[SERVICE_HOST_RESULT](../bringup/mpu6050/SERVICE_HOST_RESULT.md)。用户断电接线USER_CONFIRMED；配套Linux sensor KO/rpmsg_srv/Core/Qt仍待实现，NOT_DEPLOYABLE，完整业务四等级与最终集成NOT_RUN，无本轮板端操作。下一步完成Linux桥及canonical/UI Host集成，不退回旧冻结tip或重复权限诊断。


### 2026-10-05 Linux/Core/Qt Host闭环（最新）

实际KO有界接口/ready ioctl与CCF持有、rpmsg_srv QUERY/epoch/订阅/freshness/正常退出、Core typed canonical校验与Qt六轴/芯片温度/状态/age/source完成，FakeI2C→实际driver/service→C++worker→Core→queuedQt闭环通过。host_ci34/79/5及适用sanitizer PASS：HOST_PASS，实板三个sensor等级与最终集成NOT_RUN。配套KO-v4精确vermagic/导出符号通过，MODVERSIONS=n/CRC=N/A；APPROVAL_PACKET_NOT_READY / NOT_DEPLOYABLE，ARM应用未构建，正式sensor包未就绪。[结果/限制](../bringup/mpu6050/LINUX_CORE_QT_HOST_RESULT.md)。用户接线已确认，主控一次L1连接超时、未登录，当前启动身份UNKNOWN/UNREACHABLE，不沿用历史default快照。下步正式包须补一次WHO/关键读回与有限M0raw观测；本轮没有子代理板端操作。
