# P006: Vehicle Core Foundation ExecPlan

状态：HOST_TESTED

## 目标与验收

在固定基线 `42c3c11` 上实现 Host-only `vehicle_core` 控制平面：结构化命令经同步校验后返回 ACK，由单一 control event loop 路由到可控 Mock adapter；只有最终 RESULT 才能提交成功业务状态。实现 canonical state、revision、服务健康、有限请求缓存、deadline/late-result 栅栏、Voice `CandidateAction` 桥、Host client 与有界队列，并通过可重复 Host 测试。

## 已有证据及未知项

- `libs/protocol` 已有显式 wire header、`Status/StatusCode`、request/session/epoch/deadline；仓库没有通用 `Result<T>`。
- `libs/ipc` 已有可关闭 `BoundedQueue` 与单 worker `InMemoryTransport`。
- `voice_srv` 已有 `SessionToken`、`CandidateAction`、`IVehicleCommandSink` 和 cancel/stale-session 栅栏。
- media、voice、RTOS、system 均无真实服务连接；本任务只使用来源明确为 Mock 的 adapter。
- Qt、ASR、AMP/RPMsg 与任何板端状态不在本分支集成，CORE-10 保持 PLANNED。

## 文件范围 / 负责人 / Agent

主控 Agent 单独修改本 worktree 的 `apps/vehicle_core/`、必要的 `libs/protocol` 消息类型扩展、`tests/unit/vehicle_core/`、根 CMake、P006/ROADMAP/manifest、`docs/architecture/VEHICLE_CORE.md` 与本计划。不得修改 UI、ASR 或 AMP worktree。

## 依赖 / 权限等级 / 板端独占

仅 L0：本地源码、文档、构建与 Host 测试。不 SSH、不访问设备节点、不安装依赖、不占用开发板、摄像头、声卡、NPU 或 AMP 资源。分支 `agent/vehicle-core-foundation` 的起点必须保持为 `42c3c11`。

## 分步操作

1. 冻结 VehicleCommand、ACK/RESULT、state、service registry 与 adapter 契约。
2. 实现单 writer event loop、有界 command/completion queue、clean start/stop/join。
3. 实现同步校验、路由、ACK、最终结果、状态提交与全量 snapshot event。
4. 实现有限去重缓存、deadline、late-result、boot epoch 与 voice cancel 栅栏。
5. 实现可控 SUCCESS/FAILURE/TIMEOUT Mock adapter、in-process/loopback client。
6. 实现现有 `CandidateAction -> IVehicleCommandSink -> VehicleCommand` 白名单桥。
7. 添加 Host 单元/集成测试，重复运行至少 20 轮；再运行全仓 Host CI。
8. 若工具链直接支持，独立运行 ASan/UBSan；TSan 为非阻塞检查并如实记录。

## 测试：命令、预期结果、实际结果、退出码

已执行：

- `cmake --preset host-debug && cmake --build --preset host-debug --parallel 2`
- `ctest --preset host-debug --output-on-failure`
- `ctest --test-dir build/host-debug --repeat until-fail:20 -R vehicle_core`
- `bash scripts/dev/host_ci.sh`
- 独立 sanitizer 构建（若当前 GCC/CMake 可直接启用）。

实际结果：

- `cmake --build --preset host-debug --parallel 2`：退出0。
- `ctest --preset host-debug --output-on-failure`：退出0，11/11通过。
- `ctest --test-dir build/host-debug --repeat until-fail:20 -R "vehicle_core_"`：
  退出0，四个测试各连续20轮通过。
- `bash scripts/dev/host_ci.sh`：退出0；CTest 11/11、Python unittest 6/6、shell
  syntax checks通过，输出 `HOST_SCAFFOLD_CHECKS_PASSED (not hardware validation)`。
- GCC 11.4 独立 ASan+UBSan build：退出0，Vehicle Core 4/4通过，启用 leak detection。
- GCC 11.4 独立 TSan build成功；运行4个测试均在测试逻辑前因 WSL runtime
  `unexpected memory mapping` 退出，记录为 `BLOCKED_BY_RUNTIME`，不记PASS。

首次 CTest 暴露新增 Vehicle message type 未纳入 `is_known_type()`，导致 loopback
编码被拒；扩展协议已修正并由后续全部测试覆盖。没有删除或放宽测试。

## 失败恢复与停止条件

构建或测试失败时只修正本任务文件并保留真实失败记录。若需要真实 service、Qt、RPMsg、板端部署或修改其他 worktree 才能继续，则停止该集成点并标记后续任务；不以 sleep、kill 或删除测试规避生命周期问题。

## 风险：资源、隐私、版本、第三方许可

Mock Online 仅表示 Host adapter 可接收命令，所有 state source 标记为 Mock，不等价于服务或硬件实测。请求缓存和队列均固定容量。没有第三方源码、模型、媒体或凭据进入本分支。

## 交接：提交、文档、待办

CORE-01至CORE-09均为 PASS（限Host/Mock边界），CORE-10保持PLANNED。
最终等级：`VEHICLE_CORE_HOST_PASS`。后续在独立 integration branch 合并
`IVehicleCoreClient` 与 UI 的 `VehicleCoreUiBackend`，再分别替换真实 service adapter
和跨进程 transport；本任务未开始这些集成，也未访问开发板。
