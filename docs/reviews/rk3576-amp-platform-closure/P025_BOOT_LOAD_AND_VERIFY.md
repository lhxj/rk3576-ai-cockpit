> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# AMP FIT 入口与验签：静态链闭合，当前板策略未确认

2026-10-03；SOURCE_VERIFIED/HOST_TESTED与BOARD_RUNTIME分开。**C. HOST_BUILD_PASS**。

## 用户串口日志补充（P025后续取证）

完整日志已提供，无需重复请求或重新上电。SPL明确`Verified-boot:0`并经六个SHA检查进入U-Boot：**BOARD_OBSERVED_USER_REPORT，记录的SPL FIT不要求签名**。后续正常`booti`，没有proper U-Boot AMP policy调用/输出。固定源码中proper阶段另经OP-TEE读取flag，失败按required处理；`security partition`缺失提示属于文件存储初始化，不证明验签关闭。**proper U-Boot AMP policy/key仍UNVERIFIED**，新unsigned包仍不可部署。证据/源码行号见 [串口验签证据](../rk3576-amp-board-evidence/SPL_VERIFIED_BOOT_SERIAL_EVIDENCE.md)。下文“当前策略未确认”专指AMP阶段，不再包含已取得的SPL记录。

## 固定源码和现有实机固件

| Evidence | Repository/commit/file/function | 已证明范围 |
| --- | --- | --- |
| SOURCE_VERIFIED | LubanCat/uboot `8f53f800da2c25d0c6ba414fb45902a01675703a`，`arch/arm/mach-rockchip/board.c:board_late_init` | CONFIG_AMP在late_init调用loader，早于命令prompt |
| SOURCE_VERIFIED | 同pin `drivers/cpu/rockchip_amp.c:amp_cpus_on`，AMP_PART=`amp` | 按GPT分区名寻找、blk_dread读header/完整FIT；无可配置filesystem路径 |
| SOURCE_VERIFIED | `common/image.c:boot_get_loadable`；`common/image-fit.c:fit_image_load` | AMP走IH_TYPE_LOADABLE；kernel-only分支才调用配置验签/板级policy，`images.verify=1`不能单独覆盖配置 |
| SOURCE_VERIFIED | `arch/arm/mach-rockchip/fit_misc.c:fit_board_verify_required_sigs` | SPL读secure OTP；proper U-Boot的OPTEE_CLIENT路径读trusty verified-key flag，失败按required处理；其它路径查ATAG pubkey flag |
| SOURCE_VERIFIED | `common/image-sig.c:fit_config_verify_required_sigs`，490–519 | 缺signature node报错；若存在node但无required=conf key，循环可直接返回0，不能证明满足required policy |
| SOURCE_VERIFIED | `common/image-sig.c:fit_config_verify`，522–525 | 从gd_fdt_blob中的信任公钥验证FIT配置 |
| BOARD_OBSERVED_READONLY | 恢复后 `/proc/cmdline`、lsblk、运行DT/日志 | 相同fwver，只有uboot/boot/rootfs，无AMP节点，没有可见verified flag |
| SOURCE_VERIFIED | 原恢复包uboot FIT/控制DT、已取旧实板prefix | 控制DT SHA=`2eef029c7e599dc695b48f2bfdeabe0f2a87dda8588973750adaa8462edf9013`；FIT signature只有metadata无value，不证明OTP/policy关闭 |

从BootROM/DDR/SPL进入U-Boot时BL31/OP-TEE作为uboot FIT原payload装载；AMP是**之后的另一份FIT**，不能混淆。当前版本字符串/先前payload静态一致不证明当前CONFIG_AMP开启，也不证明SMC运行可用。

`AMP_PART`是GPT label，不是SDK逻辑文件名或resource subimage。当前官方parameter为uboot从sector`0x4000`、boot从`0x8000`、rootfs从`0x48000`grow-to-end。包内没有amp；不能把不存在的分区写作既有加载目标，也不能自行从rootfs切出一块“空闲空间”。本pin未找到官方file/FIT-memory MCU release路径；普通`boot_fit/bootm`或能加载文件不等于AMP。

## P025 项目补丁（非厂商发布）

派生worktree `artifacts/local/p025-uboot`，branch `agent/rk3576-amp-fit-policy`；从P023`87f467be568f1189dce4b6eb65ab138279311984`追加两提交，最终`96c9a009eed997c318cd247943e5fc88e8e1bcc6`。主仓保存 `patches/rk3576-amp-platform/0007-uboot-fit-policy-and-partition-bounds.patch`，按mailbox含两commit，接在0001/0006之后，不修改reference。

1. header读取前及整体FIT读取前，检查block size、partition大小、round-up/有符号长度溢出；超过amp分区拒绝，不继续load/release。
2. `boot_get_loadable`前执行板级policy和config检查；required但无CONFIG_FIT_SIGNATURE拒绝。
3. required但控制DT没有`required="conf"`公钥拒绝；不能靠验签函数“空key列表返回0”放行。
4. 存在required-conf key时走标准`fit_config_verify`；任何失败拒绝。无required policy且无required key，仅保持image hash验证；该路径必须先有实际policy证据，不能据此发布unsigned部署包。
5. 不写OTP，不换信任key，不关闭verified boot；沿用P023cold-reset/SiP错误传播。合法key若需要，必须由已有信任链授权，不能随意生成一个dev key冒充板上信任。

Host C测试：signature支持开/关两变体，`-Wall -Wextra -Werror`，46个policy/边界条件+3条调用顺序检查PASS。完整fresh U-Boot AArch64 build exit0；CONFIG_FIT_SIGNATURE/OPTEE_CLIENT/AMP/ROCKCHIP_AMP=y。测试不能证明RSA在板端成功，当前控制DT无新增key，required策略会明确拒绝本unsigned包。

## Host package 身份

`package_uboot_host_fit.py`先核恢复uboot payload SHA=`06f48ed3716ccdde6adcb3187ca3eff766927e27bf6b7f432046539b782c4a56`，原双2MiB slot相同；标准mkimage external data start=`0x1200`，512B对齐。替换nodtb U-Boot code；ATF1/2/3、OPTEE、FDT和各load/entry/arch/config metadata逐字节保持。产物FIT 2,056,192B，4MiB双slotpackage SHA=`c6448d11b130fab7154be4edebc6b3d8ee9508f056a7f86d0f8a04b832c98488`。

**HOST_PACKAGED_NOT_DEPLOYABLE / SIGNED=false。** 没有执行该firmware；没有调用SDK预编译打包工具；未生成合法签名。Hash检查只提供文件完整性，不能代表来源认证。完整artifact/code/config/build log SHA见P025 manifest。

## 最终 gate

- 官方加载来源：`amp` GPT partition，SOURCE_VERIFIED；当前不存在，AMP_PARTITION_REQUIRED_FOR_PINNED_LOADER。
- 当前U-Boot AMP开启/正确release：UNVERIFIED，不能由新Host候选代替。
- 已有BL31包含所选MCU setter分支：SOURCE_VERIFIED；实际SMC/reset/effective mapping：UNVERIFIED。
- 当前板verified policy/授权key：UNVERIFIED；unsigned候选不会因Host build成功获部署许可。
- U-Boot复制前完整RAM reservation：BLOCKED，不能用Linux no-map代替。
- 不开APPROVAL_GATE_BOARD_TEST，不创建分区，不上传/烧写候选。
