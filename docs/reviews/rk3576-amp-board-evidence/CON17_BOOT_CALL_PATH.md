# CON17：已匹配固件中的配置调用链

2026-10-02，P020。只分析 Host 文件，没有 SSH、MMIO、SMC 或板端操作；没有修改 linker/ITS/DTS。等级 **C. HOST_BUILD_PASS**。

## 结论

**已证明的静态能力：** 对应板端的 BL31 payload 有 BUS M0 CON17 setter，值由调用方传入；固定 U-Boot RK3576 release 路径只调用 CODE selector，未调用该 shared-window setter。**没有获得当前 CON17 值，没有找到能确定它的默认常量或完整冷启动赋值链。**

当前 U-Boot payload 中没有找到该公开 AMP loader 的关键字符串，构成“当前镜像可能未编入该 loader”的 **SOURCE_INFERRED** 证据；实际 `.config` 仍 **UNVERIFIED**。不能把新增 DT、放入 FIT 或拥有 SDK 源码视为已经具备可执行的 M0 启动路径。

## 固定证据与复核

- U-Boot 源：`LubanCat/u-boot@8f53f800da2c25d0c6ba414fb45902a01675703a`，跟踪文件无本地修改。
- 已提取 U-Boot payload SHA256：`c084f257e761b898c45d681a2d1b739ca2cdd99b3916b64f3e9b34109d7a865d`。
- 已提取 BL31 atf-1 payload SHA256：`1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07`。已与四月 SDK BL31 ELF 对应 PT_LOAD 全字节匹配，见 [SDK_MANIFEST_EVIDENCE.md](SDK_MANIFEST_EVIDENCE.md)。
- 本次 [Host verifier](../../../scripts/amp/verify_con17_boot_evidence.py) 核 SHA、源码 commit/调用点、选定原始指令及 branch/store 地址；正向 exit 0。把 U-Boot bin 错传为 BL31 的负向检查 exit 1，明确拒绝错误 hash；`py_compile` exit 0。不是整个固件的控制流或运行时证明。
- 机器结果见 [CON17_BOOT_PATH_RESULT.json](CON17_BOOT_PATH_RESULT.json)。

## 1. U-Boot 调用顺序及参数来源

以下来自固定源，**SOURCE_VERIFIED**：

| 顺序 | 文件 / 函数 | 行为与边界 |
| --- | --- | --- |
| 1 | `arch/arm/mach-rockchip/board.c:512–513` / `board_late_init()` | 仅 `CONFIG_AMP` 下调用 `amp_cpus_on()`；当前镜像此配置未知 |
| 2 | `drivers/cpu/Makefile:10`、`Kconfig:16–18` | loader 对象由 `CONFIG_ROCKCHIP_AMP` 选入，依赖 `AMP/ROCKCHIP_SMCCC/RKIMG_BOOTLOADER` |
| 3 | `drivers/cpu/rockchip_amp.c:444–518` / `amp_cpus_on()` | 查启动设备上名为 `amp` 的 partition；读取 FIT、验证/装载 loadables、flush A-core D-cache |
| 4 | 同文件 `brought_up_all_amp()` → `brought_up_amp():263,278` | standalone 使用 FIT **load** 传给 `standalone_handler()`；这里未读取独立 FIT `entry` 来决定 MCU 映射 |
| 5 | 同文件 `standalone_handler():225–241` | 分配/检查 load 区域，调用 SoC `fit_standalone_release(description, load)` |
| 6 | [`rk3576.c:255–274`](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/arch/arm/mach-rockchip/rk3576/rk3576.c#L255-L274) | description 为 `bus_mcu` 时：CODE selector 1 → CRU gate 写 `0x5c000000` → reset 写 `0x38000000`；没有 SRAM selector 3 |
| 7 | `rockchip_smccc.c:182–187` | `sip_smc_mcu_config()` 发 SMC `0x82000028`，返回 `res.a0`；SoC release caller 未检查此返回值 |

因此参数名 `entry_point` 在这条链上实际指 **FIT 物理 load / code 映射基址**，不能把它等同于 ELF `e_entry` 或 M0 视图的 PC。有效 M0 reset/vector 映射仍依赖 MCU reset 与硬件窗口，当前没有启动证据。

全仓跟踪 C/H 搜索 `MCU_SRAM_START_ADDR` 只有 header 的 `0x03` 定义，没有 caller；搜索 `sip_smc_mcu_config()` 的 RK3576 caller 只有上表 CODE 调用。未发现可由 ITS 中 `shm_base/rpmsg_base` 自动驱动 CON17 的 loader 属性或调用；SDK 构建脚本导出这些参数给编译器不等于启动时配置寄存器。

**必须记录的错误传播缺口：** `fit_standalone_release()` 忽略 SMC 结果后继续放开时钟/reset；其上层 `brought_up_amp()` 又忽略 `standalone_handler()` 返回值并走成功出口。未来 changeset 必须在 MCU release 前检查 MCU config 结果并向上传播失败，不能用 `Handle standalone ... OK` 单独证明映射配置成功。本次只记录，未修改第三方源码。

## 2. 对应板端 BL31 的 CON17 setter

地址均为以 atf-1 load `0x40040000` 换算的 Host VMA，**HOST_TESTED**；高层 ABI 与职责解释为 **SOURCE_INFERRED**：

| 指令位置 | 复核结果 |
| --- | --- |
| `0x4005eb28/2c/38/40` | 保留 SMC 调用号；原 x1 MCU ID → x0，原 x2 selector → x1，原 x3 value → x2 |
| `0x4005ebb0–ebbc` | 构造并匹配 `0x82000028`，转到 MCU handler `0x4005ec98` |
| `0x4005ec98–eca8` | MCU ID 必须为 0；selector 1 → CODE，selector 3 → `0x4005ed04` |
| `0x4005ed04/08` | 检查 `value & 0x3ff == 0`，不满足返回 -4 |
| `0x4005ed0c/10/14` | 构造 `0x26004064`，`str w2,[x0]` 写入 **参数低 32 bit** |
| `0x4005ed18 → 0x4005ecfc` | 转成功返回 0；该分支没有 read-back/compare 或 MCU reset |

这是一条 **写入接口**，不是读取入口，不调用来试旧值。它没有内置 CON17 默认值；对超出 32-bit 范围的参数存在截断风险。selector 1 另外会写 CON14/15/16，详见 [MCU_MAPPING_ALTERNATIVE_PATH.md](MCU_MAPPING_ALTERNATIVE_PATH.md)，未来必须一起审 cache/属性。

## 3. 包含 CON17 的表是保存/恢复表

新复核到 EL3 文件中的表和使用路径（**HOST_TESTED**）：

- 表 `0x40069930` 共 47 行，每行 24 bytes；第 12 行 `0x40069a50` 为 `{start=0x2600403c, end=0x26004064, stride=4, OR-mask=0, saved-buffer-pointer=0}`。区间包含 CON16 和 CON17。
- `0x4005a3e8–a3f4` 把表及行数送入 `0x40055424`；该函数在 `0x40055498` 写入运行时保存 buffer 指针。
- `0x40059c60–9c6c` 调用 `0x400554cc` 保存：`0x40055500` 从寄存器地址读取 W 值，`0x40055508` 存入 buffer。
- `0x40059c78–9c98` 调用 `0x4005551c` 恢复：`0x40055554` 读取 buffer，`0x40055564` 写回寄存器；此行 OR-mask 为 0。

所以文件中末尾的零是**待运行时填入的 buffer 指针**，不是 CON17 reset value 或默认映射。恢复路径的值来自之前保存的状态，不提供初次 cold boot 的固定地址。这些代码是安全固件读写意图证据，**未观察本次运行触发，不能授权 Linux/U-Boot EL2 直接读取 SYS_SGRF**，也不能单独保证软件读属性。

没有取得匹配 ATF 源码或符号表。本次是 selected-path 分析，不能排除其它间接写入者、更早 DDR loader/BootROM、其它可信固件或后续覆盖；当前写入责任与有效状态仍未知。

## 4. 当前 U-Boot AMP 能力线索

在已提取完整 U-Boot payload 中，ASCII 查找 `bus_mcu`、`Handle standalone:`、`Brought up amps`、`Load loadables, ret=%d`、`standalone:` 均未命中，**HOST_TESTED**。固定源码的 `configs/rk3576_defconfig` 也未显式选择 AMP。

这与“当前未编入该 AMP loader”相符（**SOURCE_INFERRED**），但未取得实际构建 `.config/map`，不能仅用缺字符串宣判所有 AMP 能力不存在。公开源中的 loader 存在并不证明当前二进制选入；当前无 `amp` partition 也不可能使这条公开 `amp_cpus_on()` 路径进入 payload release。

## 裁决与后续工作

| 问题 | 本次结果 |
| --- | --- |
| BL31 有 CON17 setter 吗 | **HOST_TESTED**：有目标写分支；运行时调用成功未测 |
| 对应公开 U-Boot MCU release 会调用吗 | **SOURCE_VERIFIED**：没有 shared selector caller，只配置 code |
| 能由文件确定当前 CON17 吗 | **BLOCKED**：参数型 setter、保存表均不提供当前/默认值 |
| 能证明 M0 有效 DDR mapping 吗 | **UNRESOLVED**：仍缺确定参数、写入/soft-reset 顺序和生效证据 |
| 能据此选新 linker/ITS/DTS 地址吗 | **不能**；保持现有 artifact 不变 |

下一条具体 Host 工作应是准备**明确 shared mapping 参数且检查 SMC 返回值的启动 changeset**，先补 U-Boot 实际构建配置/可恢复镜像来源、共享物理区 reservation 依据与 cache 属性；参数未闭合前不填猜测地址。也可向厂商索取该 SDK/BL31 的 BUS M0 初始化示例、CON17 默认/配置说明和生成 `.config`。不再重试直接 MMIO，也不通过试不同 SMC 来读旧值。没有实板部署许可，不升级 D。

## 复核命令

在 project 根目录，Host 执行：

```sh
python3 scripts/amp/verify_con17_boot_evidence.py \
  --uboot-source artifacts/local/uboot-pinned \
  --bl31-bin artifacts/local/amp-uboot-dump-20261002/image-1.bin \
  --uboot-bin artifacts/local/amp-uboot-dump-20261002/image-0.bin
```

检查只运行自写 Python 与 Git。它不打开 `/dev/mem`，不连接 SSH，不执行输入固件。
