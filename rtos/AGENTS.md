# RTOS层附加规则

继承根AGENTS.md。当前RTOS未启动验证，禁止给定假核号、DDR地址或I²C分配。
RPMsg仅小消息；明确长度/字节序/超时/重试。
MPU6050实际接线由用户完成。LED/按键/蜂鸣器是模拟状态。
CPU、外设控制器、GPIO bank、IRQ、clock/reset/power共享依赖需实际SDK审查。
任何固件加载与启动修改需用户批准，Mock不得伪装RT-Thread实测。
