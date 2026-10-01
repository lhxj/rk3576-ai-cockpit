# P012：RK3576 AMP board evidence closure

- 目标：用当前 LubanCat-3 v2 的**真实只读证据**判断 CON16/17、U-Boot/BL31、AMP FIT 来源、恢复路径能否支持唯一可信的 M0/RPMsg 配置；关键事实缺失则维持 C。
- 基线：`agent/amp-platform-closure` 的 `b89ef3a`；固定 RTOS/HAL reference 不修改，派生 RTOS 不改 linker/ITS/DTS。
- 范围：仅四条证据链及 RPMsg/cache 的有界核验；新报告 `docs/reviews/rk3576-amp-board-evidence/` 和必要的只读盘点脚本。
- 权限：Host 静态分析 L0；经既有 `ssh lubancat` 和 board lock 的用户 `cat` 只读盘点 L1。**不执行** devmem、`/dev/mem`、debugfs 寄存器读取、U-Boot console、sudo、boot/eMMC 写入、M0 release、重启。
- 步骤：①复核上一轮报告和固定源码；②只读寻找板上已有日志、启动镜像路径、版本与 hash；③按实际板镜像和公开固定源码审查 AMP loader/BL31/CON16/17；④冻结或否决 RPMsg resource candidate；⑤记录 recovery 对象与尚缺镜像；⑥写八份证据报告及条件式 memory layout candidate，复核 C/D gate。
- 验收：每条关键结论区分 SOURCE_VERIFIED、BOARD_OBSERVED_READONLY、INFERRED、UNVERIFIED；若需要特殊寄存器读取，仅输出 `APPROVAL_REQUIRED_READ`（寄存器、纯读命令、风险、用途）而不执行。无证据时不填写新的物理地址。
- 验证：只读脚本 shell syntax、Python/文本检查、`git diff --check`；如使用 SSH，保存有界原始输出至忽略的 `artifacts/local/`，不提交敏感日志。结束后提交并更新已有 Draft PR。
- 失败恢复：板端无写入，因此无板端 rollback 操作；Host 只在独立 worktree 提交。板锁失败、需要 sudo/特权读或来源不一致时停止该路径并记 BLOCKED。

## 实际结果与交接

- 两份 bounded SSH 只读脚本均 exit 0，原始输出保存在 `artifacts/local/amp-board-evidence-20261001T155533Z-187081/` 与 `artifacts/local/amp-boot-package-20261001T155602Z-187208/`，未提交原始日志；无 sudo、raw block、MMIO、U-Boot console、重启或板端写入。
- 板端版本指纹为 `uboot-8f53f800da-04/24/2026`、`bl31-v1.14`，GPT 无 `amp`，`/boot/boot.cmd` 没有 FIT 装载命令。普通板端日志与 sysfs 无 CON16/17 当前值或 AMP capability 证据。
- Host 浅克隆并锁定公开 LubanCat U-Boot `8f53f800da2c25d0c6ba414fb45902a01675703a` 到 ignored artifacts。源码可选 `rk3576-amp.config` 才启 `CONFIG_AMP`；`amp_cpus_on()` 仅从 `amp` GPT 分区加载 FIT。公开源码与板端二进制能力严格分开。
- 八份目标报告与额外 `APPROVAL_REQUIRED_READ.md` 落在 `docs/reviews/rk3576-amp-board-evidence/`。无最终 memory layout 或新的 load/ITS/DTS 修改；CON17、真实 U-Boot/BL31、coherency 与 recovery 均 BLOCKED，最终维持 C。
- Host 验证：只读脚本 `bash -n`、`git diff --check`；资料页与源码链接在报告中。下一步需要板端负责人对纯读寄存器方案及实际 bootloader/BL31 镜像获取方式给出独立授权，然后再定物理布局。
