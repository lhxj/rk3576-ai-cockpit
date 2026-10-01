# AMP 资源所有权草表

`LINUX OWNER` / `RTOS OWNER` 为当前证据支持的使用权建议或现有配置；`SHARED` 只在有明确协议时成立。此表**不是**资源迁移决定，尤其没有选择 MPU6050 引脚。运行中 DTB、板图、pinmux、clock/IRQ 和占用须在部署前只读核实。证据路径缩写见 `STATIC_AUDIT.md`，Linux 公共源码链接见 `LUBANCAT_DIFF.md`。

| RESOURCE | LINUX OWNER | RTOS OWNER | SHARED | UNRESOLVED | EVIDENCE |
|---|---|---|---|---|---|
| A53 CPU0–7 | 当前 Linux 历史在线 | 无 | 否 | AMP 配置后 CPU 在线变化待证 | `docs/STATUS.md` §2；M0 独立 ISA `rtconfig.py` |
| BUS Cortex-M0 | boot/clock 管理方未定 | 目标固件 | 否 | 启动实体、reset、载入地址、恢复 | `hal_conf.h`、HAL `soc.h`、Linux `rockchip_amp.c` |
| M0 512 KiB code RAM/DDR 别名 | 必须 reserved | ELF 使用 `0..0x80000` | 否 | FIT load `0x47800000` 与参考 DTS RPMsg ring 同址；代码物理映射/预留不一致 | `gcc_link.ld.S`、`Image/amp.its`、AMP DTS |
| RPMsg vring/buffer | virtio master / DMA pool | RPMsg-Lite remote | 协议共享 | M0 `0x27d00000` 的物理映射未证；若为 `0x47d00000`，与参考 `0x47800000` 不同且 4 MiB 区重叠 MCU reserve | `RPMSG_ALIGNMENT.md` 参数表 |
| MBOX0/3/其他、CH0 | Linux mailbox transport 一端 | M0 port 一端 | 控制消息共享 | MCU link 0x04 的 controller/client/IRQ 未闭合 | `rpmsg_platform.c`、`rk3576-amp.dtsi` |
| UART5 / GPIO3_D4,D5 | Linux 当前占用未证 | EVB 候选 console | 否 | m0/m2 物理接线、串口控制台/时钟归属 | `board/evb/iomux.c`、`board/common/iomux_base.c`、AMP DTS |
| I2C1/2/3/4 | I2C1/2 Type-C/RTC 源码；v2 I2C3 RTC/codec；历史相机 I2C3/4 | 不授权 | 否 | 运行 DT 和相机实际复合使用 | LubanCat base/v2 DTS；`docs/STATUS.md` §4 |
| I2C6/7/8 | 当前运行占用未知 | 实际 `rtconfig.h` 仅开 I2C7，EVB 写 I2C7 pinmux；`.config` 另列 I2C6/8 | 否 | 配置文件漂移；先查 DT/pinctrl/板图/PMIC，再决定 MPU6050 总线 | `.config`、`rtconfig.h`、`drv_i2c.c`、`iomux.c` |
| PWM0_CH0 / IR key | **Linux** | 候选 EVB 误用 | 否 | RTOS 后续配置应禁用 | LubanCat base DTS `&pwm0_2ch_0`；`board/evb/iomux.c` |
| GPIO banks / SPI0/4 | Linux 电源、HP、CSI 等使用部分 pin | EVB 默认 pinmux 写入 | 否 | 逐 pin ownership，模拟 LED/按键/蜂鸣器不需独占 | LubanCat base DTS；`board/evb/iomux.c` |
| SysTick / TIMER11 | Linux 不应直接占 M0 SysTick | RTOS tick + HAL SYS_TIMER | 时钟源可能共管 | timer clock/reset 是否已启用、频率 | `board/evb/board.c`、`hal_conf.h`、AMP DTS |
| CRU / power domain / reset | Linux/firmware 共同平台管理 | M0 仅被明确授权的外设 clock | 需受控协调 | 当前 `clk_inits` 空、BSP_Init 空 | `board.c`、`hal_bsp.c`、Linux AMP DTS |
| Camera/ISP、ALSA codec、HDMI/VOP、Wi-Fi/PCIe | **Linux** | 无 | 只通过 Linux 应用状态消息 | AMP 后回归需证明未回退 | LubanCat base/CSI DTS、历史 `docs/STATUS.md` |
| MPU6050 | 暂无板端归属 | 目标为 RTOS 采样 | 消息通过 RPMsg | 控制器、pinmux、接线、IRQ、地址均未选择 | 项目目标及 `LUBANCAT_DIFF.md` |

接下来先将候选 EVB 默认 I2C/PWM/SPI/GPIO 的实际写入列表与运行 DTB 逐 pin 对照，再形成最小资源转移提案；没有所有权证据的项保持 `UNRESOLVED`。
