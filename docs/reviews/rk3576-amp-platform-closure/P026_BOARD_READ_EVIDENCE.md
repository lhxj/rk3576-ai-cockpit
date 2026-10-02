# P026：验签开关、原 firmware 与 UART5 的实机只读证据

2026-10-03；用户直接批准当日12:00（Asia/Shanghai）前的本轮取证。执行时间约03:00。没有写boot/GPT/OTP/MMIO、没有启动M0、加载KO或重启；唯一上传是内存tmpfs中的已审查Linux诊断程序，结束已清理。原始firmware/日志保留在忽略的`artifacts/local`。机器证据见 [P026_BOARD_READ_EVIDENCE.json](P026_BOARD_READ_EVIDENCE.json)。

## 1. 厂商验签开关：读取成功

**BOARD_OBSERVED_READONLY**：固定厂商TA UUID `2d26d8a8-5134-4dd8-b32f-b34bceebc471`，只调用command5，MEMREF_OUTPUT，4 bytes；Linux `/dev/tee0`，GP OP-TEE实现。open/invoke均返回0、origin4、输出确实更新为`0x00000000`；按固定vendor函数`bootflag==0xff`解释，`required_flag=0`。不是从SPL的Verified-boot=0或缺security分区猜出来的。

实际命令形态：`timeout -k 2s 15s sudo -n /run/rk3576-p026-read-flag-<本次时间和PID>/read_verified_boot_flag --read-vboot-flag`。Host/板端hash均为`2b8929f02dbc86ff4f6f34eaecc0d878b9de1706956f8a8a33b84b8ca2e6beca`；exit0、stderr空、cleanup成功。没有OTP写命令、密钥读出或MCU SMC。

首次 `/dev/shm` 尝试被文件系统noexec拒绝，**程序没有启动**，不是TA拒绝。只读findmnt证明`/run`为已有、允许执行的tmpfs；没有remount或绕过noexec，用同一个binary在`/run`执行一次成功。真实TA调用没有重试。

**SOURCE_INFERRED**：派生proper U-Boot使用相同TA/command/4B ABI，预计解释为不强制FIT签名。仍保留运行时读取和失败required=1的策略，不关闭verified boot、不写OTP、不换key。Linux这次调用不证明proper U-Boot已经运行了该调用，也不是全局TrustZone/OTP状态证明。当前无签名Host FIT只在此明确策略条件及持续SHA验证下作为候选。

## 2. 当前原 U-Boot 完整备份

**BOARD_OBSERVED_READONLY**：先核by-partlabel解析到`mmcblk0p1`、blockdev大小8388608，再以`sudo -n dd if=/dev/disk/by-partlabel/uboot bs=1048576 count=8 status=none`仅向Host读回8MiB。

- 整分区SHA256：`ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`。
- 前4MiB SHA256：`06f48ed3716ccdde6adcb3187ca3eff766927e27bf6b7f432046539b782c4a56`，与已核恢复包payload完全相同；后4MiB全0。
- Host解析六个FIT payload及其SHA，全部通过。BL31 atf-1：`1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07`，OP-TEE：`010f86355ada5f4c24ff1334cab2341c05d9fbfd07a6ca1d3bef52bd4c1bf005`，control DT：`2eef029c7e599dc695b48f2bfdeabe0f2a87dda8588973750adaa8462edf9013`。
- 原U-Boot中未找到`bus_mcu/Handle standalone:/Brought up amps/amp_m0load`字符串。字符串缺失不是完整config证明；不能标记原板已启用AMP。项目新命令必须来自审核后的派生U-Boot。

普通sysfs只读确认：uboot start=16384（`0x4000`）、size=16384 sectors；boot start=32768（`0x8000`）、size=262144；rootfs start=294912（`0x48000`）。与已核factory parameter一致；没有amp分区。sector元数据不是写入许可。

## 3. UART5 两脚

**BOARD_OBSERVED_READONLY**：`sudo -n find /sys/kernel/debug/pinctrl -maxdepth 2 -type f -name pinmux-pins`找到现有owner文件，再只读cat。`pin124(gpio3-28)`、`pin125(gpio3-29)`均`MUX UNCLAIMED / GPIO UNCLAIMED`。与运行DT未分配、v2硬件表40Pin16 RX/18 TX对应。

**BOARD_OBSERVED_USER_REPORT**：用户确认TTL支持3.3V，16/18脚没有外设。已有Debug串口仍用于U-Boot/Linux1500000；M0 UART5候选115200是另一路。当前没有改接线或pinmux，没有宣称M0日志已出现；首次部署审批前确认独立观察通道即可。

## 4. 不扩大结论

本次未得到CON16/17实际值、有效映射、MCU cache状态，也未执行cold-reset/SiP setter或派生U-Boot。**这些仍UNVERIFIED**，不得以这次flag=0升级为AMP VERIFIED或自动打开D门。整机恢复用户实测PASS与最新原件备份保持独立证据。
