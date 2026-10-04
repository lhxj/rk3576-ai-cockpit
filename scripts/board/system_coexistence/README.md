# System coexistence test fixture

仅测试：SI_HEALTH_V1。产品apps/libs、冻结rtos/platform/patches均不修改。
不得把此目录当VehicleCore↔RTOS正式业务实现，不常驻运行，不自动修改boot。

## 组件

- `persistent-echo.patch`：应用于冻结C源码的**独立副本**，仅重复原PING/PONG，
  15分钟/1024请求截止。标准diff空白context不是代码尾随空白。
- `amp_health_linux/`：配对kernel ABI的独立健康KO及共享状态机Host测试；
  12分钟、单in-flight、零重试；sysfs读取不触发新命令。
- `install_health_fixture.py`：精确批准包manifest/hash、7文件独立目录、
  拒绝覆盖/链接/未知文件；只被动部署，不启动M0或加载KO。
- `application_probe_target.cmake`：通过CMAKE_PROJECT_INCLUDE注入test-only target，
  保留既有35个板端CTest。需要已有Qt/MPP/RKNN/ALSA/Sherpa依赖与模型。
- `system_coexistence_probe.cpp`：单Qt shell/单MediaService/单编码器，逐级启动，
  300–360秒有界负载，正常停全部消费者并再核查真实PONG。ASR无synthetic FINAL。
- `run_coexistence.py`：本次固定测试目录/资产路径的控制器，运行35项CTest、
  从实际GNOME进程确认X11授权路径、先验证CIF sensor→ISP mainpath图，
  启动独占驱动和已有ffmpeg UDP回环客户端，每5秒采样，单日志上限2MiB。

## 已验证构建边界

Host：`bash scripts/dev/host_ci.sh`，31 CTest + 47 Python；状态机另跑sanitizer。
RK3576：现有CMake启用V4L2/MPP/ALSA/Sherpa/RKNN，附加
`-DCMAKE_PROJECT_INCLUDE=<source>/scripts/board/system_coexistence/application_probe_target.cmake`。
现有模型/库路径见审批包与BUILD_MATRIX，不下载或升级依赖。
M0采用pinned RT-Thread/HAL副本与原配置的SCons入口；KO采用paired kernel prepared
headers独立O/M目录；signed FIT必须使用已有合法vendor签名/核验入口，产物不进Git。

## 运行门与退出

任何再次部署/冷启动/KO加载必须取得新的精确L3窗口批准，不能因源码存在直接执行。
本次运行已经结束并冷恢复，控制器固化的输出目录会拒绝覆盖；再次测试应另建审批和证据目录。
preflight仍保留启动<100秒限制；控制器要求健康窗口elapsed<180秒、回归后<270秒。
只有真实回包、逐级成功才运行负载；失败停止继续加载，仅管理自己创建的PID。
外部timeout不能保证中断内核D状态；异常情况下保存串口/日志，用户物理恢复，
不能把强杀或断电恢复写成clean Application shutdown。

SIGTERM/SIGKILL清理属于失败兜底，不是成功路径；成功路径由既有服务正常STOP与RAII释放。
控制器的cat级FD检查由本次主控另以root只读fuser复核，证据见Board结果。
本次冷恢复采用人工完整断电/默认启动，不rmmod重试或同会话重启M0。

结果见`docs/bringup/system-integration/BOARD_COEXISTENCE_RESULT.md`与`RESOURCE_RESULT.md`。
