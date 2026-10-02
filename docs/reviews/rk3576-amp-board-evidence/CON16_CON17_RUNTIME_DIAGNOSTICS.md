# CON16 / CON17 运行时读取排障（2026-10-02）

**裁决：两个当前值均未取得。** Linux `/dev/mem` 实际读取触发 `SIGBUS`；当前 BL31 对两个目标地址的 Rockchip SiP 读取均返回 `SIP_RET_INVALID_ADDRESS`。这两项是不同访问路径的失败证据，不代表寄存器值为零，也不足以证明寄存器本身存在读副作用。AMP 仍为 **C. HOST_BUILD_PASS**。

## 对象与来源

| 项目 | 值 / 证据等级 |
| --- | --- |
| 板端身份 | `LubanCat-3 v2`，`Linux 6.1.99-rk3576 #8`；新 SSH IP `10.232.249.223`。`BOARD_OBSERVED` |
| SYS_SGRF / CON16 / CON17 | `0x26004000` / `0x26004060` / `0x26004064`；TRM §1.1、§8.6.2 和固定 HAL 一致。`SOURCE_VERIFIED` |
| SiP read ABI | 固定 LubanCat U-Boot `arch/arm/mach-rockchip/rockchip_smccc.c:133-139` 的 `SIP_ACCESS_REG(0x82000002), 0, addr, SECURE_REG_RD(0)`；板端内核 headers 的 `SIP_RET_INVALID_ADDRESS=-4`。`SOURCE_VERIFIED` |
| 本次 Host 诊断模块 | `scripts/board/amp_sgrf_smc_probe/`，只接受两个上述地址，不写寄存器；Host `.ko` SHA256 `0dd962e03e102693789f180c1318a548bab147066ec9d1f749ba63e0b34cdfe2`。`HOST_TESTED` |

本次是用户解除上轮“失败即停止”限制后的新排障。此前单次 BusyBox 命令返回 1 的历史记录保留于 [CON16_CON17_APPROVED_READ_ATTEMPT.md](CON16_CON17_APPROVED_READ_ATTEMPT.md)。所有实板操作串行持有用户级 board lock。没有写目标寄存器、启动 M0、修改 `/boot`、更换固件或重启。

## 路径一：Linux 直接 MMIO

执行：

```text
sudo -n /usr/bin/strace -e trace=openat,mmap,munmap,close /usr/bin/busybox devmem 0x26004060 32
```

相关原始输出：

```text
openat(..., "/dev/mem", O_RDONLY|O_SYNC) = 3
mmap(NULL, 4096, PROT_READ, MAP_SHARED, 3, 0x26004000) = <mapped address>
--- SIGBUS {si_signo=SIGBUS, si_code=BUS_OBJERR, si_addr=<mapped address>+0x60} ---
+++ killed by SIGBUS +++
```

`/dev/mem` 打开和 mmap 已成功，异常发生在读取映射页的 CON16 偏移 `0x60`。因此不能把先前无输出的 exit 1 解释成“缺少命令”或“mmap 被拒”。**Linux EL1 直接 MMIO 读取 CON16 不可用（`BOARD_OBSERVED`）**。安全防火墙是公开 TF-A 配置与该症状吻合的一种解释，当前异常本身不能唯一证明其根因。没有直接访问 CON17，不能把 CON16 的 `SIGBUS` 冒充 CON17 的直接实测结果。

## 路径二：当前固件 SiP 只读服务

Host 用板端只读复制的 `linux-headers-6.1.99-rk3576` 构建一次性模块，构建 exit 0，`modinfo` vermagic 为 `6.1.99-rk3576 SMP mod_unload aarch64`；GCC 版本与内核构建器不同（Host 11.4，内核 10.3.1）。模块严格检查 `target_addr`，然后只调用一次 `arm_smccc_smc(SIP_ACCESS_REG, 0, target_addr, SECURE_REG_RD, 0, 0, 0, 0, &res)`，打印结果并故意返回 `-ENODEV`，不保持模块驻留。`nm -u` 仅有 `__arm_smccc_smc`、`__stack_chk_fail`、`_printk`、`param_ops_ulong` 等常规内核依赖。

模块复制到板端 `/dev/shm/rk3576_sgrf_smc_probe-0dd962e0.ko` 后 SHA256 核对一致。先用 `target_addr=0` 检查地址限制，`insmod` 报 `Invalid parameters`，无 SMC 调用。随后各执行一次：

```text
sudo -n /usr/sbin/insmod /dev/shm/rk3576_sgrf_smc_probe-0dd962e0.ko target_addr=0x26004060
sudo -n /usr/sbin/insmod /dev/shm/rk3576_sgrf_smc_probe-0dd962e0.ko target_addr=0x26004064
```

两次 `insmod` 均显示 `No such device`，对应模块主动返回 `-ENODEV`；内核 journal 的关键原始行是：

```text
rk3576_sgrf_smc_probe: addr=0x26004060 status=0xfffffffffffffffc value=0x00000000
rk3576_sgrf_smc_probe: addr=0x26004064 status=0xfffffffffffffffc value=0x00000000
```

`status` 是有符号 `-4`，按当前内核 Rockchip SiP header 定义为 `SIP_RET_INVALID_ADDRESS`。**`value=0` 在错误状态下无效**。这证明当前运行固件提供了 SiP 应答，但此只读接口不接受这两个地址（`BOARD_OBSERVED`）；不能推断固件没有其它内部读法，亦不能推断 CON16/17 实值。

### 板端状态影响

加载诊断模块使 `/proc/sys/kernel/tainted` 从 `0` 变成 **`4096`**，即已加载过非树内模块的内核运行时标记；它会持续到下次正常重启。本次**没有为了清除标记而重启**。测试后删除了仅位于 `/dev/shm` 的诊断 `.ko`，`lsmod` 中没有该模块；SSH 仍响应，`uptime -s` 仍为 `2026-10-01 13:45:00`。没有持久化板端文件或启动链修改。后续内核故障分析应知晓本次 taint，不能称本次尝试完全无状态影响。

## 路径三：当前启动固件镜像静态分析

经 `lsblk` 核对当前 eMMC `uboot` GPT 分区 `/dev/mmcblk0p1` 为 8 MiB 后，只读 `dd if=/dev/mmcblk0p1 bs=1048576 count=8` 流送到 Host 忽略目录 `artifacts/local/amp-uboot-dump-20261002/`。复制退出码 0、字节数 8,388,608、分区镜像 SHA256 `ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`。没有在板端写文件或分区。

`dumpimage -l` 确认该分区中的 U-Boot FIT 包含 `uboot`、`atf-1/2/3`、`optee` 与 FDT。提取的 `atf-1` SHA256 `1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07`，字符串含 `bl31-v1.14` / `v2.3-859-gc481e5368`；实际 `uboot` 子镜像字符串含 `U-Boot 2017.09-g8f53f800da-241224`。这些是**当前板镜像的静态指纹**，比公开仓库版本推测更直接，但不等于当前寄存器读数。

在 `atf-1` 偏移 `0x29a54` 存在 32 位字面量 `0x26004064`；周围是连续地址/范围表（例如 `0x2600403c, 0x26004064, 4, 0`），并无对应的当前寄存器值。`CON16=0x26004060` 字面量未出现。静态镜像不能证明谁在何时设置 CON17，也不能替代运行时读取。**`SOURCE_INFERRED`**（仅可证明二进制包含该地址常量）。

## 结果与边界

| 字段 | 结论 |
| --- | --- |
| `CON16_RUNTIME_EVIDENCE` | `BLOCKED`；直接读 `SIGBUS`，SiP `-4` |
| `CON17_RUNTIME_EVIDENCE` | `BLOCKED`；SiP `-4`，未作直接 MMIO 读取 |
| `M0_LINUX_ADDRESS_MAPPING` | `UNRESOLVED`；缺实际 CON17 值 |
| vring0 / vring1 Linux PA | 不能数值计算；不使用失败响应的 `value=0` |
| `0x47800000_CONFLICT` | 与 CPU3 参考 DTS 的静态设计冲突仍成立；与当前 M0 shared window 的运行时关系 `UNRESOLVED` |
| AMP 总等级 | `C. HOST_BUILD_PASS` |

当前普通 Linux 直接 MMIO 与已知 SiP 只读服务均不能给出目标值。下一条技术路径必须有**经板厂确认的合法读取接口**，或在具备完整恢复链后，受控进入拥有该寄存器访问权的固件/M0 诊断环境。现有条件下不应靠猜测修改 linker、ITS、DTS，也不应随意试其它未知 MMIO 地址或 SMC 编号。
