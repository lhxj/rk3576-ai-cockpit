# P030 v5 准备与实际操作记录

2026-10-04。v4实测显示计数在变化但未跨周期；已记录覆盖缺口及默认冷恢复。32K频率是候选；没有宣布IRQ损坏或问题已修复。

新增0015：已分配UART5在115200 8N1发送两组6144 raw bytes（每组名义0.533秒），每次新增UART轮询都有1048576读预算。基线保持LOAD239998；计数15000..21000、wrap≤1且ISR/tick一致时，短暂屏蔽IRQ并仅重配MCU本地LOAD326（HAL参数327，不重复减1）。保持EXT源。第二组ISR40..75且RT tick同增量才继续首次延时/RPMsg；否则STOP。发送ASCII pace行不是错误。

固定RTOS/HAL fresh构建无warning/error；当前control-key vendor验签、FIT外部payload/entry/hash覆盖、容器长度、单组件SCRIPT/CRC通过。最大诊断格式107字节，低于console128。未新增/运行测试套件。SDK vehicle-evb的32K配置仅支持候选；UART参照是粗略判别，非精密频率证书或后续故障看门狗。

沿任务授权持锁有界，先核default Linux，再完整8MiB U-Boot、factory六文件/链接、旧包内容和metadata及paired assets。**只新增 `/boot/amp-p029/tickdiag-v5`**，安装结果 `P030_TICKDIAG_V5_PASSIVE_STAGE_READBACK_PASS`。独立SHA256SUMS四项及回执hash通过，原默认入口/旧树保持，RAM包及helper目录清理。回执SHA：`b55739cb9d7da4f6e9c9e194f86a1766d0ed922cc411e175c7aa7da0482f0ac8`。Agent没有source/M0/KO/SSH MMIO/重启。

v5 FIT：131072B / `348109ebecda0f00314d4dbcaaae0f8a51e2d88ac714c03ab74c1966a3b19bd6`；SCRIPT：3055B / `60f8dcb149e4e5a948dcdafe12638856e30013b7a6cb569428e8850049c01181`。完整身份及操作结果在同名JSON。尚未执行v5硬件B；tick率、首次延时唤醒、15s退出和完整RPMsg仍未验证，C/D关闭。
