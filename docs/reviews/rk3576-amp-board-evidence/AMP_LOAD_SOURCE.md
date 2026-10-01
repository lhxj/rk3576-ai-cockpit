# AMP FIT 来源：公开 loader 的唯一已证入口

公开固定 U-Boot [drivers/cpu/rockchip_amp.c:44,444-523](https://github.com/LubanCat/u-boot/blob/8f53f800da2c25d0c6ba414fb45902a01675703a/drivers/cpu/rockchip_amp.c) 定义 `AMP_PART="amp"`，`amp_cpus_on()` 取当前 boot device，调用 `part_get_info_by_name(dev_desc,"amp",&part)`，失败即返回 `-ENODEV`。找到分区后通过 `blk_dread()` 从分区起始块读取 FIT header/full image，`images.fit_uname_cfg="conf"`、`images.verify=1`，再由 `boot_get_loadable()` 将 loadables 装到各自 FIT `load` 地址并 release。`board_late_init()` 的 `CONFIG_AMP` 条件控制此函数是否调用。

这里的 `amp` 是**GPT partition name**。FIT 内 `conf` 或 standalone 节点、`/boot` 文件、U-Boot env 名字都不能代替该分区。对这条公开 loader 路径，结论为 **AMP_PARTITION_REQUIRED / SOURCE_VERIFIED**。当前板只读 GPT 只有 `uboot`、`boot`、`rootfs`，故若这条路径已启用，会找不到 FIT；没有证据显示它正被启用。当前 `/boot/boot.cmd` 也无另一个 AMP FIT loader。公开 pinned 源中 `amp_cpus_on()` 没有 file/FIT-in-memory fallback；不能据此否定其它未取得的 vendor binary/custom loader，但目前**没有验证可用的不改分区路径**。

候选 `amp.its` 的 `load=0x47800000` 是 loadable 的**实际目标物理地址**，不是读取 GPT FIT 时 `memalign()` 取得的临时 FIT 缓冲区。它与 CPU3 参考 RPMsg vring0 的物理地址相同，原样组合会冲突。当前板未启 AMP DT，尚无正在运行的重叠。由于 CON17、保留内存与 boot chain 未证，本轮没有改 ITS/linker/DTS，也没有创建 `amp` 分区。

要确认实际加载来源，先取得当前 U-Boot 镜像与 `.config`、和供应商 manifest/hash 比对；若 `CONFIG_AMP=y`，应按实际 loader 对应的 GPT/文件结构设计；若没有，AMP 入口本身需独立设计与审查。任何分区变更在本轮均禁止，并将扩大 recovery 要求。
