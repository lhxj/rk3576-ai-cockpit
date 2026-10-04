# RTOS层附加规则

继承根AGENTS.md。RK3576 BUS M0最小echo已启动并实板双向RPMsg通过，当前事实见docs/amp/AMP_RPMSG_INTEGRATION_TIP.md。用户冻结当前链，不新增RTOS业务；核/DDR/通知采用已测身份，I²C等未分配资源仍禁止虚构。
RPMsg仅小消息；明确长度/字节序/超时/重试。
MPU6050实际接线由用户完成。LED/按键/蜂鸣器是模拟状态。
CPU、外设控制器、GPIO bank、IRQ、clock/reset/power共享依赖需实际SDK审查。
任何固件加载与启动修改需用户批准，Mock不得伪装RT-Thread实测。
