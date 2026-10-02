# P026：一次只读取证审批对象与执行记录

## 当前记录（2026-10-03）

用户直接批准“今天中午12点之前，不需要单独审批，直接执行即可”。本次03:00左右完成批准范围内的读取；详细结果见 [P026_BOARD_READ_EVIDENCE.md](P026_BOARD_READ_EVIDENCE.md)。真实TA command5一次成功，raw0；原8MiB U-Boot备份与UART5 owner读取成功。`/dev/shm` noexec导致第一次程序未启动，改用已有可执行RAM tmpfs `/run`，同SHA程序执行并清理；没有改mount策略。下文保留审批前的对象和风险说明，其中“未执行/false”是当时状态，已由本节更新。许可不解释成自动部署或启动M0。

2026-10-03。普通SSH已确认`/dev/tee0`和8MiB uboot分区仅root可读，pinmux debugfs也对cat不可读；`tee-supplicant`/现成TEE测试工具未发现。按AGENTS L2，新自写诊断程序上传/运行需单独批准；不自动利用root权限或改sudo策略。以下Host对象已经可审查，尚未上传、运行或读受保护接口。

## 精确对象

- 源码：`tools/amp/read_verified_boot_flag.c`，SHA256=`9114dc2d2ee20124efcc97c644c76888761e0d660a335921a361bae71abbe000`。
- AArch64静态Linux可执行文件：`artifacts/local/p026-tee-flag-host-reviewed/read_verified_boot_flag`，SHA256=`2b8929f02dbc86ff4f6f34eaecc0d878b9de1706956f8a8a33b84b8ca2e6beca`。
- Host build：gcc11.4.0，`-Wall -Wextra -Werror -static`，exit0/0warning；强制include固定6.1 kernel的TEE UAPI。builder逐项比较固定U-Boot实际TA UUID、read command5、4B output和`bootflag==0xff`解释；4项错误UUID/command/写方向拒绝测试PASS。
- 命令：`timeout -k 2s 15s sudo -n /dev/shm/rk3576-p026-read-flag/read_verified_boot_flag --read-vboot-flag`。仅在用户批准、目录是本轮新建且二进制SHA匹配时使用；`sudo -n`失败就停止，不索取密码、不修改权限策略。

## 动作与边界

1. 在Host核hash，把该自写程序放到新的`/dev/shm/rk3576-p026-read-flag/`，板端再次核hash；这是**临时RAM文件写入**，不是完全零写入，必须纳入审批。
2. 只打开`/dev/tee0`，查OP-TEE GP capability，打开固定vendor TA `2d26d8a8-5134-4dd8-b32f-b34bceebc471`，调用**唯一command5、output-only 4B**一次，再关闭session。它对应vendor `trusty_read_vbootkey_enable_flag`调用的读操作。不会提交OTP写命令、读密钥、调用MCU SMC、读CON或任意MMIO。
3. 同批可批准：仅向Host复制当前`/dev/disk/by-partlabel/uboot`的8MiB，核SHA与原件；只读取`/sys/kernel/debug/pinctrl/*/pinmux-pins`的owner状态。所有root读取只用固定路径，不开放通用root shell、不改debugfs挂载/权限、不扫描未知寄存器。
4. 结束关闭TEE session、移除本轮临时程序/空目录，原始日志/firmware只留忽略的Host目录；不提交凭据或完整日志。

**不写/boot、uEnv、DTB、Kernel、U-Boot、BL31、OTP或eMMC；不加载KO、不启动M0/RPMsg、不重启。** Linux ioctl/共享输出缓冲区使用正常内核/TEE内存管理，不等于直接MMIO。flag=0也不证明当前U-Boot实际调用成功，需以SOURCE_INFERRED单独标记该等价路径。

## 风险与停止

secure TA可能不可用、拒绝访问、等待缺失supplicant或超时；使用应用alarm12s与外层15s timeout，但这不能保证终止EL3/内核不可中断挂起。第一次异常就停止，不改UUID、command、访问路径重试，不自动重启。闭源TA内部行为未独立审计，读意图来自固定厂商调用契约，不能保证“零风险”。已知root执行范围需用户明确批准。

有效结论必须同时有open/invoke返回0、output长度4、buffer确实更新；失败不把初始0当disabled。成功后按vendor映射raw`0xff`→required1，其它有效值→required0；记录Linux OP-TEE观察与proper U-Boot预计解释的区别。raw firmware/pinmux证据也不能代替CON17/cache运行值。

## 审批状态

`BOARD_EXECUTION_AUTHORIZED=false / APPROVAL_REQUIRED_READ`。Host工作继续；收到明确许可前不执行以上板端动作。
