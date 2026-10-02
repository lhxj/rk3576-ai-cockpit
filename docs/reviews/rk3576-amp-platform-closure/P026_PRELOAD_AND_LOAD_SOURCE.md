# P026：复制前完整预留与显式文件加载候选

2026-10-03；本子任务仅 Host，不接触实体板，不启动 M0，不写 boot/GPT。
结论：**Host 内存保护和无 GPT 改动入口已形成，交主控审核；不能单凭本报告判 D。**

## ExecPlan 与边界

目标：修复 P025 已确认的“先复制、后按 BIN 大小保留”问题，并形成明确可停用的首次 echo 加载入口。

| 步骤 | 文件范围/权限 | 验收 | 实际结果 |
| --- | --- | --- | --- |
| 追 FIT/sysmem/LMB 写入链 | 固定源码只读，L0 | 明确首次写入、各 allocator 边界 | SOURCE_VERIFIED |
| 派生 U-Boot 修复和文件入口 | 独立 worktree、0008 patch、新 p026 脚本 | fail closed，无分区写入、无默认自启动 | HOST_TESTED |
| C 故障/真实 FDT/全量编译 | 忽略的 artifacts/local；主控维护 manifest/STATUS | 完整预留先于 copy/release；fresh build；返回主控审查 | PASS，证据如下 |

失败恢复：仅保留 Host 失败日志，修改独立派生源码；不覆盖 reference、不 reset/clean 用户源码、不写板。范围不包含 board policy/SiP 实读、实际启动/cache 状态、整机部署授权。

## 1. 原问题与修复

**SOURCE_VERIFIED**，固定 vendor `8f53f800da2c25d0c6ba414fb45902a01675703a`：

- `common/image.c:boot_get_loadable` 调用 `common/image-fit.c:fit_image_load_index`；后者直接 `memmove(dst, buf, len)`。
- `drivers/cpu/rockchip_amp.c:standalone_handler` 原来在复制后才 `sysmem_alloc_base_by_name(id, load, BIN_size)`。
- `lib/sysmem.c:sysmem_alloc_align_base` 存在越过管理范围直接返回地址、同名 shortcut、tail guard 写入等语义，不适合直接当严格远端保留 API。
- sysmem 的 LMB 与 `bootm_headers_t.lmb` 不同；`fs/fs.c:fs_read` 和 `cmd/booti.c:booti_setup` 的直接写/搬移也不会自动服从 sysmem。

项目派生修复：

1. `amp_preflight_m0` 只接受 **一个** `mcu` loadable：`description=bus_mcu`、standalone/ARM/none、精确 load/local entry、完整 code/shared 元数据；拒绝 Linux dispatcher/CPU3/PMU MCU/额外 loadable，避免首个 core release 后被后续 image 覆盖。
2. 源数据必须完全位于已读取 FIT 内，长度不超过完整 code 区；FIT buffer 自身不得落入 code/shared；要求非忽略 SHA256/hash value。先检查范围，再进入 P025 board policy/config signature 和 image hash 验证。
3. 新 `sysmem_reserve_amp_region` 严格检查初始化、alignment、overflow、实际 DRAM bank、已有 LMB reservation 冲突。元数据分配失败返回 NULL；不会 reloc、写 guard bytes 或允许 alias/idempotent shortcut。
4. `amp_boot_fit` 在首次 payload copy 前保留 **全部 code 512 KiB + shared 128 KiB**。任一区保留失败，不 copy、不 release。一次 cold boot 只准一次有效启动尝试；失败后保留已成功的区域，不支持 hot retry。
5. standalone handler 检查完整 reservation 已成功，取消复制后的 BIN-sized 第二次分配。
6. `fs_read` 在 guard active 时解析 len=0 的实际文件大小，copy 前拒绝覆盖；未准备 AMP 时保持原路径。`booti_setup` 在 direct relocation 前检查 Linux Image source/destination。initrd 原址/relocation source/destination、FDT 含全部 padding 的 source/destination 另作显式 guard，检查先于任何写入。
7. FIT copy 除唯一、预检通过的 MCU 短暂 code 写入窗口外，后续 loadable 无权写 code/shared。函数返回后关闭例外。
8. `board_lmb_reserve` 导出两区至 bootm 的独立 LMB。**`arch/arm/include/asm/config.h:10/11` 定义 CONFIG_LMB/CONFIG_SYS_BOOT_RAMDISK_HIGH；不能因 autoconf.h 没该宏就说关闭。** 最终 ELF `nm` 确认 `board_lmb_reserve/amp_reserve_boot_lmb/boot_ramdisk_high/boot_relocate_fdt` 实际编入；没有执行板端 hook。
9. 显式入口先检查已准备 Linux DT 的 RAM 来源和完整 no-map 保留，最终 `image_setup_libfdt` 的正常 fixup 后再检查一次。stock DT、disabled parent/child、错误 cell/ranges、缺失/短小/移位区域、其它 owner overlap、reusable 不能满足。入口检查整个 FDT 不与 code/shared/FIT heap 相交，检查先于文件读取、payload copy 和任何 AMP SMC。

预留是软件约束，不是 MMU/firewall：任意 `mw/mm/cp/go`、raw block read、TFTP/network load、未知应用或另一路硬件解压仍可绕过它。首次测试只允许审核后的 named-boot filesystem → uncompressed Image → booti 路径；不执行这些其它命令，也不把 LMB 描述成能阻拦普通任意地址写。

### U-Boot 自身与 Linux 动态写入的覆盖

**SOURCE_VERIFIED + HOST_TESTED**，不是假设 malloc 服从 LMB：

| 对象 | 实际源码关系 | 保护机制 |
| --- | --- | --- |
| U-Boot code/BSS/GD/control DT/heap | `board_f:reserve_uboot → reserve_malloc → reserve_noncached → reserve_fdt → reserve_stacks`；`board_r:initr_malloc` arena=`[relocaddr−TOTAL_MALLOC_LEN,relocaddr)`，本 config malloc 32 MiB、noncached 1 MiB | `sysmem_init` MEM_UBOOT=`[gd.start_addr_sp,gd.ram_top)` 含全部 arena；AMP strict reserve 拒绝已有 framework 交集。普通 malloc 仍只在独立既定 arena 中，不改其 allocator |
| U-Boot stack | `sysmem_init` MEM_STACK=`[gd.start_addr_sp−CONFIG_SYS_STACK_SIZE,gd.start_addr_sp)`；最终 config `0x200000` | 整个 2 MiB 预留先存在；AMP strict reserve 拒绝交集。栈越过其既定容量属于栈损坏，软件 reservation 不是防溢出硬件 |
| Linux Image | `booti_setup` 依据 ARM64 Image header 算完整 image_size 与 relocation dst | source/dst guard 在 memmove 前；非法/wrap/与全部 code/shared 相交均拒绝 |
| initrd | `boot_ramdisk_high` 的原址和 relocation | source guard、独立 LMB 保留、dst guard 在 memmove_wd 前；LMB reserve 本身能合并，因此不能只依赖 reserve 返回值 |
| Kernel DT | `boot_relocate_fdt` 扩容 `of_size+CONFIG_SYS_FDT_PAD`，随后正常 fixups | padding overflow 和全范围 guard 先于 set_totalsize/open_into；最终精确保留检查防止 Linux buddy 重新领取 AMP DDR |

真实 Host Kernel DT 原件没有 `/memory` 节点；正常 `arch_fixup_fdt` 才用运行 gd 的 DRAM bank 填入。入口要求 gd 中整个 FDT 在同一真实 DRAM bank、严格 reservation 在真实 bank 内；允许此时 memory 缺失，**不发明/写入它**。最终 prep 强制 Linux `/memory` 包含 code/shared。实际 Host merged DT 入口校验 PASS；在未运行正常 fixup 的 Host 原件上 final-memory 校验返回 `-EINVAL` 是预期，不能把它称实板最终 DT 验证 PASS。

## 2. 参数与单一来源

唯一机器参数源：`docs/amp/AMP_PLATFORM_CONTRACT.yaml` 的 `host_proposal`；合同本身保持 `BLOCKED_PREBOARD`、final 未填。

`p026_generate_preload_contract.py` 复用既有 `generate_host_proposal.layout/generate`，同时生成：

- 派生 U-Boot `include/amp_project_contract.h`（记录合同 SHA）；
- FIT 的 `load/entry/rockchip,mcu-code-size/rockchip,mcu-shared-window-base/rockchip,mcu-shared-region`；
- 原 Host overlay、layout JSON、RTOS contract header。

**HOST_TESTED**：compiled header 与生成器 `--check` 完全一致。U-Boot 将经过验证的 FIT 元数据与 compiled expected geometry 比较；改合同后必须重生成、重编译，不使用四份手工地址。旧 P023/P025 FIT 无新预留字段，本 loader 会拒绝它，不可混用。

| Region | 未来拟 Linux PA | 拟 M0 view | 全部保留 size |
| --- | ---: | ---: | ---: |
| code/data/bss/heap/stack | `0x47800000` | `0x00000000` | `0x80000` |
| vring0 + vring1 + payload | `0x47d00000` | `0x27d00000` | `0x20000` |

以上是 Host proposal，不能说是当前 CON16/17 值。local Thumb entry `0x141`；vendor BUS M0 CODE setter/release 参数是 physical load `0x47800000`。

## 3. PROJECT_ADDITION 文件入口

**SOURCE_VERIFIED/HOST_TESTED，非厂商发布功能**：`amp_m0load <file> <linux_fdt_addr>`。

调用链：已准备 Linux DT 的 DRAM/完整保留校验 → 当前 `rockchip_get_bootdev` → 只允许 MMC → `part_get_info_by_name("boot")` → 以返回的真实 devnum/partition 选择 filesystem → `fs_size` → 有界 U-Boot heap allocation/与 DT 分离 → 重新选择 filesystem → 一次指定长度 `fs_read` → `amp_boot_fit` → 完整验证/预留/copy → 既有 checked cold-reset/SiP release。

- 使用现有名为 **boot** 的 filesystem；无需 `amp` GPT，也不移动 rootfs、不改分区。
- 不硬编码 partition number 或用户输入 staging PA。read length 与测得 file size 必须相等；截断、读失败、未知/过大文件不进入 startup。
- 本 first-test 派生头 `AMP_PROJECT_EXPLICIT_FILE_ONLY=1`：上电 `amp_cpus_on` 直接返回 0，**不自动读取 amp 分区**，即使未来板上有旧 amp 分区也不意外执行。
- 没有 default environment/boot.scr 自动触发；缺少文件不启动。实际部署可先放入 passive file，未来经批准在串口显式执行一次；本轮没有执行命令。
- 完整保留 P025 proper U-Boot board policy；未签名 FIT 若当前板要求签名，仍拒绝。未关闭 verification，不换 key、不写 OTP。

未来待审批的具体源例：Host `amp-host.itb` → `/boot/amp/amp.itb`（MMC boot filesystem 中 `/amp/amp.itb`）。先把 approved AMP Linux DT 装载至当前脚本的 FDT 地址 `0x48300000`，再显式 `amp_m0load /amp/amp.itb 0x48300000`；本报告不是命令执行批准。本机已观察 boot 挂载关系，但新目录尚未创建；**示例目的文件不存在，未上传。** U-Boot 替换属于 L3，主控需要逐对象 old/new hash、启动顺序和 rollback；不是仅 scp 文件即可使用现有原厂命令。

未来 Linux-only rollback 可先不执行 `amp_m0load` / 移除 passive file，且不需要 GPT 恢复；恢复原 U-Boot/Image/DT 默认链接仍遵循主控完整清单。若 loader/bootloader 自身坏到进不了 prompt，使用已确认的整机恢复流程，不能靠删除 file。

## 4. FIT header 的新发现

**SOURCE_VERIFIED**：pin 的 `fit_get_totalsize` 要求根节点自定义 `totalsize` property；标准系统 `mkimage` 不会添加。旧 P023 `amp-host.itb` 该 property 为 FDT_ERR_NOTFOUND，因此不能直接证明 vendor 分区 loader 能读它。

`p026_build_fit.py` 使用标准系统 mkimage `-E -p 0x1000 -B 0x200`，ITS 预分配 `totalsize=<0>`，签名前仅原位修正这四字节为完整文件长度。没有用 fdtput 重写整文件，避免 external data 被截掉。核：external payload 完全等于 M0 BIN、SHA256 value、load/entry/shared geometry、文件长度不变。后续若需要合法签名，必须在全部 metadata 完成后签，不把本 unsigned 文件当合法 signed package。

## 5. 源码、patch、构建身份

- 父项目 base：`dba800e`，branch `agent/amp-platform-closure`。主项目新增文件未由子代理 commit/push，交主控统一审核。
- 派生 U-Boot：`artifacts/local/p026-uboot-preload`，branch `agent/rk3576-amp-preload`。
- 父源码：`96c9a009eed997c318cd247943e5fc88e8e1bcc6`；vendor base `8f53f800da2c25d0c6ba414fb45902a01675703a`。
- 最终派生 commit：`2314a3f9f5795b88c7c53a805e9d59d82c6715b3`，源码 clean。中间 `2c115b49` 保留 branch `agent/rk3576-amp-preload-v1`、`53a364be` 保留 v4，不用于最终包。
- Patch：`patches/rk3576-amp-platform/0008-uboot-preload-and-explicit-fit-file.patch`，接 P025 0007，SHA256 `075c553f0812bbb2cfeb01cb6c7a3b1489bbfaf18e3a7452441903ee08ba2773`。
- 固定 RTOS/HAL reference 未修改；不修改 linker/M0 source/现有 Linux DTS。

| 实际 Host 命令/结果 | Evidence |
| --- | --- |
| `p026_generate_preload_contract.py --contract docs/amp/AMP_PLATFORM_CONTRACT.yaml --output artifacts/local/p026-host-inputs-final --uboot-source artifacts/local/p026-uboot-preload --check` exit0 | expected header/ITS 全部同源 |
| `p026_test_preload.py --source artifacts/local/p026-uboot-preload --fit artifacts/local/p026-fit-clean-v2/amp-host.itb --linux-fdt artifacts/local/p026-kernel-guard-fixture/merged.dtb --output artifacts/local/p026-preload-tests-final-v6` exit0 | 严格 reservation 20 + fault/order/file 113 + initrd/FDT 16 + 实际 vendored libfdt FIT/LinuxDT 54 项，另实际候选 Kernel DT+contract overlay 的入口检查；9 个静态顺序检查 |
| 实际新增 C 函数：Host `cc -std=c11 -Wall -Wextra -Werror` | PASS；非 MCU code 执行，firmware/file调用明确为 fault stubs；actual libfdt 的 FIT helpers 是薄属性适配器，Linux DT validator 为实际原函数 |
| `python3 -m unittest discover -s tests/python -p test_amp_p026_contract.py -v` exit0 | 合同改动传播/冲突拒绝 3 项 |
| `build_uboot_fit_policy_host.sh artifacts/local/p026-uboot-preload artifacts/local/p026-preload-clean-final-v6 enabled` exit0 | commit 固定后 fresh 全量 AArch64 U-Boot，cross gcc 11.4.0；v4/v5 只是中间构建 |
| `p026_build_fit.py --contract ... --bin artifacts/local/p023-m0-clean-v5/rtthread.bin --output artifacts/local/p026-fit-clean-v2` exit0 | fresh FIT，不复用旧 FIT；原 M0 BIN 不变 |
| `package_uboot_host_fit.py --original artifacts/local/p024-recovery-image/payloads/uboot.payload --build artifacts/local/p026-preload-clean-final-v6 --output artifacts/local/p026-loader-package-final-v6` exit0 | 新 U-Boot code，原 ATF1/2/3、OPTEE、control DT 全保留；4 MiB 双 2 MiB FIT slot |
| `git diff --check`；派生源 clean | PASS |

Fresh U-Boot warning 共 23：22 条 OpenSSL 3 Host RSA API deprecated，1 条 Host `bmp2gray16` 格式提示；同 P025，上游 Host 工具类。目标新增代码/linker 无 warning；不能称“全 build 零 warning”。Native LMB 不改源码，在单元 harness 对其既有 `-Wsign-compare` 单独 pragma，新增 reservation 仍 Wextra/Werror。

## 6. 新产物（不部署）

| File | Size | SHA256 |
| --- | ---: | --- |
| `artifacts/local/p026-preload-clean-final-v6/u-boot` | 11078488 | `e08f32a9baf98de44ff2f62bbb2d924f8414d06d52985ce2182776aa97a0e0f3` |
| `artifacts/local/p026-preload-clean-final-v6/u-boot.bin` | 1405368 | `6f0a67a2733df91b87ea855c05361e144b12dc9475219f3d69b990d4d5d0cd6d` |
| `artifacts/local/p026-preload-clean-final-v6/u-boot.dtb` | 9795 | `2eef029c7e599dc695b48f2bfdeabe0f2a87dda8588973750adaa8462edf9013` |
| `artifacts/local/p026-preload-clean-final-v6/.config` | 43570 | `838931f060c01c20f50c53e3be31fe6b1841cf89603d35d8d95bf8288f810d88` |
| `artifacts/local/p026-fit-clean-v2/amp-host.itb` | 126976 | `0c745d05a5404e004500445787c0319cee2edc7970e761508fc263fb1dbef6a2` |
| `artifacts/local/p026-loader-package-final-v6/uboot-host-unsigned.img` | 4194304 | `4b6e615beba307d95beff1cbbff76b06bfe794f9a2f269599d2ccd5026ca4f1f` |

M0 payload 保持 122696 bytes/SHA256 `a07e208836f5024fbc93674b62d4571dbb7ac6241789575228e40492626f2c78`。新 Ub FIT 2060800 bytes，2 MiB slot 内剩 36352 bytes，已核两 copy 结构和所有 payload identity。包装器旧固定 `missing_gate` 文本仍提 AMP GPT，不适用于本 P026 file-only 入口；主控应按本实际源码重新生成权威 manifest/changeset，不用该旧文本推导新增 partition。

## 7. 留给主控的事实门

本子任务只关闭 **Host copy 前完整内存保留 + 明确文件加载机制**。当前 proper U-Boot verified flag、实际 BL31/SiP 成功、CON16/17/cache 状态、Kernel/modules 联合部署身份、UART5 live/接线、写前旧件 hash 和真实 rollback 状态均由主控取得独立证据。

### 独立 D 判定意见

主控报告已取得 OP-TEE command5 的只读 `open/invoke=0,raw=0,required_flag=0`、当前 raw U-Boot 8 MiB hash、空闲 pinmux/3.3V TTL 接线及隔离 Kernel/modules clean build；这些不由本子代理再访问板验证，应引用主控独立原始记录。

**沿用用户原门槛时建议保持 C。** 同一 BL31 image 的静态 setter、TRM remap/reset定义及 checked cold-init 路径，能证明未来配置协议的输入/顺序/返回检查明确，不能单独证明当前 CON17 数值、MCU reset 已锁存映射或 cache 有效 bypass。Host 故障 stub 不替代这些运行证据。本补丁也没有运行 SMC。

最小余缺是 secure/合法读取路径的 CON16/17 和 reset/cache 有效态证据，或厂商对当前匹配 firmware 初始化效果的确定保证。后续若需诊断 firmware/启动链变更，主控应单列 L3 审批，保留原 BL31/bootloader恢复；不再次用 Linux devmem/U-Boot md 直读已 abort 的 SGRF。如果用户明确接受“checked cold-init、不依赖当前 remap 值”为等价准入证据，可单独改裁决依据；不能悄悄把 SOURCE_VERIFIED 写成 BOARD_OBSERVED。D 本身仍不等于部署授权。

必须先把本 patch/新 FIT 的身份、显式启动方式、原始 Linux 与 Host AMP DT/Image 顺序纳入最终 changeset；不得先 `amp_m0load` 然后以未 reserved 原 DT 启动 Linux。尚未发布部署许可，`deployment_authorized=false`；D 是“可以申请测试”，不是“已批准/已启动/AMP VERIFIED”。
