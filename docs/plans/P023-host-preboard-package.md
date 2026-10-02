# P023：上板前 Host 闭合包

- 用户要求：先完成实板测试前的工作，不限制方法。完成一个上板前候选包里程碑；不自动部署、写板或启动 M0。
- 基线：`agent/amp-platform-closure / 9c6f025`；固定 reference 不改；前轮 echo、Linux barrier 与 U-Boot shared setter 草案保留历史。
- 范围：独立派生 RTOS/UBoot/transport；生成配置、patch、候选 contract/Host 输入、检查和 manifest；源/构建/恢复证据。仓外 worktree 实验已由此前用户工作方式授权。
- 权限：Host 源码/构建；必要时通过既有 alias 做有界普通文件只读盘点和向 Host 复制原件。无 MMIO 读取重试、SMC、板端上传、配置写入、分区/固件写入、重启或 M0 启动。
- 顺序：落实冷启动 cache bypass 与有界地址转换 → Linux 专用 pool fail-closed → 唯一参数源及无重叠候选布局/Host 编译 → 关闭 loader/reset/FIT/recovery 静态缺口 → 验证、审查、提交与 Draft PR。
- 候选参数必须标 Host proposal：显式 setter/reset 后的配置与当前板状态分开；不得伪造当前 CON17、AMP 能力、签名策略或恢复介质。
- 验收：clean M0 build 及实际 ELF cache 路径证明；payload 映射边界/互逆 Host 检查；transport 对象编译与负例；可确定的 DTS/FIT 合同检查；所有输出/配置 hash；具体剩余外部证据逐项列出。能自主解决的源码/构建工作完成后才报告所缺信息。
- 停止条件：同一阻塞两次有依据尝试；板端异常立即停止该路线。若缺关键板端/厂商输入，维持 C 并交付已验证候选包，不降低 D 条件。

## 执行结果

完成Host候选包里程碑，等级仍C。冷reset/cache bypass/bounded PA映射、Linux专用pool fail-closed、U-Boot reset/clock/正常Linux boot修复已形成独立派生提交和0003..0006 patch。M0 clean v5（122,696B、0warning）、DTS/FIT、板headers Kbuild、完整候选Kernel Image/modules/v2DTB+echo、完整U-Boot bin均PASS；实际v2DTB与板原件相同，Module.symvers与实际headers副本相同，但精确运行Image源码身份不冒认。

9,672项Host合同/DT保留/ELF检查、65,536字节实际header转换检查、26+1项提取U-Boot C函数fault-injection、Host CI/14 Python测试与CTest通过。原boot文件及运行DT通过只读SSH复制Host，硬件PDF定位UART5；普通pinmux读拒绝后停止，没有sudo/MMIO。reference SHA/clean状态保持。

交接和manifest见`docs/reviews/rk3576-amp-platform-closure/HOST_PREBOARD_PACKAGE.md`。剩余输入是完整Debian12 GNOME update.img、当前boot加载/签名能力及后续批准的动态映射/console确认；没有写板、上传固件、加载ko、重启或M0启动。主分支保持不merge，按本轮review/CI提交task branch和更新既有Draft PR。
