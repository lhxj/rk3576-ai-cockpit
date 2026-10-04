> 2026-10-04 最新结果：本指南对应 C 已由用户实测 HELLO_ACK/PONG 双向通过，测试后完整冷恢复默认 Debian 通过。见 [执行记录](P030_RPMSG_C_V1_EXECUTION.md)。以下保留本次操作步骤，无需重复执行 C；原始日志中的第二次 insmod 返回 File exists 已如实记录。

> 新C准备/暂存/读回已完成，可按 [新版C指南](P030_RPMSG_C_V1_GUIDE.md) 执行。本文件下文保留准备前的条件清单；旧入口暂停。

# P030 新版C准备条件

2026-10-04。v5 B代码执行、cache entry、时基、首次延时、预计15s退出与完整冷恢复已通过。**新版C已准备、暂存并读回，可按上方新版指南在新冷会话执行；旧C保持暂停，不在原B会话加载KO。** 下文列出准备与验收条件，通信结果仍待实测。

## 必须沿用的身份

- proper U-Boot149b1c5和完整8MiB SHA f9beef07f807123a48f5115458dedb7bdf47337306df1a981190abf65372c0b3。
- v5 signed FIT131072B/SHA348109ebecda0f00314d4dbcaaae0f8a51e2d88ac714c03ab74c1966a3b19bd6，payload SHA28691e30790f1585782f3528dbd611c1e8005f8790947462574d73b8a75b6124；当前M0本地时基条件修正保持，不回退旧固件。
- 已安装的配套6.1.99-rk3576-m0echo-p026 Image/initrd/C DT及相同构建的echo KO。新准备时须重新核字节身份与模块匹配。

## 新C入口已完成的准备

Host已复核C DT的no-map、rings/pool和既定transport引用、无amp-cpus第二次启动路径；生成正确单组件SCRIPT，结束项0、CRC正确，替换签名FIT路径与长度门，保留root p3/同一预加载Linux DT/单次loader。当前公钥验签、包内实际字节、来源和新manifest核验通过；只新增独立目录，完整U-Boot/factory/旧tree检查前后保持，逐项读回与RAM清理完成。实际C运行的DT/地址/服务仍由只读preflight核验。技术保护不因已授权而跳过。

## C的实板验收

用户新一次冷启动并提前保存双串口；先只读核C身份、预留区、Linux真实ring PA0x47d00000/0x47d08000及DMA backing范围[0x47d10000,0x47d20000)。在C窗口按新版指南加载唯一配套KO，确认Linux真实收到HELLO_ACK/PONG，同时M0实际收到pool指针并核entry/link/after-pong bypass1。日志/观察有上限，异常停止且不在同次会话重试，结束完整冷恢复默认系统。

上述通信只证明已访问子范围的有效映射，不证明安全寄存器原值或整个512MiB窗口，也不证明长期或缓存开启模式。C执行条件具备，C实际通信/D尚未通过。可执行命令在新版指南。
