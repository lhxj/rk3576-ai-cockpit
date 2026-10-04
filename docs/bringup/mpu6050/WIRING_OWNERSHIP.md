# 接线与 I2C ownership 审查

2026-10-05。**WIRING_NOT_READY / OWNERSHIP_NOT_CLOSED**。
候选为SoC I2C9_M1；本表不能作为已经批准的接线指令。

## 版本门与来源

用户最终更正确认实物PCB：**EBF410513V2R0 20260521**；软件实时model为
**EmbedFire LubanCat-3-v2**，与取得的原理图 **EBF410513V2R0_SCH_20260521.pdf**一致。
用户最初提供V0R1 20240808后明确更正，曾据此暂缓接线；当前以更正为准。
不能从DT名称反推物理板版本。依据[板厂硬件资料](https://doc.embedfire.com/lubancat-rk/hardware/ebf_lubancat_rk_hardware/zh/latest/board/ebf_lubancat_rk.html)
中的对应版本图纸核对引出与供电。PCB版本门已解除，模块测量/Pin1人工核对仍待完成。

来源与hash（原始PDF/图片/归档只留忽略目录）：

| 资料 | 身份/用途 |
|---|---|
| 用户三张模块截图 | 用户提供设计/商家资料；不是实物测量。图1 AD0 R6=4.7kΩ，图2文字10kΩ，存在差异；图3底部还有串联电阻版本。不能据此确认实物R6 |
| [板厂40Pin图](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/40pin/40pin.html) | lbc3-40Pin.jpg SHA256 `ac980260d0e0b9ffdbfdefbe076e551a4319da96cb418354940b14acb369e74d`；未标明V0R1兼容范围 |
| [V2R0资料包](https://mall-ebf.oss-cn-shenzhen.aliyuncs.com/product/sync/DEMO_BOARD/LubanCat3/Datasheet/LubanCat3_20260521.zip) | SHA256 `66dccd86321f29e04861a6fab8a594398ab041747e31073f5c5926b9bda6571c`；视觉核对sheet3/15/24与PCB尺寸图，仅设计依据 |
| 板厂V2R0变更通知 | 明确RTC从I2C2迁到I2C3；提醒软件/物理版本匹配，不能用当前DTS证明旧PCB走线 |
| Linux固定源码 | kernel `521833e2d28decbd6473d5717f1f96cc4108e208`；rk3576.dtsi、pinctrl、lubancat-3/-v2、clk-rk3576.c、rockchip_amp.c |
| RTOS/HAL固定源码 | RTOS import8541f7a→3a39b0f+v5补丁；HAL277de3f→bc99978；SDK保持外部 |

## 候选接线表（V2R0设计/实物版本已核，电压/Pin1待确认）

| MPU模块pin | 板物理Pin | SoC pad/复用 | 控制器 | 电压域/约束 |
|---|---:|---|---|---|
| VCC | 2（候选） | J12 DCIN_5V0，无SoC pad | — | 模块LDO输入5V方案；先测LDO/上拉，再决定；不能接芯片VDD |
| GND | 20（候选） | J12 GND，无SoC pad | — | 共地，避免占用风扇Pin6 |
| SCL | 19（候选） | GPIO1_B5，I2C9_SCL_M1，function10 | SoC I2C9 | **VCCIO3**，V2R0 sheet15接VCC_3V3_S0；不是VCCIO1，不由GPIO bank号推电压 |
| SDA | 23（候选） | GPIO1_B4，I2C9_SDA_M1，function10 | 同一SoC I2C9 | 同上；模块4.7kΩ上拉设计到LDO 3.3V，实测未完成 |
| AD0 | 不接（有条件） | — | 预期7-bit地址0x68 | 必须确认实物下拉；WHO_AM_I仍应0x68，与地址0x69选择不同 |
| XDA/XCL/INT | 不接 | — | — | 第一版不启用辅助总线/中断 |

**40Pin方向依据**：V2R0原理图sheet24的J12与该包PCB尺寸图一起核。
按该尺寸图的元件面观察，J12在板边；靠Type-C/Maskrom/On-Off一端为1/2，
另一端靠USB-A/RJ45为39/40。靠板内为奇数排，靠板边为偶数排。
Pin1同排后续为3/5；Pin2同排后续为4/6。不可仅凭“左上角”或树莓派照片识别。
此方向仍须由用户与实物“1”丝印/焊盘标志核对后才可用于信号接线。

用户确认风扇VCC=物理Pin4、GND=Pin6，当前正常转动，电源5V/4A。
保持原供电，不增加PWM/GPIO。风扇标注电流未知，4A额定值不是系统剩余预算；
需记录实际风扇电流或可靠规格、现有负载与5V压降，测电流不得把表跨接5V/GND。
模块尚未供电，没有LDO/SDA/SCL/AD0电压证据。风扇振动与安装方式进入采样记录。

## 资源核查结论

[实时脱敏DT索引](resource-evidence.json)，原始普通用户SSH证据留
`artifacts/local/hardware-review/live-ownership.txt`与
`artifacts/local/p025-post-recovery-20261004T155744Z-241045/`。
持共用board_lock，无全总线扫描/强制访问/MMIO/GPIO改写。

| 项目 | 已核依据 | 结论/后续门 |
|---|---|---|
| 控制器/MMIO | SoC I2C9=/i2c@2ae80000，Linux PA0x2ae80000，span0x1000；HAL_MCU_CORE加0x20000000 | M0 HAL指针0x4ae80000为源码地址；没有实板访问证明，不套共享RAM CON17换算 |
| 引脚 | Linux pinctrl i2c9m1-xfer=<1,13,10>,<1,12,10>；设计J12 pin19/23 | V2R0设计引出已核，实物电压/Pin1待确认 |
| 运行DT/绑定 | i2c9 disabled、无子节点；无Linux i2c9 adapter、无platform绑定 | 当前没有Linux控制器owner；不是未来clock/access结论 |
| 全部Linux从设备 | 实际I2C1 rk806@23；I2C2 fusb302@22；I2C3 dw9714@0c/es8323@11/ov8858@36/hym8563@51；HDMI/DP adapter另列 | I2C1/2/3保持Linux；尤其不能停I2C3 |
| 其他候选 | I2C4含CAM1设计子节点；I2C7/8含CAM3/4节点且对应3/5、27/28；I2C0 pin13/15 | 选择无子节点的I2C9，避免摄像头共享；不把换mux当新控制器 |
| pad消费者 | DT中SPI1@2ad00000与SDMMC1@2a320000使用B4/B5，均disabled；无其他当前pinctrl消费者 | 不启用SPI1/SDMMC1相关overlay；GPIO hog与实际pinmux仍需获批只读核对 |
| UART/风扇 | 候选不占UART5 M0诊断；Pin4/6供电与19/23分离，不用Pin12 PWM | 不改风扇/诊断pinmux；实际杜邦线占用由用户确认 |
| BSP支持 | common/drivers/drv_i2c.c包含RT_USING_I2C9，g_i2c9Dev；M0 SConscript纳入common；当前rtconfig无I2C | 有源码框架，未注册/编译/运行；加RT_USING_I2C/I2C9/HAL_I2C并通过配置生成链 |
| IRQ | HAL soc.h BUS M0 I2C9=129+NUM_INTERRUPTS(32)=161，走INTMUX；Linux DTS GIC_SPI97 | 两域编号不可互换；路由/权限待审查和定向T2证据 |
| clocks | Linux CLK_I2C9=130/PCLK_I2C9=118；HAL gate0xd4/0xc8；Linux amp probe持bulk clocks | 派生amp资源节点加入这两clock且保持parent稳定；仅disabled节点不足以保护clock |
| reset | SRST_I2C9=212/SRST_P_I2C9=200；HAL reset0xd4/0xc8 | 复位仅由经审查RTOS初始化一次负责；禁止Linux另行reset/探测 |
| 动态clock风险 | RTOS xfer直接gate enable/disable；Linux CCF引用不感知M0写gate | 必须补选定总线held-clock模式，避免M0关Linux持有的gate；共享父时钟不能被随意改频 |
| 电源域 | I2C9 DTS没有单独power-domains属性 | 不能由此推断永远有电；查BUS/CRU/父域及Linux runtime/suspend；获批窗口不新增suspend |
| 安全访问 | 当前最小链只证明mailbox/shared子范围 | I2C9控制器/IOC/CRU/reset/IRQ的M0访问权限未证明；禁止盲目写SGRF/CON16/17，缺证据则停止 |

## 拟闭合的独占契约（尚未实现）

Linux I2C9始终disabled，无adapter、无从设备绑定、无用户态事务；不加载泛用i2c访问。
派生mcu-amp资源节点持有I2C9两clock和I2C9_M1 pinctrl，仍保留UART5；
CLK_I2C9选择稳定24MHz xin_osc0 parent（需核clock rate可实现），参考div来自实际rate。
Linux只负责保持供电/CCF资源，不发I2C事务。RTOS通过原框架独占事务与初始化；
选定I2C9关闭原逐事务gate切换（其余总线行为不变），reset/pad配置按启动顺序
一次设置且不与Linux重复写。若该最小适配或访问权限不能闭合，禁止部署。
M0先于Linux启动，因此sensor task须等资源就绪约定后才尝试地址；不能把固定sleep
当所有权证据。握手门明确资源就绪、总线配置、clock保持和超时诊断。

## 解除接线门

V2R0版本与引出设计门已解除。仅电源测量步骤已发给用户：正常关机并断电后，
模块VCC接Pin2、GND接Pin20；风扇Pin4/6不动；所有信号线保持不接板。
用户先核Pin1丝印/方向，再上电测模块VCC、LDO输出、SDA/SCL/AD0对GND的DC电压。
不得用电阻档测带电模块；不清楚LDO测试点则停止猜测。5V只能进模块VCC/LDO输入。
目前测量未回报，不发`WIRING_READY_FOR_USER`。
模块LDO型号/实际下拉结合实物核验，断电后检查AD0下拉；风扇电流/供电预算继续登记。
电平相容并确认Pin1后给正式信号接线表，用户断电接线并明确确认后才允许M0定向访问。
接线确认不授权boot/DT/KO部署。I2C runtime ownership及权限仍须派生配置和获批T2闭合。
