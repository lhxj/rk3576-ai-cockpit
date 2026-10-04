# P030 新版C双向RPMsg验证准备

2026-10-04，Asia/Shanghai。用户询问推进双向通信；v5 B时基/首次延时/预期退出与完整冷恢复已经通过。沿本任务用户“批准所有操作”的既有授权，不重复审批；用户手工冷启动/source/KO，Agent只持锁被动新增与必要读回，不自行M0/KO/MMIO/重启。

## 一个里程碑

1. 核既有paired C DT、modules/KO和driver实际来源，保留v5 signed FIT字节身份。新C仅启用已规划transport，保留root p3、paired Linux、预留内存、单次loader及失败冷恢复。生成正确单组件SCRIPT（结束项0和CRC）与可审核manifest，沿用完整原件/旧tree/metadata/no-overwrite安装保护。
2. 沿现有授权持锁有界核当前default、新完整U-Boot、旧包及paired资产；只新增 `/boot/amp-p029/rpmsg-c-v1`，独立readback/RAM清理。运行前脚本只读核C身份、DT/no-map/iomem、服务和实际ring/DMA backing，拒绝缺失/不匹配，不加载模块。
3. 保存Windows执行记录/指南与身份索引，更新STATUS并review/commit/push既有Draft PR。用户新冷C后必须Linux收到HELLO_ACK/PONG且M0实际共享指针/cache snapshots匹配，不能把包准备/设备注册/单方PONG当通信通过。C结束需完整冷恢复；仍不标D。

## 边界和失败

沿用现有M0/内核/DT/KO，不另刷U-Boot、改默认启动、共享地址、时钟或pins。新目录不存在/原件身份不符/只读权限缺失就停止。source仅一次，Linux在已准备C DT继续；初始化/中断异常、timeout、错误风暴立即停，不重跑source/KO、不warm reboot、不清dmesg或卸载抢设备。人操作的insmod即使有timeout也不保证能终止内核D状态，必须靠日志与冷恢复。

## 验证与交接

用户请求RPMsg验证。本轮Host校验针对实际交付字节/FIT签名、SCRIPT/DT/KO与driver来源、Linux buffer配置和保护，不新增无关测试套件或跑全CI。C启动后读回实时sysfs/dmesg，不通过SSH访问MMIO或预先发送RPMsg。两路日志每份2MiB、观察最多3分钟且firmware link后窗口180s；必须尽快完成只读核验及一次配套KO。默认恢复后核uname/cmdline再裁决结果。

进度：Host准备开始，C实际结果UNVERIFIED。


## 实际准备完成

Host核签名/精确FIT与payload、C DT及无amp-cpus/UART5 ownership、KO hash/vermagic、driver64*2*512 DMA空间和日志格式、新单组件SCRIPT/CRC通过。新增C入口3100B/0xc1c；只读preflight尚未在C运行。持锁default身份及全部原件/旧tree检查通过，单次新目录安装与五项独立readback PASS，receipt 4460df5a99a7b62a84c49e27ebcce51e3728b72c7e5b3216c87a3fd7f8fe586f；RAM目录/archive清理，原默认入口及旧metadata保持。Agent未启动M0/KO/MMIO/重启。Windows交付包括当前指南/准备/manifest/hash与精确包。待用户一次新冷C、真实HELLO_ACK/PONG/cache/pointers及冷恢复，不提前判C实测/D通过。
