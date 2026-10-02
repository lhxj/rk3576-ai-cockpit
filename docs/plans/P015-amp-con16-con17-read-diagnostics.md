# P015：RK3576 CON16/CON17 受控读取排障

- 目标：查清 2026-10-02 BusyBox `devmem` 对 CON16 返回 1 且无输出的原因；若有可用的只读路径，取得 CON16/17 当前值并计算 M0/Linux 地址映射。
- 基线：`agent/amp-platform-closure` 本地 `90ad2f2`；板端新 IP `10.232.249.223`，SSH host key 与旧 IP 匹配；第一次 CON16 BusyBox 读取返回 1，CON17 未读。
- 授权：用户本轮明确解除先前“第一条失败即停止/不尝试其它路径”的禁令，并要求尝试解决寄存器读取。授权范围按当前目标理解为板端只读诊断与受控 MMIO 读取；修改 boot、写寄存器、启动 M0 等非必要操作不在本计划内。
- 未知：BusyBox 退出阶段、`/dev/mem` 内核限制、SYS_SGRF firewall 策略及当前寄存器值。
- 文件范围：本计划、`docs/reviews/rk3576-amp-board-evidence/` 下新增诊断记录与汇总；不修改 AMP linker/ITS/DTS/固件。
- 步骤：①核既有用户级 board lock、SSH 与 BusyBox 版本；②用已安装 `strace` 定位失败 syscall；③核当前内核已有 Rockchip `SIP_ACCESS_REG` 安全读取 ABI；④在 Host 用板端只读复制的 headers 构建仅允许 CON16/17 的一次性 SiP 读取模块，审查后经 `/dev/shm` 临时加载；⑤若取得有效值，脚本验证字段和地址计算；⑥记录原始输出、退出码与仍未闭合的风险。
- 测试与预期：Host `git diff --check`；板端命令独立记录 stdout/stderr/exit code。若取得有效值，解析 `[31:10]` 并用脚本核对；失败时不填推测值。
- 失败恢复与资源：只读操作不改启动链；若出现异常，停止 MMIO，核板端 SSH/日志。所有板端操作持有 WSL 用户级 board lock，不并发使用实体板。

## 实际结果与交接

- Host 用当前运行内核的板端 headers 构建限定地址的一次性 SMC 诊断模块，构建返回 0，vermagic 为 `6.1.99-rk3576 SMP mod_unload aarch64`，SHA256 `0dd962e03e102693789f180c1318a548bab147066ec9d1f749ba63e0b34cdfe2`。Host 使用 GCC 11.4，而内核构建信息为 GCC 10.3.1；这是构建差异，不能称为正式可部署模块。
- `strace` 证明直接读 CON16 时 `/dev/mem` 以 `O_RDONLY|O_SYNC` 打开、`PROT_READ` 映射均成功，实际 load 收到 `SIGBUS/BUS_OBJERR`。未重复直接读 CON16，未直接读 CON17。
- 模块使用与固定 U-Boot `sip_smc_secure_reg_read()` 相同的 SiP ABI，对 CON16、CON17 各发一次只读 SMC。两者均返回 `a0=0xfffffffffffffffc`，即 `SIP_RET_INVALID_ADDRESS=-4`；`a1=0` 是失败返回值，**不是寄存器值**。模块初始化故意返回 `-ENODEV`，因此 `insmod` 的“No such device”是预期表现。
- 首次 `insmod` 使板端 `/proc/sys/kernel/tainted` 从 `0` 变为 `4096`。这项运行时内核状态会持续到重启；不能把本次操作描述为对板端完全无状态影响。诊断 `.ko` 从 `/dev/shm` 移除，模块未驻留，板端 SSH 正常，`uptime -s` 仍为 `2026-10-01 13:45:00`；**没有重启**。
- 只读复制当前 eMMC `uboot` 8 MiB 分区至 Host 忽略目录，SHA256 `ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`。提取实际 `atf-1` BL31，SHA256 `1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07`；其中仅找到 `0x26004064` 作为连续地址表中的字面量，无法由此计算当前 CON17 值。
- CON16、CON17 的真实值均未获得；M0/Linux 地址映射与实际 vring Linux PA 仍不可计算。完整原始结果、证据等级与下一步边界见 `docs/reviews/rk3576-amp-board-evidence/CON16_CON17_RUNTIME_DIAGNOSTICS.md`。AMP 保持 `C. HOST_BUILD_PASS`。
