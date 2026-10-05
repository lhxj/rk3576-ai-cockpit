# 接线前资源方案及验证（2026-10-05）

最新状态：**WIRING_READY_FOR_USER**。下文Host阶段及权限缺口为历史；attempt4已经按用户最新授权取得M0十地址白名单只读证据并完成25文件默认冷恢复。见末尾最新里程碑。

已完成可在 Host 审核的 ownership 配置、BSP/IRQ gate/reset 适配、构建与失败回归。
等级 `PREWIRE_SOURCE_CONFIG_HOST_VERIFIED`；不是当前硬件独占/访问权限已通过。
当前保留 BL31 的 I2C9 权限缺口已定位，有两个明确的闭合途径；不重复测量或盲猜安全寄存器。
`WIRING_READY_FOR_USER` 尚未发出，模块 SDA/SCL 仍保持未接。

## 资源所有权实现

`patches/mpu6050/i2c9-linux-owner.dtso` 只在冻结 C DT 上增加 I2C9 两 clock 与 pinctrl：
Linux mcu-amp 持 PCLK_I2C9(118)/CLK_I2C9(130)，CLK_I2C9 明确固定 xin24m 24MHz；
原 BUS MCU/UART5 五 clock、UART5 pinctrl 保留。Linux I2C9 保持 disabled/无从设备。
当前 driver 的 bulk_prepare_enable 引用在绑定期间保持，Linux 不取得 I2C9 adapter、不发事务；
M0 的 I2C9 模式不再直接切换这些 gate，不改 parent，不在无人消费的 DT 上挂 reset。
绑定窗口禁止 unbind/卸载 owner、改 clock/pinctrl、系统 suspend；冷恢复时统一退出。

DT 源经过 dtc 编译、fdtoverlay 应用到精确冻结 SHA dd68818…；validator 逐节点逐属性比较，
只有 /mcu-amp 五项资源属性有差异，全部其余属性保持。CAM0/codec/RTC/PMIC、原 RPMsg
reserved-memory/mailbox/link-id/transport 完全保持。独立检查所有 enabled pinctrl states、
GPIO consumers 与 GPIO1 hog，GPIO1_B4/B5 无活跃 alternate owner。
7 个 fixture 回归拒绝 CAM0/transport/memory 篡改、I2C9 enabled/children、错 parent、
SPI1/SDMMC1 占用、GPIO hog/GPIO consumer；fixture 不算硬件证据。

## Reset 与 INTMUX 已补到派生驱动

补丁顺序 0001 -> 0002 -> 0003，原冻 SDK/HAL 不动。
延迟 probe 先检查 clock 24MHz，再仅一次 enable PCLK_INTMUX2BUS_GATE(0xbc)，
然后安装/mask IRQ、仅 deassert I2C9 APB(0xc8) 与 function(0xd4) 两 reset，再 Init/register。
门前不会访问这些资源；错误锁存，重复 READY 不再 gate/reset/probe。
不 assert reset，不 reset INTMUX，不 disable INTMUX gate，不改 NVIC/INTMUX 既有初始化。

HAL reset/gate API 使用 CRU_WRITE 的 upper-half write mask，单 bit 写，不覆盖同寄存器
其他设备；0xc8=SOFTRST_CON12 bit8，0xd4=CON13 bit4。
INTMUX gate 0xbc=CLKGATE_CON11 bit12，位掩码 enable 写值 0x10000000，
reset deassert 分别 0x01000000/0x00100000。地址均由固定 HAL bank metadata 计算，
本轮没有执行这些 MMIO。HAL API 实现正常路径直接返回 HAL_OK，无硬件 readback，
所以 Fake 失败覆盖只能证明调用者错误传播，不能把 HAL_OK 当 firewall/reset 实测。

固定 Linux clk-rk3576.c 没有注册 INTMUX2BUS gate，rk3576 DT binding 无此 clock ID；
RK3576 CCF 没有 CLK GATE CON11 bit12 的管理项。clk-gate.c 的其它 gate 均窄位 hiword-mask，
clk_disable_unused 只迭代已注册 clk_core，因此不会由该 CCF 分支关闭本 bit。
检查固定 clk/soc/pinctrl/irqchip 源未找到该 RK3576 gate 的其它写者；不外推至闭源 BL31
睡眠路径。INTMUX2BUS 是 BUS MCU 的私有路由 controller，本门以后 M0 只持该 gate 到冷恢复；
PCLK_BUS_ROOT 的祖先引用同时由 Linux 持有的 I2C9 PCLK 保持。仍需获批窗口验证实际 gate/reset。

I2C9 Linux GIC_SPI97 -> system INTID129；HAL vector129+32=161。
TRM Part1 p764/765 公式得到 INTMUX group2 bit1 -> BUS NVIC18。
INTMUX Linux PA 0x2aec0000，M0设备地址0x4aec0000。
HAL_INTMUX_Init 设置 NVIC_OUT2，rt_hw_interrupt_* 转发 vector161 到 INTMUX；
运行期只操作 group2 bit1，不把 Linux SPI97 直接当 HAL vector。
原 mailbox dedicated IRQ9 的 PASS 不能作为这条路由的运行证据。

## 当前环境与访问依据

本轮普通用户持 board_lock 有界只读 SSH 已复核默认 6.1.99-rk3576，root p3/boot p2，
BL31v1.14/U-Boot149b1c5；I2C9 disabled/无 driver、Linux adapter 仅1/2/3及HDMI/DP，
无 mcu-amp 绑定、RPMsg device 空、DT gpio-hog 空。软件快照与 JSON 见配套结果。
首次脚本遇 debugfs PermissionError，第二次把权限不可用显式记录后 exit0；未 sudo/绕过权限，
未读取 debugfs 引脚寄存器，也没有读取 MMIO、MPU 或进行 I2C 事务。

TRM Part1 p618：I2C1~9/BUS_MCU/CRU/INTMUX2BUS 在 PD_BUS (ALIVE)；
p762/763：M0 0x40000000..0x5fffffff 的设备地址=CPU PA+0x20000000。
所以 I2C9 M0 地址0x4ae80000不需改 CON16/17。文档没有提供当前 board firewall policy。

在线主来源也已核查：
[TF-A 固定源码](https://github.com/ARM-software/arm-trusted-firmware/blob/3d425f4f459820a44d24b23464dbc09d6baab7bb/plat/rockchip/rk3576/drivers/secure/firewall.c)
把 BUS_MCU 列为 SYS master36/domain1，常规 BUS slave 默认 group0 允许 domain1；
这是可支持访问的公开实现模型，尚未证明当前保留固件使用同样 policy。
[Rockchip rkbin 固定 release notes](https://github.com/rockchip-linux/rkbin/blob/3e288fe814e059dd06833495f845cab04ac20a5c/doc/release/RK3576_EN.md)
对应旧 v1.14 构建 c481e5368（2024-11-08），与本地 atf-1 embedded build/当前 fwver 相符；
v1.07 开始支持 NS 配置 BUS MCU。新版 primary/secondary v1.14 是另一个版本，不能混用。
这些 release notes 不公开旧固件的 I2C9 group/lookup 配置，不能据版本号断言权限已证明。
旧 U-Boot 两个 MCU SMC 只配 CODE/SRAM remap，没有新增 I2C9 firewall grant。
没有替换 BL31/TEE，也不从 mailbox/shared-memory 成功推导所有 peripherals 均可访问。

## 尚待闭合的具体事项与两条路径

1. 厂商确认：对 BL31v1.14 c481e5368（atf-1 SHA1c50b2…）、保留 AMP 启动链，
   BUS_MCU 的 master domain/security 属性是什么；I2C9、CRU 的目标 reset/gate 与
   INTMUX2BUS 是否为可访问 slave/group、lookup 是否允许；是否在两个 MCU config SMC
   中或 Linux 启动后改变。需要相应源码/寄存器定义/只读证据，不询问泛泛“支持I2C吗”。
2. 若取不到逐字来源，准备独立、无传感器信号接线的最小诊断审批包：
   从原已验证链派生、保留 health，M0 不调用 I2C Transfer/Setup、不配置 START、
   无地址探测；Linux 成功持 clocks/pins 后仅定向读取经语义审查的 I2C9 CON/CLKDIV、
   指定 clock/reset/INTMUX 状态各一次，记录 begin/end 与值，证明是 M0 的访问。
   Linux/CPU 读取不能替代 M0 访问证据。只读 MMIO 本身也必须另批。
   此诊断代码/FIT 尚未构建，必须先审查每个寄存器读语义、ELF/map/hash、完整部署/撤回
   命令，输出 APPROVAL_REQUIRED_BOARD_CHANGE 后得到明确批准；本文件不是授权或可执行包。

诊断建议单次冷启动≤120秒，UART每路≤256KiB；异常/HardFault/health失联或读未返回立即
停止，不重试/扩探/盲改 SGRF。M0 单个 bus fault/被拒读可能停止核心，不能保证它自行恢复；
预先把单次获批冷恢复包含进窗口，默认 Debian/冻结 hash 核验必需。不做固件热重启。
若 reset assert/gate/security 状态不符，先保存只读证据，再提出窄位方案，不现场试写。
该方案无 MPU 访问，不冒充 RTOS_SENSOR_PASS。

## 接线门与运行门分别判断

物理 PCB/40Pin方向、Pin19/23空闲、模块5V输入与3.3V上拉、AD0低已确认；
风扇4/6、串口地20保持，模块电源2/14保持。不要求再次测微小 LDO或用电流孔跨电源。
供电预算未知记录到以后负载验收，不把风扇振动当传感器故障。
物理电压/针脚资料已满足候选接线审查，不能用缺少 debugfs 泛化成电气证据全无。
然而用户明确要求 I2C ownership 先闭合；当前实际 M0 权限尚缺最后一项依据，因此
不擅自发 WIRING_READY_FOR_USER。此门与“已有真实样本/实际 IRQ PASS”不同：
上述权限可以通过厂商定论或独立获批无信号诊断解决，无需先访问 MPU。
部署审批、断电信号接线确认及未来 T1/T2 必须各自满足，不能被 Host 配置通过代替。

## Host 结果与产物

实际 driver ASan/UBSan 100/1000Hz、board iomux、reset 两条错误、INTMUX gate 错误锁存、
提前0调用、仅指定 gate/two reset、重复幂等均通过；native SCons useconfig+build通过。
Host CI31/31 CTest、59/59 Python、5/5撤回通过。DT/原生 artifact exact hashes
见 [PREWIRE_RESOURCE_RESULT.json](PREWIRE_RESOURCE_RESULT.json)。ELF text136100/data2648，
heap370980与stack1024/原共享区未变；运行期高水位未测。
全量 Kconfiglib 的原 LED自依赖限制保留；适配诊断不是 sensor业务/FIT组合。
没有部署、KO、boot写入、MMIO、I2C或重启。以上配置可交主控审核，不能升最终 PASS。

## 主控独立复核

主控从冻结 stage-C 实际重跑 DT 编译/对比，三补丁实际驱动 ASan/UBSan100/1000Hz、7项 DT 失败回归、source预检与 native/DT实际hash复核均通过；再次 host_ci31/31 CTest、59/59 Python、5/5撤回。日志仅在 artifacts/local/hardware-review/prewire-reviewer-host-ci.log，独立 DT 输出在 artifacts/local/mpu-i2c9-owner-dt-main-review。

本里程碑为 Host 接线前配置完成；实际冻结 BL31 权限仍 `BLOCKED`。无传感器权限诊断包尚未构建，缺少确切 FIT/启动/安装 hash，不能据本结果批准部署。NVIC18/group2公式已核对实际 HAL_BUS_MCU_CORE RK3576 后半 INTMUX 实现；不是前半 INTMUX_IRQ_INTEN_L_OFFSET 布局。

## 2026-10-05 attempt4接线前里程碑（最新）

**WIRING_READY_FOR_USER / USER_POWER_OFF_WIRING_CONFIRMATION_PENDING**。主控在共享锁内完成真实冷进入、一次独立I2C_RESOURCE_PROBE_V1诊断、正常退出及默认冷恢复；不是传感器业务验收。实测M0按白名单顺序读取10个地址一次，BEGIN=1/END=1/status=0，新增诊断寄存器写=0、I2C事务=0、样本=0。Linux预检确认派生owner绑定、I2C9禁用无adapter、固定24MHz及clock引用/保护；实际health握手后仅发一次诊断，HELLO_ACK=1、PING/PONG=3/3、timeout/error=0/0、RTT=2ms、elapsed=2508ms。KO正常卸载并再次核实不存在；双UART结束、正常shutdown后用户冷恢复，默认6.1.99-rk3576/rootp3boot2、RPMsg/项目进程为空、25个默认/冻结/SI文件hash一致。共享锁一直保持至默认确认后释放，所有会话结束。

M0实际白名单只读访问证据闭合，Linux ownership派生配置在本次启动中已实测保持。此证据不证明寄存器写权限、I2C交易、IRQ投递或WHO_AM_I，不推导整个BL31策略或suspend行为。完整HOST_PASS/RTOS_SENSOR_PASS/RPMSG_SENSOR_PASS/UI_SENSOR_PASS及最终集成等级全部NOT_RUN；未进行新的五分钟sensor共存。

见[脱敏实际结果](RESOURCE_PROBE_BOARD_RESULT.json)；原始日志与原review JSON保留忽略目录，原review恢复PENDING为生成时事实，不覆盖。最新Host CI31/31 CTest、72/72 Python、5/5撤回，八模式UART fixture通过；本次仅文档收敛，不改v2生产包、不再诊断或冷重启。下一步止于等待用户断电接线确认。
