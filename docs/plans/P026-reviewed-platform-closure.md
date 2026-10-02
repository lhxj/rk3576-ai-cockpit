# P026：主控审核下的 AMP 准入闭合

2026-10-03；基线`agent/amp-platform-closure / dba800e`，用户明确授权一个子代理，并要求主控审核其结果直到达到D。未授权本轮部署、重启、启动M0或修改boot/GPT。整体当前C，只有原准入条件真正满足才标D。

## 背景与权限

P023/P025的M0 echo、Linux transport/kernel/DT与U-Boot候选Host构建通过，fixed reference不动。原厂板正常Linux-only，GPT无amp；用户已确认整机恢复通过。已有串口SPL Verified-boot=0，只证明该阶段，proper U-Boot AMP策略未观测。当前CON/实际cold reset/cache路径不能由Host mock补填。

允许Host派生源码/标准构建工具/已有firmware静态分析、既有SSH普通只读盘点。特殊访问、新诊断程序/KO运行或boot写入先形成可审查对象，再按适用授权门处理；不能因用户说“到D”猜测其批准了部署。不会重试已异常的CON直接访问。

## 一个里程碑，三项任务

1. 子代理`amp_host_preload_closure`独立派生worktree修复复制前完整RTOS/shared内存保护，核无GPT改动的加载方案，交付实际源码patch、Host故障测试与限制。文件归属：新0008 patch、新p026脚本/测试、新P026_PRELOAD_AND_LOAD_SOURCE报告；不碰公共contract/STATUS/manifest。主控独立检查实现与源码调用时序，不能只采信代理总结。
2. 主控核proper U-Boot策略的已有合法只读接口、当前raw firmware身份、UART5/内核modules准备；区分源码/Host/板端观察。缺权限/特殊执行时先交付具体方案，不自行写板。
3. 主控审核并整合唯一参数源、加载/数据保护和精确changeset/rollback、更新machine gate与artifact身份，完成Host CI→review→commit→push→既有Draft PR。若关键事实仍缺，诚实保持C并提出剩余最小动作；不得无限重试或伪造D。

## 验收与恢复

实际C边界/冲突/调用顺序测试、完整必要构建，文件SHA/源码commit/命令exitcode可追；固定reference干净。D级准备与最终部署授权分别判断：达到D时部署仍未授权，停在APPROVAL_GATE_BOARD_TEST。任何实际写板仍等待用户下一轮逐项批准。

本轮不更改板，因此无新板端回滚动作；保留P025原boot tar与官方已核恢复image。禁止提交原始板日志、凭据、firmware大文件；只保存脱敏证据和patch。

## 执行

- 子代理已得到任务背景、固定pin、最新证据、职责/文件边界和禁止SSH/再spawn要求。主控已读取工作规则和P025 gates。
- UART5信号电平及16/18脚现状已向用户请求；无需其现在接线或重启。

## 实际结果（2026-10-03）

1. 子代理最终派生commit2314a3f9、0008 patch，fresh U-Boot/FIT封装PASS；主控独立阅读sysmem/allocator/FS/FIT/booti/initrd/FDT/DT no-map路径，复跑20+113+16+54实际C/libfdt项和9顺序检查，以及46验签case/3顺序检查。compile/header/合同生成一致；source/fixed reference clean。目标zero warning，23Host upstream warning保留。无GPT自动启动，显式file-only入口。
2. 用户直接批准当日12:00前取证后，原raw8MiB和pinmux owner读取PASS；TA command5输出4B0、exit0/cleanup0。/dev/shm noexec导致首次程序未启动，改用已核允许执行的/run tmpfs同binary只执行一次成功，没有remount/OTP/MCU SMC。全新kernel release、255paired modules、echo、initrd的Host构建/metadata检查成功；旧模块vermagic不足以证明ABI所以未混用。最新原件另存WindowsDesktop核hash。
3. P026 manifest/changeset和明确rollback已生成，原raw/modules备份也纳入hash门。Host integrity PASS；D readiness/部署BLOCKED。合同final没有用未来setter填伪板端值。完整结果/Host CI及Git交付见P026_CLOSURE_RESULT/P026_HOST_VALIDATION，旧报告保留。

**停止的具体条件：** 现有普通/安全读取路线没有给CON16/17有效值，cold reset/cache/新Ub运行也未观测；同一异常寄存器路线不再重试。当前授权只解决取证，不批准改变boot或启动M0。用户原D门尚未满足，不能以Host stub或“未来会写入”替代其要求；整体C。可审查产物已完成，下一阶段需独立受控固件诊断/启动链范围授权。
