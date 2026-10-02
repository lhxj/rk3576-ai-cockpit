# LubanCat 官方 SDK manifest 与当前板镜像对齐

日期：2026-10-02。用户提供入口 `https://github.com/LubanCat/manifests`。本轮为 Host 下载、XML/ELF 解析和 hash/字节比较；没有板端访问，没有执行 SDK 脚本或固件。

## 固定 SDK 入口

本次将 manifest 冻结在 `db55f9658b2460b40e6e873d391e86d1b29e2916`（GitHub 页面与认证 API 结果一致）。[官方 README](https://github.com/LubanCat/manifests/blob/db55f9658b2460b40e6e873d391e86d1b29e2916/README.md) 指 RK3576/LubanCat-3 使用 `linux` 分支通用 SDK。

该快照的 Git symlink 链为 `lubancat_linux_generic.xml` → `lubancat_linux/lubancat_linux_generic_release.xml` → `lubancat_linux_generic_20260903.xml`。两个 symlink 的 Git mode 均为 `120000`。因此默认 repo init 会选九月清单，不能把它当作当前四月镜像的版本依据。

本轮采用历史 [20260424 清单](https://github.com/LubanCat/manifests/blob/db55f9658b2460b40e6e873d391e86d1b29e2916/lubancat_linux/lubancat_linux_generic_20260424.xml)，其 SHA256 为 `d5c67f672fba00d697224f92ee99b9b7e64320f89fdc3f5e93b187aeba52f821`，Git blob 为 `2ba88b0df00a256da147645c4f126267178667cc`。`remote.xml` 将 origin 指向 `https://github.com/LubanCat/`。下载文件的 Git blob hash 已与该固定 tree 逐项核对，**HOST_TESTED / SOURCE_VERIFIED**。

| Project | April release revision | 与当前板的关系 |
| --- | --- | --- |
| U-Boot | `8f53f800da2c25d0c6ba414fb45902a01675703a` | 与实机串口版本短 SHA 一致；实际 `.config` 仍未确认 |
| rkbin | `58a39b47f77a26a1e110fa1a1ce80bcfcb0b3505` | BL31 三个 load 段与当前 eMMC 提取文件完全匹配，见下表 |
| kernel-6.1 | `521833e2d28decbd6473d5717f1f96cc4108e208` | 四月 SDK 的精确源码候选；没有运行 Image 的完整构建 provenance，仍不能称精确 build match |
| device/rockchip | `ea5af0b2e5a48cc3d225c42c717923f70b1ac03d` | 板型配置及 AMP Host 构建入口 |
| Debian 12 | `54ce86d30a04471d5db082258c37d66798ff417d` | SDK 发布 pin；不证明当前 rootfs 无后续改动 |

## 实际 BL31 文件身份闭合

固定 rkbin [RK3576TRUST.ini](https://github.com/LubanCat/rkbin/blob/58a39b47f77a26a1e110fa1a1ce80bcfcb0b3505/RKTRUST/RK3576TRUST.ini) 指向 `bin/rk35/rk3576_bl31_v1.14.elf`。下载 ELF SHA256 为 **`e11e2c86320638b533492a3a78634fe2116f53da539d91dab84d219932fa07e7`**，Git blob `46ec18257f27d54f8902330fa64c8e83230d3f60`，349032 bytes。

Host `readelf -h -l`：ELF64、little-endian、AArch64、entry `0x40040000`。下表比较的是 ELF 的文件态 PT_LOAD payload（`p_filesz`），没有把 BSS/`p_memsz` 补零冒充原始镜像：

| ELF physical load | Payload bytes | 当前板 FIT 子镜像 | SHA256 | 比较 |
| --- | ---: | --- | --- | --- |
| `0x3fe70000` | 16384 | atf-3 / image-3.bin | `3af1bf762b5e866cc0ba28720cd2e51f5b105aaa23df29b886e2f1fad5ec9f1f` | **逐字节一致** |
| `0x40040000` | 184640 | atf-1 / image-1.bin | `1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07` | **逐字节一致** |
| `0x400f0000` | 20480 | atf-2 / image-2.bin | `8afb712810e97fe9069131cc996ff8e4dcc98e8dea0f75895e23cf5b29692524` | **逐字节一致** |

**HOST_TESTED**：自写 helper `artifacts/local/check_sdk_bl31.py` 从 ELF64 headers 提取 PT_LOAD，校验 AArch64、Git blob 与三个 payload 全字节 equality，exit 0；结果保存在忽略目录的 `lubancat-sdk-manifest-db55f965/verification.json`。没有运行下载的 ELF。

因此，本轮确认的是**该 SDK BL31 的可加载文件内容与已取得的板端 eMMC 镜像一致**。不能由此证明当前 CON17 值、启动时调用过哪项服务、SMC 动态成功，或整个 SDK 的源码/配置与运行系统完全一致。ELF 没有符号表，`nm` 报 `no symbols`；后续仍需受约束的反汇编/厂商源码。`atf-2` 对应 `.ddr_m0_bin` section，不能当作 BUS M0 echo 固件。

## SDK 中有什么，缺什么

- 固定 `device_rockchip` 的 `common/configs/Config.in.amp` 明确列 HAL/RT-Thread 的 MCU target；`common/scripts/mk-amp.sh` 从 ITS 取参数，设置 `RTT_PRMEM_BASE/RTT_SHMEM_BASE` 等并调用 SCons、打包 `amp.img`。本轮只阅读，未执行。**SOURCE_VERIFIED**。
- 该脚本依赖 `$RK_SDK_DIR/rtos/bsp/rockchip` 和 `common/hal`。已解析四月 generic/full 及 Buildroot include，**没有 RTOS、HAL、ATF/BL31 源码 project entry**；不能把“有 AMP 构建脚本”解释成这些源码已通过该 manifest 提供。
- `.chips/rk3576/LubanCat_rk3576_debian_gnome_defconfig` 选择 kernel 6.1、`lubancat_linux_rk3576_defconfig`、extboot 参数和 FIT；该文件没有选择 AMP。它不是板端实际生成 `.config` 的替代物。
- full manifest 的 Buildroot include 使用 `remote="internal"`，而此快照 `remote.xml` 仅定义 origin；未验证 full repo sync 可用，不尝试内部站点或补造权限。

## 下一步与裁决

现在可沿**与板端 load payload 相同的 BL31 文件**核 MCU config、CON17 写入责任、调用时序及被写的 CON14/15 等属性；固定 U-Boot 源码也有对应发布 pin。先做 Host 调查，不重新试 MMIO，不调用写映射 SMC，不选择新 linker/ITS/DTS 地址。

尚缺当前 CON17/有效映射、实际 U-Boot AMP 配置与加载路径、coherency、最终物理布局和完整恢复条件；kernel pin 还需与实际 Image 构建记录核对。**C. HOST_BUILD_PASS**，没有升级 D。

机器可读结果见 [SDK_BASELINE_IDENTITY.json](SDK_BASELINE_IDENTITY.json)。厂商文件和源码片段保留在忽略目录，不 vendor 或提交进主仓。
