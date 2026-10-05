# 接线与 I2C ownership 审查

2026-10-05最新：**WIRING_READY_FOR_USER / USER_POWER_OFF_WIRING_CONFIRMATION_PENDING**。
主控attempt4取得M0十地址白名单只读访问与派生Linux owner/clock实测证据，诊断正常退出、默认冷恢复25文件hash PASS。完整传感器业务未运行。下方早期“未闭合”段落保留为历史，以本条和末尾最新表为准；当前SDA/SCL仍不接板，下一步由用户断电接线后明确确认。

## 版本门与来源

用户最终更正确认实物PCB：**EBF410513V2R0 20260521**；软件实时model为
**EmbedFire LubanCat-3-v2**，与取得的原理图 **EBF410513V2R0_SCH_20260521.pdf**一致。
用户最初提供V0R1 20240808后明确更正，曾据此暂缓接线；当前以更正为准。
不能从DT名称反推物理板版本。依据[板厂硬件资料](https://doc.embedfire.com/lubancat-rk/hardware/ebf_lubancat_rk_hardware/zh/latest/board/ebf_lubancat_rk.html)
中的对应版本图纸核对引出与供电。PCB版本门已解除。用户已回报模块输入/信号静态电压；用户也确认按Pin1丝印识别方向，Pin19/23空闲，Pin20接串口调试器GND。

来源与hash（原始PDF/图片/归档只留忽略目录）：

| 资料 | 身份/用途 |
|---|---|
| 用户三张模块截图 | 用户提供设计/商家资料；不是实物测量。图1 AD0 R6=4.7kΩ，图2文字10kΩ，存在差异；图3底部还有串联电阻版本。不能据此确认实物R6 |
| [板厂40Pin图](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/40pin/40pin.html) | lbc3-40Pin.jpg SHA256 `ac980260d0e0b9ffdbfdefbe076e551a4319da96cb418354940b14acb369e74d`；未标明V0R1兼容范围 |
| [V2R0资料包](https://mall-ebf.oss-cn-shenzhen.aliyuncs.com/product/sync/DEMO_BOARD/LubanCat3/Datasheet/LubanCat3_20260521.zip) | SHA256 `66dccd86321f29e04861a6fab8a594398ab041747e31073f5c5926b9bda6571c`；视觉核对sheet3/15/24与PCB尺寸图，仅设计依据 |
| 板厂V2R0变更通知 | 明确RTC从I2C2迁到I2C3；提醒软件/物理版本匹配，不能用当前DTS证明旧PCB走线 |
| Linux固定源码 | kernel `521833e2d28decbd6473d5717f1f96cc4108e208`；rk3576.dtsi、pinctrl、lubancat-3/-v2、clk-rk3576.c、rockchip_amp.c |
| RTOS/HAL固定源码 | RTOS import8541f7a→3a39b0f+v5补丁；HAL277de3f→bc99978；SDK保持外部 |

## 候选接线表（V2R0设计/实物版本已核，信号电压与Pin1/占用已确认）

| MPU模块pin | 板物理Pin | SoC pad/复用 | 控制器 | 电压域/约束 |
|---|---:|---|---|---|
| VCC | 2（用户已接） | J12 DCIN_5V0，无SoC pad | — | 用户测得模块VCC约5V；仅模块输入，不能接芯片VDD |
| GND | 14（用户已接） | J12 GND，无SoC pad | — | 用户称Pin20已占用，改用Pin14；不改风扇Pin6 |
| SCL | 19（候选） | GPIO1_B5，I2C9_SCL_M1，function10 | SoC I2C9 | **VCCIO3**，V2R0 sheet15接VCC_3V3_S0；不是VCCIO1，不由GPIO bank号推电压 |
| SDA | 23（候选） | GPIO1_B4，I2C9_SDA_M1，function10 | 同一SoC I2C9 | 同上；模块4.7kΩ上拉设计到LDO 3.3V，用户测得SDA/SCL各约3.3V |
| AD0 | 不接（有条件） | — | 预期7-bit地址0x68 | 用户测得静态约0V，但未测R6阻值；WHO_AM_I仍应0x68，与地址0x69选择不同 |
| XDA/XCL/INT | 不接 | — | — | 第一版不启用辅助总线/中断 |

**40Pin方向依据**：V2R0原理图sheet24的J12与该包PCB尺寸图一起核。
按该尺寸图的元件面观察，J12在板边；靠Type-C/Maskrom/On-Off一端为1/2，
另一端靠USB-A/RJ45为39/40。靠板内为奇数排，靠板边为偶数排。
Pin1同排后续为3/5；Pin2同排后续为4/6。不可仅凭“左上角”或树莓派照片识别。
用户续审明确确认已依据实物Pin1丝印/标志数出Pin2/14，候选Pin19/23空闲。

用户确认风扇VCC=物理Pin4、GND=Pin6，当前正常转动，电源5V/4A。
保持原供电，不增加PWM/GPIO。风扇标注电流未知，4A额定值不是系统剩余预算；
需记录实际风扇电流或可靠规格、现有负载与5V压降，测电流不得把表跨接5V/GND。
模块已由用户供电并完成静态测量；LDO输出测试点没有直接测量。风扇振动与安装方式进入采样记录。

## 资源核查结论

[实时脱敏DT索引](resource-evidence.json)，原始普通用户SSH证据留
`artifacts/local/hardware-review/live-ownership.txt`与
`artifacts/local/p025-post-recovery-20261004T155744Z-241045/`。
持共用board_lock，无全总线扫描/强制访问/MMIO/GPIO改写。

| 项目 | 已核依据 | 结论/后续门 |
|---|---|---|
| 控制器/MMIO | SoC I2C9=/i2c@2ae80000，Linux PA0x2ae80000，span0x1000；HAL_MCU_CORE加0x20000000 | M0 HAL指针0x4ae80000为源码地址；没有实板访问证明，不套共享RAM CON17换算 |
| 引脚 | Linux pinctrl i2c9m1-xfer=<1,13,10>,<1,12,10>；设计J12 pin19/23 | V2R0设计引出已核，模块SDA/SCL电压已回报；Pin1方向和19/23空闲已由用户确认 |
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
| 电源域 | TRM Part1 p618表6-1明确I2C1~9、BUS_MCU、CRU、INTMUX_2BUS属于PD_BUS（Virtual Power Domain, ALIVE）；I2C9 DTS无单独power-domains | 电源域归属SOURCE_VERIFIED；仍需保持总线父时钟和板端VCC_3V3_S0，不能推定suspend/安全访问；获批窗口不新增suspend |
| 安全访问 | 当前最小链只证明mailbox/shared子范围 | I2C9控制器/IOC/CRU/reset/IRQ的M0访问权限未证明；禁止盲目写SGRF/CON16/17，缺证据则停止 |

## 拟闭合的独占契约（尚未实现）

Linux I2C9始终disabled，无adapter、无从设备绑定、无用户态事务；不加载泛用i2c访问。
派生mcu-amp资源节点持有I2C9两clock和I2C9_M1 pinctrl，仍保留UART5；
CLK_I2C9选择固定24MHz `xin24m` parent，参考div来自实际rate。固定Linux源码确认该parent为mux值3，不改共享PLL或pclk_bus_root。
Linux只负责保持供电/CCF资源，不发I2C事务。RTOS通过原框架独占事务与初始化；
选定I2C9关闭原逐事务gate切换（其余总线行为不变），reset/pad配置按启动顺序
一次设置且不与Linux重复写。若该最小适配或访问权限不能闭合，禁止部署。
M0先于Linux启动，因此sensor task须等资源就绪约定后才尝试地址；不能把固定sleep
当所有权证据。握手门明确资源就绪、总线配置、clock保持和超时诊断。

## 用户测量记录（2026-10-05续审）

用户使用照片所示万用表，黑线COM、红线VΩ、直流20V档。以下为
**USER_REPORTED_MEASUREMENT**，由用户文字回报，未取得测量点/读数照片：

| 测量项 | 用户回报 | 可支持的结论 |
|---|---|---|
| 模块VCC对模块GND | 5V左右 | 输入电源约5V，接线为VCC→Pin2、GND→Pin14 |
| SDA对模块GND | 3.3V | 信号未接板条件下静态上拉电平符合3.3V设计 |
| SCL对模块GND | 3.3V | 同上 |
| AD0对模块GND | 0V | 地址选择预期0x68；不证明R6阻值或WHO_AM_I |
| LDO输出测试点 | 未测 | 没有用SDA/SCL测值冒充LDO输出直接测量 |

用户续审确认：依据Pin1丝印/标志数针脚，Pin19/23均空闲，Pin20接串口调试器。
Pin20是开发板GND，此处按调试器地线理解；不移动该连接，不接触调试器电源/信号。
测量前已要求所有信号线保持不接板；没有取得正式断电信号接线确认。
5V/4A与风扇正常转动不能证明剩余供电预算；不要求初学者使用电流孔跨接电源。

## 续审：最小派生适配必须覆盖的副作用

以下均为固定本地源码检查；没有修改原RTOS/HAL/Linux SDK，没有板端MMIO操作。

1. `common/drivers/drv_i2c.c:393–415` 的 `INIT_PREV_EXPORT` 自动probe；
   `:274–287` 在probe中调用HAL_I2C_Init（继而写DIV/CON）。不能只让sensor_task
   等待：I2C9自动probe本身也必须延迟到Linux资源就绪后。派生配置只启用I2C9，
   跳过其PREV自动probe，提供单次、幂等、错误可返回的延迟注册入口。
2. `drv_i2c.c:164–165,228–229,285–287` 与PM resume均会直接gate；
   `drv_clock.h:68–89` 调HAL_CRU，不具备Linux CCF引用管理。
   I2C9 held-clock模式要覆盖probe/xfer/resume的所有路径，不是只删除事务末尾disable。
   M0不改CLK_I2C9 parent、不碰共享PLL、不给其他总线套该策略。
3. `rk3576-mcu/board/evb/iomux.c:66–68` 的通用RT_USING_I2C条件
   无条件调用i2c7_m1_iomux_config。必须在派生补丁中让选定I2C9配置不触发它；
   原文件没有I2C9_M1配置。候选由Linux mcu-amp默认pinctrl独占配置19/23，
   M0不在早期board_init再写IOC。保留UART5原配置。
4. `drv_i2c.c:59,172,191` 混用RT tick数和毫秒，并忽略bus.timeout；
   修正选定总线事务预算和单位，重试有上限。HAL_I2C_Init返回值在原probe中未检查，
   自动init也忽略probe结果；派生入口必须传播错误，失败不得报I2C_READY。
5. Linux `clk-rk3576.c:321,603–604,635–637` 确认
   PCLK_I2C9是CLKGATE_CON(12) bit8，CLK_I2C9是CLKGATE_CON(13) bit4，
   CLKSEL_CON(58)[1:0]可独立选xin24m。不能用整寄存器覆盖共享其他设备位。
   `rockchip_amp.c:671–677`持bulk clocks直到remove；保持该驱动绑定，不执行unbind。
   该驱动没有运行期独占rate保护；审批窗口须禁止重新parent/rate和suspend，
   部署前/运行窗口记录clock parent/rate/enable证据。不是只以probe成功裁决。
6. TRM Part1 V1.2/20240624，SHA256
   `6094ae5874d8494e73fa363d9cf35dd65acbd54a9a9d633b1ba5e4bea289f0a8`，
   本轮重新hash并视觉核对p618；p762表8-6确认外设窗口映射，
   结合HAL得到I2C9 M0地址0x4ae80000。以上不提供当前firewall权限证据。

拟资源就绪时序：M0启动仅health/sensor控制面→Linux mcu-amp成功保持clock/pin→
Linux预检匹配DT、绑定、I2C9无adapter、clock身份和获批接线→独立sensor控制消息
声明本次remote epoch对应资源就绪→M0单次延迟初始化I2C9→定向WHO_AM_I。
就绪消息不等于已取得安全访问权限；未闭合权限来源时仍禁止发起访问。
无就绪/初始化失败时只发布STATUS，health保持响应，I2C访问次数应为0。
取消订阅不关共享时钟；资源释放沿获批冷恢复进行，不在M0持续运行时unbind。
这是待实现契约，Host测试必须证明早期/重复就绪/失败路径；不是已实现行为。

## 解除接线门

模块静态信号电压与V2R0设计3.3V域相容检查通过，AD0低电平支持预期0x68。
Pin1/候选19/23空闲及Pin20串口调试器占用已由用户确认；
I2C9运行期所有权/安全访问仍未闭合。
当前不发`WIRING_READY_FOR_USER`，不要求用户继续接SDA/SCL。
已接电源可以保持；调整/拆线须用户正常关机并拔电，风扇4/6不动。
后续派生补丁/DT/build完成且权限依据明确后，发布正式信号接线表与断电确认门；
真实WHO_AM_I/配置读回仍只能在明确部署审批后的T2取得。
接线确认不授权boot/DT/KO部署。本轮没有MPU6050 I2C事务。

## 2026-10-05 当前续审结论（覆盖上述“待实现”现状，不改写历史）

0001 的早期访问/held I2C clocks/I2C7 mux/tick 与错误路径已 Host 修复；0002 延迟解除 I2C9 两项 reset，0003 只持有 INTMUX2BUS 专用 gate。派生 DT 在原 mcu-amp 上增加 I2C9 clocks/pinctrl、固定 xin24m，保留总线 disabled；非目标资源逐项一致，包含 GPIO hog/消费者与 SPI/SDMMC pin 冲突检查。配置/实际驱动回归和原生诊断构建已通过，详细来源、范围与hash见 [PREWIRE_RESOURCE_RESULT](PREWIRE_RESOURCE_RESULT.md)。独立 sensor resource-ready 控制消息和完整采集业务仍未实现。

HAL_BUS_MCU_CORE 的 RK3576 soc.h 为 NUM_INTERRUPTS=32、INTMUX_IRQ_START_NUM=64、每输出一个32位 group；I2C9_IRQn=129+32=161，hal_intmux.c 实际使用不定义 INTMUX_IRQ_INTEN_L_OFFSET 的后半实现：减64和32得到65，group2/bit1，INTMUX_OUT2_IRQn=18。不引用前半另一硬件布局。INTMUX gate 常开方案只在固定 Linux CCF 源码范围成立，闭源 suspend/安全固件管理不据此宣告绝不关钟；审批运行窗口必须禁止 suspend。

L1普通用户实时盘点确认当前默认 Debian/I2C9 disabled/无绑定/无 AMP。当前 BL31 v1.14 与本地保留二进制版本有关联，未能逐字关联其 firewall policy。静态 ownership 配置可审查，实际 BUS_MCU 访问许可、INTMUX 初始 reset 状态尚需厂商精确依据或另批有限无传感器验证；本轮不发 WIRING_READY_FOR_USER、不访问 MPU。电气兼容与供电事实已确认，不把 debugfs 无权限扩大为电气测量缺口；后续验证可在信号线尚未接板时完成。

## 2026-10-05正式接线表（最新，等待用户断电执行）

**WIRING_READY_FOR_USER**。V2R0设计引出、电压相容、用户Pin1/占用确认、Linux派生ownership与M0定向白名单读证据已闭合到接线前范围。实际结果见[RESOURCE_PROBE_BOARD_RESULT.json](RESOURCE_PROBE_BOARD_RESULT.json)。当前已恢复默认Debian，不在AMP诊断环境；不再运行诊断/冷启动。

| 模块引脚 | 开发板物理Pin | SoC pad/复用 | 控制器 | 电压域/依据 |
|---|---:|---|---|---|
| VCC | 2（已接） | J12 DCIN_5V0，无SoC pad | — | 用户测约5V；仅模块LDO输入，不接芯片VDD或SDA/SCL |
| GND | 14（已接） | J12 GND，无SoC pad | — | 共地，风扇Pin6/串口Pin20保持 |
| SCL | 19（待接） | GPIO1_B5 / I2C9_SCL_M1 / mux10 | SoC I2C9，Linux PA0x2ae80000，M0地址0x4ae80000 | VCCIO3，V2R0设计3.3V；用户模块SCL实测3.3V相容 |
| SDA | 23（待接） | GPIO1_B4 / I2C9_SDA_M1 / mux10 | 同一SoC I2C9 | 同一VCCIO3设计3.3V；模块SDA实测3.3V相容 |
| AD0 | 不接 | — | 预期7位地址0x68 | 用户静态实测0V；设计R6下拉阻值未实测，不宣称已测4.7kΩ；WHO仍待读取 |
| XDA/XCL/INT | 不接 | — | — | 第一版轮询不使用这些引脚 |

方向继续以实物Pin1丝印为首要依据：按V2R0尺寸图元件面观察，靠Type-C/Maskrom/On-Off端为1/2，另一端为39/40；靠板内奇数排、板边偶数排。用户已按Pin1标志确认Pin2/14及空闲Pin19/23。不能用GPIO号或通用树莓派图替代物理编号。

用户接线步骤：先正常关机，拔主电及可能反向供电的USB电源，确认板与模块断电；保持现有VCC2/GND14、风扇4/6、串口GND20连接位置，仅增加SCL→19、SDA→23，检查没有相邻短接。固定模块与杜邦线，不移动MIPI/整板，不触碰风扇叶片。完成后明确回复断电接线确认；在该确认前不访问MPU。用户无需重复已有电压测量或测微小LDO点。风扇电流/系统剩余预算未知保留到后续获授权负载记录，不能仅凭5V4A推导。

本次派生DT保持I2C9 disabled/无adapter及从设备，Linux mcu-amp持两clock/pin并固定xin24m；诊断KO额外CCF引用/rate保护在有限窗口内实测通过。M0只读10个白名单地址均返回，相关reset未assert、gate已开，INTMUX只读返回；不是BSP写初始化、I2C交易或IRQ投递PASS。默认恢复后这些AMP资源不继续占用；正式采集必须重新使用经过审查的派生sensor组合与resource-ready门。不能把本次读访问推广为全部BL31权限或suspend保证。

WHO_AM_I/配置/100样本/RPMsg业务/Core/Qt/人工方向变化/五分钟共存均NOT_RUN。接线就绪与接线确认不替代正式MPU_SENSOR_V1业务构建、部署及对应窗口要求。

## 2026-10-05 用户断电接线确认（最新）

用户明确回复“已断电接线完成”，USER_CONFIRMED；SCL19/SDA23按既定表接入，电源2/14及风扇4/6、串口GND20保持。没有因此访问MPU或取得WHO/配置/样本证据。driver/sampler Host子里程碑见DRIVER_HOST_RESULT；下一步service/codec/epoch/订阅Host，不把未接control面的native FIT部署上板。
