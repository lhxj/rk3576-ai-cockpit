> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# P030 tick 诊断与首次延时前失败停止：执行记录

2026-10-04。用户要求继续解决首次 mdelay 未返回。已完成干净构建、签名封装、安装器范围审查与被动暂存；**v4 的实际时钟/IRQ根因、首次延时与完整 B 尚未验证**。

## 源码依据与修改

实际旧 ELF 中 board init 调用外部 SysTick 选择、reload、enable，但不消费返回值。RT delay 会挂起线程并依赖 timer/tick 唤醒；应用 15 秒检查不能包住不返回的 delay。

0013 补丁仅在 evb board 保存既有 HAL 返回值，ISR 增加本地 volatile 计数（无 ISR 打印），增加本地寄存器/向量/中断状态与固定 1048576 VAL 读取的预检；app 在 cache gate 之后、remote_init 之前调用预检，失败 STOP/return，成功才保留原等待逻辑。没有改共享映射、transport、时钟值或添加自动 fallback。该预检不是运行期 watchdog；不能保证随后 tick 故障时的延时返回，主动运行探针也会改变进入 idle 的时机。

0014 只把打印拆成短行，以适配现有 128 字节 console buffer；最长按完整 phase/32位数字估算为 99 字节。v3 先已暂存，交付前发现长行会丢失字段；因此暂停 v3，不覆盖它，独立新增 v4。没有用户或 Agent 执行 v3。

## 构建与封装

fresh pinned RTOS archive + 0010/0011/0012/0013/0014，固定 unchanged HAL 与 ARM GNU 13.2.rel1；干净编译无 warning/error。ELF SHA `09e44af79cda7172d1ebf0f3453ab9fb86d0d3cd6edad49ee8e115c6a7795e34`。BIN 124840 字节/SHA `4fafc33bdf6fb27802117415a87c22dee9afe3406ff57248daa49754171cbc54`，stack 0x80000、entry 0x141、code <0x80000。

实际固定 vendor mkimage 使用现有本地开发测试 key 签新 FIT，不修改 control key DT；匹配 fit_check_sign 对当前 control-key DT 检查通过。签名覆盖五个预期节点，payload/entry/load/外部数据位置/容器总长核对；正确单组件 SCRIPT 结束项为0并重算 CRC。没有新增或运行测试套件，未把封装校验当作板端通过。

## 板端实际操作

两次均持锁、有界、串行预检→RAM传输→固定 installer。每次预检/安装核当前默认 6.1.99-rk3576 #8/root p3/无 stage、完整 8MiB U-Boot SHA `f9beef07f807123a48f5115458dedb7bdf47337306df1a981190abf65372c0b3`、factory 六内容/链接和既有包的内容/metadata/尺寸/路径白名单。原子 no-overwrite 只新增目录，安装后独立 SHA256SUMS/receipt/cmdline 读回，RAM源与 archive 清理。

v3：`/boot/amp-p029/tickdiag-v3`，暂存 PASS，receipt SHA `305ad62805fdbeb84c0c45889def32b75644645e357d9bd951f2f73c8c5321c6`，随后因日志宽度暂停。

v4：`/boot/amp-p029/tickdiag-v4`，暂存 PASS，receipt SHA `198b4eea46b55cc9b4b47599996da57307a3bc3b015c9d2ec0a39600ff17e8da4`。旧 tree/factory 保持，四项 sums 独立读回通过。板子仍默认 Debian；Agent 未 source、启动 M0、加载 KO、访问 MMIO 或重启。

## 下一步及限制

用户按 [v4 指南](P030_TICKDIAG_V4_GUIDE.md) 做一次新的完整冷启动 B，回传 COM5/COM6 全文。根据 cfg/VAL/ISR/tick/PRIMASK/向量区分原因后再改时钟或调度路径。B/有效 mapping/cache/RPMsg/D 不提前通过，C 关闭。所有执行记录与指南同步 Windows 桌面，私钥/登录凭据不交付。

[机器证据](P030_TICKDIAG_V4_PREPARATION.json)。
