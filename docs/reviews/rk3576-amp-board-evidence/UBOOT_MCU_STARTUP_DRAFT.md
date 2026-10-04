> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# BUS M0 启动补丁草案与 Host 检查

日期：2026-10-02。状态：**DRAFT_NOT_DEPLOYABLE / C. HOST_BUILD_PASS**。本轮仅 Host 源码修改、模拟测试和交叉对象编译，无板端访问。

## 1. 来源与范围

- 固定 U-Boot：`8f53f800da2c25d0c6ba414fb45902a01675703a`。
- 派生分支：`agent/rk3576-mcu-startup-draft`；提交：`7daeb0fc8ad0818a833b162405a35d0511d767bb`。
- 派生目录：`artifacts/local/uboot-mcu-startup-draft/`，主项目忽略；原 `uboot-pinned` 未修改。
- 补丁：[0001-amp-require-explicit-BUS-MCU-shared-mapping-and-prop.patch](../../../patches/rk3576-amp-platform/0001-amp-require-explicit-BUS-MCU-shared-mapping-and-prop.patch)。身份、测试及对象 hash 见 [UBOOT_MCU_STARTUP_DRAFT_RESULT.json](UBOOT_MCU_STARTUP_DRAFT_RESULT.json)。
- 原问题依据：[CON17_BOOT_CALL_PATH.md](CON17_BOOT_CALL_PATH.md)：CODE SMC 返回值被忽略、没有 shared setter 调用、standalone 失败未传回上层。

## 2. 参数与启动顺序

**SOURCE_VERIFIED（派生源码）**：`drivers/cpu/rockchip_amp.c` 将 FIT image 的节点及 blob 传给新 hook `fit_standalone_release_with_config()`。其他 SoC 默认 weak hook 仍转交原 hook。

RK3576 BUS M0 要求 image 节点提供项目自定义属性 `rockchip,mcu-shared-window-base`：恰好一个 32-bit 大端 cell，非零、1 KiB 对齐。**这不是厂商已有 binding**。它表示 CON17 的 system physical window base **B17**，不是 vring0 或共享池的 Linux PA。没有缺省值，未填入实际地址，也未修改 ITS、linker、DTS 或 contract 的 null 地址。

M0 `0x27d00000` 对应 `B17 + 0x07d00000`；本轮不能给 B17 赋值。拒绝零是本项目草案的保守策略，不是声称 BL31 禁止零。

顺序：

1. 验证参数；CODE base 必须为 32-bit、`>0x00800000`、1 KiB 对齐。这些结构检查对应已分析 BL31 CODE 路径，不是 DDR 有效性证明。
2. 调用 `sip_smc_mcu_config(BUSMCU_0, CODE_START_ADDR, base)`，检查返回值。
3. 调用 `sip_smc_mcu_config(BUSMCU_0, SRAM_START_ADDR, B17)`，检查返回值。宏名称是 SRAM，但对应板端 BL31 selector 3 的 CON17 写入，不代表实际指向片上 SRAM。
4. 两次都返回零才执行原厂 gate/reset release 写入。
5. 错误通过 `standalone_handler → brought_up_amp → brought_up_all_amp → amp_cpus_on` 返回；`board_late_init()` 记录错误后继续原 Linux boot 流程。

接口沿用 vendor standalone 分支传入的 **FIT load** 作为 CODE remap base；不是读取独立 FIT entry，也不是 M0 本地 PC。该地址语义未改变，最终 load/entry 仍须与 linker 和 M0 reset/vector 另行闭合。

旧的无配置 BUS M0 hook 返回 `-EINVAL`，避免绕过必需参数；同时初始化 `data_size`，拒绝非正长度、缺 description/load 和分配失败。

## 3. Host 验证

**HOST_TESTED**：测试脚本从实际派生 C 文件提取所修改函数，用 Host mock 替代 FDT、SMC 和 MMIO。24 项检查通过：23 项 BUS M0/上层错误检查，加 1 项 weak hook 向其他 SoC 旧 hook 转交参数和返回值的检查。

覆盖缺属性、错误 cell 长度、零/未对齐 base、无效 code base、两阶段 SMC 失败、异常正返回值、legacy hook 拒绝、缺 description/load、无效长度、长度查询失败、分配失败，以及成功时 CODE→shared→gate→reset 的顺序。所有测试地址都是合成 fixture，不是拟部署布局；执行的是自编 Host 测试程序，没有运行固件或真实 SMC。

```sh
python3 scripts/amp/test_uboot_mcu_startup_draft.py \
  --source artifacts/local/uboot-mcu-startup-draft \
  --report artifacts/local/uboot-mcu-startup-final-check/host-mock.json

python3 scripts/amp/build_uboot_mcu_startup_objects.py \
  --source artifacts/local/uboot-mcu-startup-draft \
  --output artifacts/local/uboot-mcu-startup-another-fresh-check
```

构建脚本拒绝复用既有输出目录。此次新目录为 `artifacts/local/uboot-mcu-startup-final-check/`。

**HOST_TESTED**：`rk3576_defconfig → AMP fragment → reviewed merge_config.sh -m → olddefconfig`，生成 `CONFIG_AMP=y`、`CONFIG_ROCKCHIP_AMP=y`。`rk3576.o / rockchip_amp.o / board.o` 均 exit 0，readelf 确认 AArch64 relocatable object，标准对象构建日志无 warning/error。没有链接完整 U-Boot，没有生成可启动镜像。

工具链为已安装的 Ubuntu `aarch64-linux-gnu-gcc 11.4.0`。板端串口记录是 GCC 10.3.1，本次不能称为精确复现板端二进制。

Host mock 使用 `-Wall -Wextra -Werror`，对旧通用 vendor 代码显式排除 `unused-parameter/sign-compare/type-limits`。初次严格编译揭示原 `u8 arch/type == -ENODATA` 恒假：是非 standalone 分支的既有缺陷，本轮未修，不声称全工程 `-Wextra` 无 warning。函数指针不匹配没有豁免。

`git apply --check` 在原 pinned tree 上 exit 0，未应用。常规 `bash scripts/dev/host_ci.sh` exit 0：2 项 CTest、6 项 Python 测试通过；它们是主项目骨架回归，不替代本次单独运行的启动 mock。

## 4. SDK 配置依据

**SOURCE_VERIFIED**：固定 device_rockchip `ea5af0b2e5a48cc3d225c42c717923f70b1ac03d`：

- `common/configs/Config.in.loader` 的 `RK_UBOOT_CFG` 无特殊 SoC 分支时 default `RK_CHIP`；允许 `RK_UBOOT_CFG_FRAGMENTS`。
- `common/scripts/mk-loader.sh::build_uboot()` 将这两项交给 `./make.sh`，并传 SPL/安全启动等选项。本轮只读取，没有执行 SDK wrapper。
- 已取得的 LubanCat RK3576 Debian gnome defconfig 未显式覆盖 U-Boot cfg/fragments。

**HOST_TESTED**：固定 U-Boot `rk3576_defconfig` 生成的 base config 为 `# CONFIG_AMP is not set`。

**SOURCE_INFERRED**：与当前板 U-Boot payload 缺 AMP 字符串吻合，支持“默认未启用 AMP”的判断。

**UNVERIFIED**：当年完整 SDK 最终展开配置、全部 fragment/选项和 GCC 10.3.1 输入未取得；不能把本次 Host config 当作当前板实际 `.config`。

## 5. 限制与剩余门禁

- CODE 成功、shared 失败时可能已写 CON14/15/16；仅保证本次 hook 不继续 release，不保证寄存器回滚或原子事务。
- 仅考虑 MCU 事先处于 reset 的单 BUS M0 冷启动；该初始状态未验证，不支持热重启。SMC 返回零不等于读回或 remap 经 reset 生效证据。
- FIT loadables 在 hook 前已装入 RAM；失败时不是“没有任何内存写入”。reservation 与 loader 范围检查仍待闭合。
- 继续 Linux 不代表多个 loadable 全部回滚，也不保证此前启动的其他 AMP 核已停止。
- DDR 有效范围、窗口加法溢出、镜像/vring 重叠、CON14/15 cache 影响与签名授权尚未通过 contract 校验。
- 原 loader 仍读取 GPT `amp`；本轮没新增文件加载入口或分区。
- 没取得当前 CON16/17 值，没证明当前 U-Boot 已有 AMP，没动态验证 BL31 SMC；恢复/离线镜像仍有缺口。

补丁可供审查和 Host 回归，**不得部署、不得启动 M0、不得升级 D**。下一步先闭合显式配置所需的物理 reservation、uncached/coherency 与 boot/recovery，再决定是否产生经过审批的启动镜像。
