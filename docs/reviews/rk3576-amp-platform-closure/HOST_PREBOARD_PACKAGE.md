# P023：上板前 Host 候选包

2026-10-02。**C. HOST_BUILD_PASS；DRAFT_NOT_DEPLOYABLE。** 本文补充历史审查，不把 Host 方案当成当前板配置。

## 已完成

| 项目 | 结果 | 证据等级 |
| --- | --- | --- |
| 最小 BUS M0 echo | clean SCons exit 0，BIN 122,696 bytes，ARMv6-M / EABI5 / soft-float，entry `0x141`；无 warning | HOST_TESTED |
| RPMsg | link `0x04`、Linux master / M0 remote、NS `rk3576-m0-echo`；既有 callback 修复保持 | SOURCE_VERIFIED / HOST_TESTED |
| cache 候选 | 冷 reset 后保持整个 MCU cache bypass；Linux 专用 no-map pool + device barriers；失败拒绝 fallback | SOURCE_VERIFIED / HOST_TESTED；运行时未测 |
| UART5 | 官方规格书确认 40Pin 16=GPIO3_D4/RX、18=GPIO3_D5/TX；当前 DT 无启用的两脚使用者 | SOURCE_VERIFIED / BOARD_OBSERVED_READONLY |
| DTS/FIT | overlay、对原 DTB/运行 DT 的 Host 合并、FIT 提取与 BIN 比较通过 | HOST_TESTED |
| Linux | 对当前板复制的 6.1.99 headers clean Kbuild：transport 对象与 echo ko 通过；编译器版本差异记录 | HOST_TESTED；非 BOARD_MODULE_READY |
| U-Boot | AMP-enabled 完整 `u-boot.bin/ELF/DTB` clean build 通过；冷 reset/SMC 错误传播和 Linux boot 保留检查 26+1 项通过 | HOST_TESTED；不是已封装/签名的 bootloader 镜像 |
| 合同 | 实际 ELF/FIT/DTB/RTOS header 与服务名等 9,672 项检查；实际地址函数 65,536 字节互逆与越界检查 | HOST_TESTED |
| 原件 | `/boot` 原始 Image、DTB、uEnv、boot 脚本、config/initrd/System.map 等及运行 DT 已只读复制到 Host | BOARD_OBSERVED_READONLY |

canonical 参数源为 `docs/amp/AMP_PLATFORM_CONTRACT.yaml`。其中 `host_proposal` 描述**未来显式配置**，其余 final 字段仍为 null；生成器不会开启部署 gate。Host 候选内存布局见 [MEMORY_LAYOUT_HOST_PROPOSAL.md](MEMORY_LAYOUT_HOST_PROPOSAL.md)。产物和完整 SHA256 见 [HOST_PACKAGE_MANIFEST.json](HOST_PACKAGE_MANIFEST.json)。

## 派生代码及复现

| 来源 | 固定基线 | 本轮派生提交 |
| --- | --- | --- |
| RTOS | `7c397f41751feb29b0b388dfda3d2c2225f1f87c` → 前轮 echo `1d0de06c394f89be35a4b6966e766b56f035c19d` | `3a39b0f4f5eb25631d05b4ffd88746ce9b1b0cdb` |
| HAL | `277de3fd4b0e640654ee73bb3308be2ef01e3aad` | `bc99978c1a030ad79610e89a5780dcd0ee3bb1f2` |
| U-Boot | `8f53f800da2c25d0c6ba414fb45902a01675703a` → 前轮 setter `7daeb0fc8ad0818a833b162405a35d0511d767bb` | `87f467be568f1189dce4b6eb65ab138279311984` |
| Linux transport | `521833e2d28decbd6473d5717f1f96cc4108e208` | 单文件 patch `0003`；不是完整运行内核源码身份确认 |

`patches/rk3576-amp-platform/0003..0006` 保存本轮修改；`0003` 从固定 Linux 文件直接应用，已经包含 `0002` 的 barrier 修改，**不要再叠加 0002**。`0004` 应用于 RTOS 前轮 echo，`0005` 应用于固定 HAL，`0006` 接在 U-Boot `0001` 后。HAL symlink 在派生 RTOS 中为 `../../../../p023-hal`，Git mode 保持 120000；reference 两仓 SHA/状态未改。

配置从 `board/evb/defconfig → .config → scons --useconfig=.config → rtconfig.h` 生成，实际 SCons 使用 `rtconfig.h`。I2C3/6/7/8、ADC、GPIO driver、串口 DMA、共享 TIMER11 均关闭；UART5 pinmux 仍启用。最小方案使用 MCU 私有 24 MHz SysTick。

```sh
python3 scripts/amp/generate_host_proposal.py --contract docs/amp/AMP_PLATFORM_CONTRACT.yaml \
  --output artifacts/local/p023-host-inputs --entry 0x141
# 把生成 header 放入派生 BSP；下面是已执行的构建流程摘要。
cp board/evb/defconfig .config
scons --useconfig=.config
scons -c
scons -j4
python3 scripts/amp/test_uboot_mcu_startup_draft.py \
  --source artifacts/local/p023-uboot --cold-proposal --report artifacts/local/p023-uboot-cold-mock.json
bash scripts/dev/host_ci.sh
```

M0 工具链 `arm-none-eabi-gcc 13.2.1`；Linux/U-Boot `aarch64-linux-gnu-gcc 11.4.0`；Host dtc/mkimage 的版本和完整命令/log hash 记录于 manifest。M0 构建时 `RTT_ROOT` 指派生 RTOS，`RTT_EXEC_PATH` 指 M0 工具链。未执行厂商预编译打包工具或固件。

## 还不能进入 D 的具体原因

1. 当前 CON16/17 没有有效读数。新的 Host setter/reset 方案能确定**拟配置**值，不能证明实际固件调用成功或当前映射。没有重试 MMIO/SMC。
2. 当前 U-Boot 的 AMP loader 仍未证明存在；候选二进制不是当前板镜像。公开 loader 只找 GPT `amp`，板上没有该分区；尚无已验证的无分区加载入口。
3. 当前 BL31 payload 已与 SDK 对应文件字节匹配，配置分支有静态证据；目标 SMC 的动态可用性、FIT/secure-boot 签名策略没有闭合。
4. 新 Linux transport 是 built-in 修改，echo ko 不能替代它；运行 Image 的精确源码/build identity 未闭合。候选源码构建只证明未来版本可构建。
5. 当前 DT 无 UART5 引脚冲突，实时 pinmux 文件对普通用户拒绝读取；USB-TTL M0 接线/电平仍需实际确认。
6. 官方恢复镜像尚为 `.downloading` 临时文件；未取得完整 `update.img`、其 loader/parameter/partition payload 和工具包。恢复路线见 [RECOVERY_PACKAGE_EVIDENCE.md](RECOVERY_PACKAGE_EVIDENCE.md)。

本轮 board 动作为普通用户 SSH 文件读取及复制到 Host；没有写板、上传候选产物、加载 ko、重启或启动 M0。**APPROVAL_GATE_BOARD_TEST 尚未开启。**
