> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# Shared memory / cache 路径证据（P022，2026-10-02）

**结论：`STATIC_HOST_EVIDENCE_PASS`；板端 coherency 仍 `UNVERIFIED`，整体仍 C。** 本轮没有 SSH、MMIO、SMC 或板端变更，没有修改 RTOS 配置、linker、ITS、DTS 和最终地址。

## 固定输入

| 输入 | 固定身份 | 用途 / 证据级别 |
| --- | --- | --- |
| 用户 Part1 TRM v1.2 | SHA256 `6094ae5874d8494e73fa363d9cf35dd65acbd54a9a9d633b1ba5e4bea289f0a8` | p750/762/765/766/772；`SOURCE_VERIFIED`，见 [原文核验](TRM_SOURCE_RECONCILIATION.md) |
| HAL | `277de3fd4b0e640654ee73bb3308be2ef01e3aad` | 固定参考仓，未修改 |
| 当前派生 echo RTOS | `1d0de06c394f89be35a4b6966e766b56f035c19d`，基于固定 `7c397f4` | **platform/rtos** 工作区；不同于较早 preboard 配置 |
| Linux SDK | `521833e2d28decbd6473d5717f1f96cc4108e208` | 固定文件/hash；不是运行 Image 精确 build provenance 的证明 |
| 已复制实际 BL31 payload | SHA256 `1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07` | 与 SDK payload 匹配；本轮静态解析，不是寄存器运行值 |
| 已有 echo ELF | SHA256 `a0942d6635ac30bc0d86953a00114b657d2e4c156ff7fa10c2bed204798933cd` | 本轮仅反汇编，不重新生成固件 |

完整源文件 SHA256 / 固定 URL / 自动检查结果见 [SHARED_MEMORY_CACHE_RESULT.json](SHARED_MEMORY_CACHE_RESULT.json)。源码级结论与当前板运行状态分开。

## 1. M0 cache 在实际候选 ELF 中已启用

**`SOURCE_VERIFIED` + `HOST_TESTED`：** 当前 `bsp/rockchip/rk3576-mcu/rtconfig.h` 定义 `RT_USING_CACHE`，`hal_conf.h` 启用 I/D cache 模块。固定 HAL 的 `lib/CMSIS/Device/RK3576/Source/Templates/system_rk3576_mcu.c::SystemInit()` 设置 `CACHE_EN`、等待 `CACHE_INIT_FINISH`，再清 `CACHE_BYPASS`。

实际 ELF 的 `Reset_Handler` 在 RT-Thread `entry` 前调用 `SystemInit`；后者操作 cache controller `0x43810000` 并清 bypass bit `0x40`。`common/drivers/drv_cache.c::rt_hw_cpu_cache_init()` 只启用 cache 中断，不能由这个后续函数没有 enable 调用而推断 cache 未启用。`rt_hw_cpu_dcache_status()` 的固定返回值也不是硬件状态读取。

TRM p750/765 给 BUS MCU 16 KiB unified I/D cache 与 reset bypass。**reset bypass 不代表这份固件启动后仍 bypass。** `rpmsg_platform.c::platform_cache_all_flush_invalidate()` 和 `platform_cache_disable()` 中 HAL 调用被注释；调用空 hook 不构成 clean/invalidate 或 bypass 证据。

## 2. CON14/15：不能只按 M0 本地地址判断覆盖范围

**`SOURCE_VERIFIED`：** TRM p766 §8.6.8/Table 8-14 列 BUS MCU non-cacheable space begin/end 为 CON14/15；缺少比较器地址视图、endpoint inclusivity 和完整 bitfield 说明。当前实际值仍未知。

对已匹配 BL31 的 CODE selector 分支，原始指令核验如下（VMA 基址 `0x40040000`）：

| 指令 VMA | 指令字 | 静态作用 |
| --- | --- | --- |
| `0x4005ecdc` / `0x4005ece4` | `d2880400` / `f2a4c000` | 形成 `x0 = 0x26004020` |
| `0x4005ecec` | `52a40001` | `w1 = 0x20000000` |
| `0x4005ecf0` | `b9003801` | CON14 `0x26004058 = 0x20000000` |
| `0x4005ecf4` | `b9003c02` | CON15 `0x2600405c = caller CODE value` |
| `0x4005ecf8` | `b9004002` | CON16 `0x26004060 = caller CODE value` |

因此，即使 SPL 源码曾写 CON15 `0x48200000`，后续执行这个 CODE 分支会用传入值覆盖它。不能继续引用 SPL 的较大范围作为最终范围。上述为静态代码行为，**没有执行这个 SMC，也不知道当前寄存器值**。

固定 HAL `lib/hal/src/hal_cache.c` 解释 cache maintenance 地址与 MCU 本地地址不同；`HAL_CpuAddrToCacheAddr()` 对 RAM window 使用 `cpuAddr - 0x20000000 + sram_addr_base`。这是维护接口的 decoded address 证据。**推断非缓存比较器也使用 decoded/system PA 属于 `SOURCE_INFERRED`；不能用此函数代替缺失的硬件比较器定义。**

此外，当前 `hal_conf.h` 设置 `HAL_CACHE_DECODED_ADDR_BASE=0x47800000`，会选择 `DECODED_ADDR(x)=x+base` 的单偏移维护路径；它不是 shared RAM 的通用 CON17 转换。不能直接取消 RPMsg hook 注释并声称所有 shared buffer 都已正确维护。

## 3. payload 转换隐含 B17 = 0x40000000

**`SOURCE_VERIFIED`：** 当前 RK3576 `rpmsg_platform.c:569::platform_patova()` 在 `HAL_MCU_CORE` 下固定执行 `addr -= 0x20000000`。实际 remote RX/TX 调用链：

```text
rpmsg_lite.c::vq_rx_remote / vq_tx_alloc_remote
  → virtqueue_get_available_buffer()
  → env_map_patova(vq_ring.desc[...].addr)
  → platform_patova(Linux descriptor address)
```

Linux 固定 transport 不提供 `VIRTIO_F_ACCESS_PLATFORM`，常规 virtqueue 路径使用 `sg_phys`。以下推导以 descriptor 地址确实为 Linux PA 为前提；最终仍需排除不同 DMA/IOMMU/Xen 路径。

TRM 逆变换为 `M0 = PA - B17 + 0x20000000`，port 为 `M0 = PA - 0x20000000`；两者相等要求 **B17=0x40000000**（`SOURCE_INFERRED`，经脚本与实际转换函数 Host mock 验证）。这证明 port 的地址假设，**不是 CON17 实读，不是最终部署参数**。

| 仅用于推导的例子 | M0 地址 | 假设 B17=0x40000000 时的 PA |
| --- | --- | --- |
| vring0 | `0x27d00000` | `0x47d00000` |
| vring1 | `0x27d08000` | `0x47d08000` |

若 CODE 参数仍为 `0x47800000`，M0 本地 `0x27d00000` 在 `0x20000000..0x47800000` 内，而对应 PA `0x47d00000` 在范围外。**如果比较器使用 decoded PA，这个范围不足以覆盖 vring。** 这只是条件反例；比较器视图、实际 CON17、pool 区间未闭合，不能据此设计新的地址或宣布实际冲突解除。

## 4. Linux ring / payload mapping 源码路径

| 对象 | 固定源码链 | 属性和必要条件 |
| --- | --- | --- |
| vring descriptor / avail / used | [`rockchip_rpmsg_mbox.c::rk_rpmsg_find_vq`](https://github.com/LubanCat/kernel/blob/521833e2d28decbd6473d5717f1f96cc4108e208/drivers/rpmsg/rockchip_rpmsg_mbox.c#L186) → `ioremap` → ARM64 `_PAGE_IOREMAP` | `Device nGnRE`；reservation 要 `no-map`，避免线性 RAM 映射冲突/缓存别名 |
| payload | `virtio_rpmsg_bus.c::rpmsg_probe` → `dma_alloc_coherent(vdev->dev.parent)` → `dma_alloc_from_dev_coherent` → `dma_init_coherent_memory` | 非 reusable `shared-dma-pool` 挂到 transport device 时，`memremap(...MEMREMAP_WC)` → `ioremap_wc` → `PROT_NORMAL_NC` |

上述 **`SOURCE_VERIFIED`，未证明当前板已使用这些 mapping**。`kernel/dma/coherent.c` 在 ARM64 要求 pool `no-map` 并拒绝 `reusable`；CMA/reusable 路径不能冒充相同路径。`kernel/iomem.c` / ARM64 `ioremap.c` 也不允许拿普通线性 System RAM 随意建立 WC/Device 别名。

**另一个 gate：** transport 的 `of_reserved_mem_device_init()` 失败只打印 `No shared DMA pool.` 并继续。没有成功附着专用 pool，后续可能 fallback 到普通 coherent allocator；分配属性和 PA 范围不能由预期 DTS 的 pool 地址保证。最终必须保证 attachment、pool 容量/可达范围、无 fallback 和 DMA identity，不能仅看 vring 地址一致。

## 5. Linux barrier 草案

M0 `env_mb/rmb/wmb` → `MEM_BARRIER` → GCC `dsb` + compiler memory clobber（`SOURCE_VERIFIED`）；它提供顺序，不负责清空 write-back cache。

固定 Linux transport 创建 virtqueue 使用 `weak_barriers=true`；[`include/linux/virtio_ring.h`](https://github.com/LubanCat/kernel/blob/521833e2d28decbd6473d5717f1f96cc4108e208/include/linux/virtio_ring.h) 对 heterogeneous CPU 通信明确要求 real barriers。ARM64 weak 路径用 `dmb ish*`；false 路径 `dma_rmb/wmb` 用 `dmb oshld/oshst`，full `mb` 用 `dsb sy`。transport 只宣告 NS，没有 `ORDER_PLATFORM` 将 weak 参数自动强制为 false。

没有源码证据证明 BUS M0 属于 A 核 inner-shareable 一致性域。准备的最小草案仅把该参数改为 false，遵循 Linux 的设备屏障路径；**不声称原内核已发生丢消息，也不声称 barrier 解决了 cache maintenance**。

- Patch：`patches/rk3576-amp-platform/0002-linux-rpmsg-use-device-barriers-draft.patch`。
- SHA256：`654fd053132cca6f54b69f989765266706af2cadc6f33eb45d9d6dc7bc3b7dc5`。
- `HOST_TESTED`：新输出目录下，使用已有板端 `6.1.99-rk3576` headers 副本，编译 AArch64 REL 对象 exit 0。
- 唯一 warning：Host GCC 11.4.0 与 board kernel GCC 10.3.1 差异；没有本次 C 代码 warning。归类 **环境差异 / exact kernel source match blocker**。
- 未生成 ko 或完整 kernel，不是 `BOARD_MODULE_READY`。对象 hash/命令/log hash 见 [LINUX_RPMSG_BARRIER_DRAFT_RESULT.json](LINUX_RPMSG_BARRIER_DRAFT_RESULT.json)。

## 验证与复现

`scripts/amp/check_shared_memory_cache_evidence.py` 核固定仓 SHA/dirty 状态、15 个下载文件 hash、BL31 指令/整体 hash、实际 echo ELF 的 cache 初始化、mapping/barrier 源码链及单参数 patch。抽取实际 `platform_patova` 用标准 Host GCC `-Wall -Wextra -Werror` 编译运行；不同 synthetic B17 会产生不一致。两项负例分别替换 BL31 字节和增加非最小 patch 修改，均被拒绝。**它是 Host 本地证据检查，不依赖忽略输入的通用 CI 测试；PASS 不等于实板 coherency PASS。**

运行参数（从本 project 根目录）：

```sh
python3 scripts/amp/check_shared_memory_cache_evidence.py \
  --hal /home/ywx/rk3576-work/reference/rk3576-amp/rk3576-hal \
  --rtos /home/ywx/rk3576-work/worktrees/rk3576-amp-platform/rtos \
  --kernel artifacts/local/p022-kernel-cache-sources \
  --transport artifacts/local/amp-platform-sources/LubanCat/kernel/drivers/rpmsg/rockchip_rpmsg_mbox.c \
  --bl31 artifacts/local/amp-uboot-dump-20261002/image-1.bin \
  --elf artifacts/local/amp-platform-candidate-build/rtthread.elf \
  --objdump /home/ywx/.local/toolchains/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi/bin/arm-none-eabi-objdump \
  --draft artifacts/local/p022-linux-transport-final/rockchip_rpmsg_mbox.c \
  --negative-checks
```

## 裁决与下一阶段

- **条件方案方向：A `UNCACHED_SHARED_MEMORY`**。Linux 有源码支持的 Device vring + NC payload 路径；M0 必须证明整个 ring/pool 范围不进入 cache，或从冷启动保持全局 cache bypass。
- **最终 scheme 仍未选择，coherency 仍 `UNVERIFIED`。** 不选 B：TRM 没有 BUS MCU 与 A 核硬件一致性保证；不宣称 C 已实现：hook 为空且维护地址转换不通用。
- 可以在下一 Host 阶段做“冷启动不启用 cache”的派生固件证明，避免依赖未明确的 CON14/15 comparator；不能对已有脏数据直接切 bypass 当成合法热切换。此轮没有修改或生成该固件。
- 进入 D 还需确定设置/soft reset 后有效 B17、修正或明确 payload 转换约束、完整 ring/pool 物理范围与无缓存别名、专用 pool 成功附着和 fail-closed 行为、对应运行内核/DT 的准确构建、启动与恢复方案。**不部署本 patch，不启动 M0。**
