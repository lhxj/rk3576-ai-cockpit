# APPROVAL_GATE_BOARD_TEST：尚未开放

当前等级 C，用户禁止本轮部署。未来只有 contract check PASS、最终 RTOS/FIT/DTS/KO clean build、U-Boot/BL31/CON17/cache/分区/恢复全部闭合后，才可提出 **APPROVAL_GATE_BOARD_TEST**，列逐字文件、SHA256、板端目标、旧/新 hash、装载 PA/M0 entry、reserved-memory、vring、MBOX/link-id、签名策略、重启步骤、USB-TTL 观察点与逐命令 rollback。当前这些字段多为 `null`；**无获批 changeset、无可执行部署命令**。

未来首次测试范围严格限于 Linux 正常启动、M0 RT-Thread banner、`rk3576-m0-echo` 的 HELLO/HELLO_ACK 与 PING/PONG。不得接 MPU6050，不测试 Camera/Audio/NPU/双摄/压力。不在达到 D 后自动上板。
