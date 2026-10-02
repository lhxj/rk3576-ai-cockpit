# P022：RPMsg shared memory cache 路径静态闭合

- 目标：核对匹配 BL31 CODE selector 的 CON14/15 写入、TRM 非缓存范围语义、候选 RTOS cache 初始化、Linux vring/payload mapping 与 barrier；确定是否能形成 uncached 方案，缺证据不升级 D。
- 基线：主项目 `agent/amp-platform-closure` / `5b172ed`；固定 RTOS `7c397f4`、HAL `277de3f`、U-Boot `8f53f80`；前轮 BL31 与 SDK payload 全字节匹配。
- 权限：L0 Host only。无 SSH、MMIO、SMC、板端部署/重启；不改 reference、不修改 linker/ITS/DTS 或最终地址。原直接读寄存器失败路线不重试。
- 文件范围：本计划、board-evidence 新 cache 调查与 machine result、SUMMARY/COHERENCY_DECISION 的新增证据、自写静态 verifier；按需要更新 THIRD_PARTY 来源登记。上轮提交推送状态仅检查/有界重试。
- 步骤：保护 Git → 核固定源码/TRM/已匹配固件的实际控制与数据路径 → 建立范围/属性/屏障证据及缺口 → 自写 Host verifier 正/负检查 → 常规 Host CI/review/commit/push。
- 验收：分开列 M0 cache admission、M0 barrier、Linux ring mapping、Linux payload DMA mapping、硬件 coherency 和当前运行状态。只有确定各路径才选择最终 scheme；允许有依据的 conditional candidate，不能用候选值代替 runtime 事实。
- 中途发现并扩展最小 Host 范围：固定 Linux transport 将 `weak_barriers=true` 传给跨 M0 virtqueue，选择 inner-shareable 屏障；准备仅改为 false 的派生单文件 patch，在已复制板端 headers 做对象编译。只保存 patch/hash，不生成或加载 ko，不改 kernel/DT/地址。另查 no-pool fallback 和 cache 视图差异并记录阻塞。
- 恢复/停止：无实板变更；输入仅静态解析不执行。新关键 UNKNOWN 保留为 blocker，不靠改地址或试未知寄存器补缺口。

## 实际结果

- **完成 Host 里程碑，未闭合实板 coherency。** 结论 `STATIC_HOST_EVIDENCE_PASS; BOARD_COHERENCY_UNVERIFIED`；整体仍 C。主报告：[SHARED_MEMORY_CACHE_PATH.md](../reviews/rk3576-amp-board-evidence/SHARED_MEMORY_CACHE_PATH.md)。没有任何板端命令，也未修改 AMP 固件、地址或固定 reference。
- 实际 echo ELF 的 cache enable/bypass-clear 路径已核；BL31 CODE 分支写 CON14=`0x20000000`、CON15/16=caller load 的原始指令已核。当前寄存器值、非缓存比较器地址视图仍未知。
- `platform_patova` 的实际固定 PA offset 经 Host GCC `-Wall -Wextra -Werror` mock 验证；它隐含 B17=`0x40000000`，没有将该推导当成实读或部署值。Linux no-map/non-reusable pool mapping 与 fallback 风险已追明。
- `python3 scripts/amp/check_shared_memory_cache_evidence.py` 的完整参数见主报告：exit 0；错误 BL31 字节、非最小 draft 两个负例均按预期被拒绝。结果及源 hash：[SHARED_MEMORY_CACHE_RESULT.json](../reviews/rk3576-amp-board-evidence/SHARED_MEMORY_CACHE_RESULT.json)。
- 设备屏障单参数 patch 在新 `artifacts/local/p022-linux-transport-final/` 输出目录编译 AArch64 REL 对象，Kbuild/readelf/compiler 命令均 exit 0。仅有已知 Host GCC 11.4.0 / kernel GCC 10.3.1 差异提示；未生成 ko/kernel。命令和 hash：[LINUX_RPMSG_BARRIER_DRAFT_RESULT.json](../reviews/rk3576-amp-board-evidence/LINUX_RPMSG_BARRIER_DRAFT_RESULT.json)。完整日志保留忽略目录，固定源文件未修改。
- 常规 `bash scripts/dev/host_ci.sh` exit 0：CMake build、2/2 CTest、6/6 Python tests、shell syntax PASS；这不是新 cache 方案的实板证明。
- 下一阶段建议：派生冷启动 cache bypass 固件与 payload 转换/专用 pool fail-closed 方案；仍须证明有效 remap、完整物理范围、当前 kernel 精确来源和 boot/recovery，才可申请板端测试。此计划未授权新的板端操作。
