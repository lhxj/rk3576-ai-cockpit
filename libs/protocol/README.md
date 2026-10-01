# libs/protocol

控制消息、显式 big-endian 序列化、长度/版本/时效校验与 Host 测试。

状态：HOST_TESTED_FOUNDATION；VehicleCommand 已有独立显式 payload codec 与 Host
loopback 测试，其余业务 payload schema 和真实跨进程部署仍待后续工作。
参见 `docs/architecture/VOICE_AI_FOUNDATION.md`。
