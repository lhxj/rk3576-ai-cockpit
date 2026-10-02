# P023 最小 echo 资源核对

| Resource | Candidate owner | 当前证据 / 限制 |
| --- | --- | --- |
| BUS M0 | RTOS / FIRMWARE reset/remap | CODE/shared SiP与CRUreset静态链；没有release或runtime值 |
| code DDR | RTOS | 512KiB no-map Host reserve；在运行DT RAM内 |
| shared DDR | SHARED_CONTROLLED | 2 rings+专用pool共128KiB；见候选layout |
| MBOX0 ch0 | M0 TX / Linux RX SPI125 | 固定port master id0；当前节点disabled |
| MBOX4 ch0 | Linux TX / M0 RX IRQ175 | remote id4；当前节点disabled |
| UART5 | RTOS | 40Pin16 RX /18 TX，GPIO3_D4/D5、mux9；Linux UART5 disabled |
| Timer | RTOS private SysTick | 24MHz external tick，RT tick100Hz；HWTIMER关闭，SYS_TIMER不定义，HAL_Init不访问TIMER11 |
| CRU clocks | SHARED_CONTROLLED | MCU207/208/209、PCLK_UART5=139、SCLK_UART5=150；Linux mcu-amp持有lease，UART5=24MHz；不占共享timerroot |
| Reset | FIRMWARE | TOPCRU SOFTRST_CON19[11:13]，write-mask；Linux overlay不请求这些reset |
| GPIO | Linux；两脚IOMUX拟RTOS | GPIOdriver关闭，仅UART5pinmux；不碰PERI5V/LED/其它bank |
| I2C3/6/7/8 | Linux / future未分配 | 最小固件全关闭；I2C3 RTC的m0引脚GPIO4_C12/C13与UART5不相交 |
| shared PLL / power | FIRMWARE / Linux | MCU不初始化sharedPLL/PM；UART5 HAL选择24MHz oscillator；既有Linux Camera/Audio/Wi-Fi/Display DT属性保留 |

U-Boot拟写（SOURCE_VERIFIED，未执行）：TOPCRU `0x27200834 ← 0x40000000`（UART5 PCLK），`0x27200838 ← 0x80000000`（UART5 SCLK），`0x27200844 ← 0x20000000`（共用mailboxbus PCLK），`0x2720084c ← 0x5c000000`（BUSMCUgate），`0x27200a4c ← 0x38003800/0x38000000`（assert/release）。mask只针对固定HAL中对应gate/reset位；不写PLL，也不扫描其它MMIO。

## UART5 的分层裁决

- SOURCE_VERIFIED：中文硬件规格书PDF第20页（印刷19）及英文对应40Pin表确认16/18脚；Debug串口另一路，规格书第16页给出1500000 8N1；M0 UART5候选115200 8N1。
- BOARD_OBSERVED_READONLY + HOST_TESTED：原`/proc/device-tree`里UART5 disabled；自动追全部启用节点及父节点的pinctrl/GPIO引用，未发现GPIO3_D4/D5拥有者。存在I2C3 m2 pinctrl组并不代表启用；实际RTC使用m0组。
- BOARD_OBSERVED_READONLY：`cat`用户读取`/sys/kernel/debug/pinctrl`被拒绝；`/sys/class/gpio`没有exported gpio条目。没有sudo、GPIO request、I2C扫描或改pinmux。
- **UART5 runtime = UNVERIFIED；DT allocation = AVAILABLE。** 不能把DT无冲突扩大为无动态GPIO用户、TTL线已接好或电平已测。

待批准首次测试的接线：独立USB-TTL TX→40Pin16（M0 RX）、RX→18（M0 TX）、GND→6/20；仅信号/GND，不给40Pin供电。使用与板I/O电平兼容的适配器，接线由用户核规格并完成。已有Debug串口保持1500000用于U-Boot/Linux恢复；M0 UART5需要另一路115200记录。未确认第二适配器和电平，不称console ready。
