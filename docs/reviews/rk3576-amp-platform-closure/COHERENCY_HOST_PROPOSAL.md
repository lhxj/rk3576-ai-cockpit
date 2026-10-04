> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 最小 echo 的冷启动 uncached 方案

候选选择 **UNCACHED_SHARED_MEMORY**，证据为 SOURCE_VERIFIED / HOST_TESTED；当前板的运行 cache 状态仍 UNVERIFIED。BUS MCU有真实16 KiB unified I/D cache，不能用 Cortex-M0 架构名称省略维护问题。

1. TRM Part1 §8.6.4：reset后 cache bypass。派生 HAL `system_rk3576_mcu.c:SystemInit()` 显式保留 `CACHE_BYPASS` bit6，执行 DSB/ISB；禁止同时定义 I/D cache模块。实际 ELF反汇编确认写 MCU-view `0x43810000` 控制器及两道屏障；`RT_USING_CACHE`关闭。
2. U-Boot冷启动 patch先打开最小clock、assert BIU/core/JTAG reset，DSB +10µs；CODE/shared SiP均返回0后DSB，再release。失败保持reset。没有做热重启或丢弃运行中dirtycache的承诺。
3. Linux `rockchip_rpmsg_mbox.c:rk_rpmsg_find_vq()` 用 `ioremap()`映射ring（AArch64 Device-nGnRE）；`vring_new_virtqueue(...false...)` 用device/DMA屏障。`virtio_ring.h`的`virtio_wmb/rmb`在false分支到`dma_wmb/dma_rmb`，而不是只做CPU间屏障。
4. payload DT=`shared-dma-pool + no-map + !reusable`；`kernel/dma/coherent.c:dma_init_coherent_memory()`通过`memremap(...MEMREMAP_WC)`走AArch64 Normal-NC mapping；`dma_alloc_coherent()`挂在对应transport device。此处“coherent”是Linux DMA API语义，不证明M0是硬件coherent master。
5. transport新`project,require-shared-dma-pool`检查link4、单vdev、non-reusable/no-map与64KiB几何。pool声明/挂接失败则probe失败并free mailbox，不能fallback到M0窗口外。M0只把该pool的物理描述符地址转换成本地地址，越界/64bit截断返回0。Host C检查所有65,536个字节及边界。
6. M0 environment的`env_wmb/rmb → MEM_BARRIER`是`dsb`+compiler memory clobber；virtqueue在payload/descriptor→avail/used发布前及读取ring后使用它。Linux DMA屏障负责write-combine drain/order。不存在硬件一致性推断；空cache hook仅在本候选**全局bypass且cache不能重新打开**的前提下成立。

链条适用于cold-only proposal，需要连同RTOS/HAL/Linux/U-Boot patch及DT一起审查。现有135k cache-on固件、仅改barrier的旧transport、当前板未配置的映射均不能混用。首次实板仍应记录cache bypass初始化、SiP状态及HELLO/PING收发，禁止直接套用到warm restart。
