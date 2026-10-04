# Bounded coexistence health fixture

TEST_ONLY，不是正式VehicleCore/RTOS业务协议，不替换冻结echo KO。
原HELLO/HELLO_ACK和4-byte PING/PONG保持；同服务/endpoint/transport/resource。
单peer、单in-flight；1秒间隔、3秒回复deadline、12分钟总窗口；非阻塞trysend。
超时/未知/重复/late reply/发送失败即终止，不重试，不把旧PONG计入后续请求。
只读`/sys/module/rk3576_amp_health_test/parameters/health_status`统计Linux往返，
不推断未同步跨域时钟的单向延迟。读此文件不触发共享内存访问或发送。
M0必须使用独立15分钟/1024接收测试变体；原180秒/PONG退出固件不可用于此KO。
remove同步cancel worker；测试会话结束按批准的冷恢复流程，不同会话重启M0。
目标KO只构建到新Host输出；部署/加载需要独立精确审批，禁止自动覆盖冻结包。
