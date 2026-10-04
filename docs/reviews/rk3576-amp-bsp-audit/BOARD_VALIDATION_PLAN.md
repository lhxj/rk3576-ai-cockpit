> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# 受控实板验证前的文件级准备

**当前不能申请启动 M0。** Host 构建通过，但内存/link-id/mailbox/缓存、LubanCat pinmux 和恢复路径未闭合。本文件给下一步的**离板修改清单与验证门**，不是本轮部署指令；以下文件均尚未修改。建议在新的派生 worktree/patch stack 中工作，保留两个固定候选源码原样。

## 先做的离板文件变更

| 文件或来源 | 需要形成的可审查改动 / 证据 |
|---|---|
| 派生 SDK 目录/构建配置 | 将 `R/bsp/rockchip/common/hal -> ../../../../hal` 所需的 `<SDK>/hal` 明确映射到固定 `rk3576-hal`；构建 manifest 锁 RTOS/HAL/toolchain。不要复制候选进主项目。 |
| 派生 `rk3576-mcu/gcc_link.ld.S`、`Image/amp.its` | 先取得 M0 DDR 地址到 SoC 物理地址的映射证据，再统一 code load、共享 RPMsg/ATAGS 长度和边界；ELF/map/FIT 三方核对。必须消除 FIT `0x47800000` 与参考 RPMsg ring 同址；若 `0x27d00000` 映成 `0x47d00000`，还须消除 `0x47d00000..0x480fffff` 与参考 MCU reserve 的重叠。 |
| 派生 LubanCat-3 v2 AMP DTS/overlay，基于运行中 DTB 对应**准确**内核树 | 从运行 DTB/板图确认 reserved-memory、`rockchip,rpmsg` `reg`/DMA pool、`rockchip,link-id`、MBOX phandle/channel、`rockchip-amp` clocks/IRQ/boot 属性与不碰 Linux camera/audio/显示资源；不能直接 include EVB AMP DTS。`rk3576-lubancat-3-v2.dts` 和必要 `.dtsi` 为未来修改点，当前文件未动。 |
| 派生 `common/drivers/rpmsg-lite/lib/include/platform/RK3576/{rpmsg_config.h,rpmsg_platform.h}` 与 `porting/platform/RK3576/rpmsg_platform.c` | 为选定 M0 link-id（参考 DTS 注释为 `0x04`，最终依真实驱动/MCU 路由定）补正确 MBOX client/IRQ/controller；移除依赖首包顺序的 queue 判断；确定 cache 属性或实现相应 clean/invalidate 与超时；避免空 client 表项。 |
| 派生 `common/tests/rpmsg_test.c` 或新独立最小 echo 应用 | 修正 NS callback/`INIT_APP_EXPORT` 签名；用有界 payload 长度、有限收包/超时、端点销毁、NS unbind、重启 epoch 与重建策略。先只做 echo，不并入产品五个 task。 |
| 派生 `rk3576-mcu/.config`/`rtconfig.h`、`board/evb/{board.c,iomux.c}` 或新 `board/lubancat-3-v2/` | 先消除 `.config` 将 I2C3/6/7/8 标为启用、`rtconfig.h` 却只启 I2C7 的配置漂移；明确只开有所有权的 UART5/MBOX/外设，禁用 PWM0_CH0、SPI0/4、I2C7 和 EVB 默认 GPIO pinmux，除非逐项证明可用；核对 UART5 m0 接线/clock/reset。MPU6050 总线和引脚以后再定。 |
| Linux `drivers/rpmsg/rockchip_rpmsg_mbox.c`/test driver 与内核配置（仅若运行内核差异要求） | 对照当前板运行 kernel 的 exact source/config，确认 transport 和 `rockchip_rpmsg_test` 是否可用；若无 test driver，规划可审查的最小 Linux 测试入口。此轮不得修改 kernel。 |
| U-Boot/BL31/FIT 配置与启动介质清单 | 锁版本、key、FIT loadable 格式、实际启动命令/分区、校验和以及可恢复路径；`mkimage.sh` 所需工具来源/哈希单独审查。当前禁止运行随仓二进制或更改 boot。 |

## 门槛与回退准备

1. **只读运行基线**：在另行授权的板端只读窗口核对运行 DTB hash/解包内容、内核 config/build ID、`/boot` 启动链、eMMC 分区、U-Boot/BL31/FIT、CPU/IRQ/mailbox/rpmsg、CMA/DDR 与 UART5/I2C/PWM pinmux 实际占用。历史配置不能替代当前值；不执行 I2C 硬件探测。
2. **离板一致性门**：派生 firmware `file/readelf/nm/size/objdump`、map、FIT/DT 地址布局无重叠；Linux/RTOS link-id、vring 0/1、buffer pool、64×512、0x1000 align、mailbox CMD/magic、IRQ 和 cache 属性逐项相等；明确 Linux test driver 的服务匹配。用静态脚本核查区间交叉。
3. **资源门**：UART5、MBOX、timer、clock/reset、power、GPIO、I2C ownership 签字表；相机 I2C、I2C3 codec/RTC、PWM0 IR、Wi-Fi/音频/HDMI 维持 Linux；MPU6050 引脚以后另定。
4. **恢复门**：保存当前 boot/DTB/kernel/U-Boot/FIT 的准确 hash 和可用备份，确认 USB-TTL 输出、恢复介质和回退步骤；先在断电/启动失败情况下有人工恢复方案，不靠 Linux 在线才能回退。任何 boot/DTB/partition 操作都需逐项 L3 授权。
5. **最小实测方案（后续单独授权）**：限定时长和日志上限，只加载固定 hash 的最小 M0 echo；串口确认 vector/heap/tick，Linux RPMsg channel 与 NS 服务出现、有限双向包及序号/RTT；随后分别验证 Linux 重启、M0 重启恢复，再回归 CAM0、音频、触控、Wi-Fi。未满足前一门不得进入下一门。

本次结论保持 **C. HOST_BUILD_PASS**；完成上述离板参数、板级差异与可恢复启动方案后再评估 D。没有实板启动证据时不写 AMP VERIFIED。
