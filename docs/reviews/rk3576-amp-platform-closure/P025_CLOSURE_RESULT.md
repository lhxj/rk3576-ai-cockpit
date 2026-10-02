# P025：地址、coherency、AMP 入口与回滚

2026-10-03；基线 `agent/amp-platform-closure / 7bd7d9f`。**C. HOST_BUILD_PASS；不是 D。** 本轮只读 SSH 和 Host 工作，没有部署、写分区/寄存器、重启、加载 KO 或启动 M0。固定 reference 未改；以下补充历史，不能把 Host 配置当作当前板寄存器值。

## 1. 地址映射：Host 方案已收敛，运行状态未验证

TRM Part1 §8.6.2/Table8-6：`PA=B16+M0_code`；`PA=B17+M0_shared−0x20000000`，`Bx=CONx & ~0x3ff`。TRM没有CON16/17详细offset/访问表；`+0x60/+0x64`另由固定HAL布局和选定BL31写入路径确认。前轮Linux/U-Boot直接访问失败，本轮不重试。

唯一参数源仍为 `docs/amp/AMP_PLATFORM_CONTRACT.yaml`。`host_proposal` **未来拟用** B16=`0x47800000`、B17=`0x40000000`；final字段保持null。派生U-Boot先assert MCU reset，CODE/shared SiP检查返回值，再release；失败保持reset。固定BL31所选静态分支：SMC `0x82000028`，MCU id0，selector1写CODE相关CON14/15/16，selector3写CON17，shared参数1KiB对齐。这是 SOURCE_VERIFIED，不证明板上执行成功。实际CON16/17/MCU reset有效时序仍 **UNVERIFIED**。

| Region | Linux PA（拟配置后） | M0 view | Size | 证据 |
| --- | ---: | ---: | ---: | --- |
| RTOS全部code/data/bss/heap/stack预留及FIT最终load | `0x47800000` | `0x00000000` | `0x80000` | HOST_TESTED |
| vring0，Linux RX/M0 TX | `0x47d00000` | `0x27d00000` | `0x8000` | HOST_TESTED |
| vring1，Linux TX/M0 RX | `0x47d08000` | `0x27d08000` | `0x8000` | HOST_TESTED |
| payload，128×512B、64/方向 | `0x47d10000` | `0x27d10000` | `0x10000` | HOST_TESTED |

ELF local Thumb entry=`0x141`，vector reset指令PA拟为`0x47800140`；vendor standalone release的参数是FIT **load `0x47800000`**，它忽略FIT entry，不是调用PA=`0x141`。检查器已修正这三种语义；不能要求load与ELFentry数值相等。各section地址/stack/heap见 [Host内存图](MEMORY_LAYOUT_HOST_PROPOSAL.md)。

Host候选不用CPU3参考vring0=`0x47800000`，所以code与rings物理不重叠。9,672项实际ELF/FIT/DT/header检查和65,536B pool双向转换/越界检查本轮重跑PASS，保留P023原产物hash。恢复后运行DT与旧副本同hash，候选位于其首段RAM且不碰已有静态reserved-memory。**Linux reservation不约束U-Boot启动前的动态分配**：当前vendor `boot_get_loadable`先复制，`standalone_handler`后分配且只按BIN长度，尚未覆盖完整512KiB及shared预留。这个Host可见风险需单独解决，部署门保持BLOCKED。

RPMsg冻结为Host资源候选：Linux master、M0 remote，link`0x04`；Linux RX=MBOX0/ch0/SPI125、TX=MBOX4/ch0/SPI129；M0 TX=MBOX0/ch0、RX=MBOX4/ch0/NVIC175。Name Service=`rk3576-m0-echo`，两侧id_table一致。未建立实际link。

## 2. Coherency：选择 UNCACHED_SHARED_MEMORY，尚未实板证成

BUS MCU有Rockchip 16KiB unified I/D cache；TRM §8.6.4规定reset后bypass。本候选不启用该cache，HAL SystemInit保持CACHE_BYPASS（bit6）、DSB/ISB，RTOS cache模块关；前提是未来cold reset确实生效。**不是“Cortex-M0天然无cache”，不是hardware coherent结论**。

Linux侧：vring `ioremap` → arm64 Device-nGnRE；独占 `shared-dma-pool; no-map; 非reusable`，payload `memremap(WC)` → Normal-NC。派生transport必须成功关联该pool；失败拒绝fallback到普通cached页；virtqueue weak_barriers=false，使用device/DMA barriers。M0 env_mb/rmb/wmb为DSB+compiler memory clobber。producer先写payload/descriptor，再经屏障发布ring并notify；consumer取ring后经屏障读payload，used发布同样排序。空cache hook仅在**实际全局bypass**这一条件成立后合理，不据空实现直接判PASS。

SOURCE_VERIFIED/HOST_TESTED：TRM reset/bypass定义、HAL/startup关闭路径、Linux固定mapping/barrier代码及编译产物。UNVERIFIED：未来SMC/reset实际成功、运行cache控制状态、最终页属性和跨核可见性。选择方案已明确，**COHERENCY_RUNTIME_GATE=BLOCKED**。只用HELLO/PING做首次未来功能验证，不声称现有板端coherency已通过。

## 3. 当前板与恢复后的原件

BOARD_OBSERVED_READONLY：普通SSH exit0；Linux`6.1.99-rk3576 #8 Apr24 2026`，model`EmbedFire LubanCat-3-v2`，fwver U-Boot`8f53f800da-04/24/2026`、BL31`v1.14`；GPT只有uboot/boot/rootfs，**无amp**，运行DT无AMP/RPMsg节点，rpmsg设备目录空。

Image/active uEnv/DTB/boot.cmd/boot.scr/config六项SHA与之前原件一致；本轮重新复制原件tar和运行DT。完整文件身份/类型/软链接记录在 [P025_ARTIFACT_MANIFEST.json](P025_ARTIFACT_MANIFEST.json)。原始日志和boot文件只在忽略的本地目录，不提交。`/proc/iomem`地址被隐藏为0、lockdown文件不存在，均不证明物理空闲或secure boot关闭。

最新备份：`artifacts/local/p025-post-recovery-20261002T173913Z-975579/boot-originals.tar`，SHA256=`ed3ef563e21be440de84f68f57093d1e977e6cc10db25d41e6b682ed312b0b7c`；运行DT tar SHA=`61fae8ebac570606f536a8f4817425d04bf0226701bf97b3fd87cc59f7b01dde`。本轮没有读raw uboot分区，恢复后的fwver相同不等于重新观测了raw firmware hash。

## 4. AMP 加载与验签

详见 [P025_BOOT_LOAD_AND_VERIFY.md](P025_BOOT_LOAD_AND_VERIFY.md)。官方pin的唯一已证AMP入口：`CONFIG_AMP`下`board_late_init → amp_cpus_on → part_get_info_by_name("amp") → blk_dread → FIT → standalone → SiP/reset`，早于prompt；当前GPT无该源。`boot_fit/bootm/ext4load`存在不证明具有BUS MCU release。没有找到该pin中官方file/memory替代入口；结论 **AMP_PARTITION_REQUIRED_FOR_PINNED_LOADER**，不自动创建分区。

本轮新增派生U-Boot `96c9a009eed997c318cd247943e5fc88e8e1bcc6`，启用AMP/FIT_SIGNATURE，修正AMP绕过kernel-only配置验签入口的问题，增加partition边界检查、必需conf-key检查，失败不复制/release。fresh完整Host build exit0，实际C提取测试46项+3顺序检查PASS，cold-reset既有fault测试26+1 PASS。23条warning均为Host工具上游（22条OpenSSL3废弃API、1条未执行bmp工具格式问题），目标改动0warning。

新Host uboot package保留恢复镜像中的BL31/OP-TEE/控制DT字节，仅替换U-Boot code；4MiB双FITslot结构和hash检查PASS。**未签名，不可部署**；编译进验签能力不等于已有合法签名/公钥。

用户随后提供完整串口日志：SPL `Verified-boot:0`、六个原厂payload SHA检查通过，U-Boot/BL31/OP-TEE版本与已有文件相符。记录的SPL FIT不要求签名，属于BOARD_OBSERVED_USER_REPORT；不能等价成OTP全局状态。后续正常`booti`没有触发AMP policy路径。proper U-Boot另通过OP-TEE读flag，失败required=1，实际结果/key provisioning仍UNVERIFIED；不能关闭policy来绕过。见 [串口验签证据](../rk3576-amp-board-evidence/SPL_VERIFIED_BOOT_SERIAL_EVIDENCE.md)。

## 5. 清单及下一步停止点

[P025_DEPLOYMENT_CHANGESET.json](P025_DEPLOYMENT_CHANGESET.json)由contract/产物和实机备份生成：逐对象source、拟destination、旧/新hash、symlink类型、用途及rollback。AMP分区destination和当前raw U-Boot hash为null，自动拒绝；不能将拟路径当作实际存在。人读步骤见 [P025_DEPLOYMENT_AND_ROLLBACK.md](P025_DEPLOYMENT_AND_ROLLBACK.md)。

`check_deployment_packet.py`：**Host integrity PASS，deployment BLOCKED，Linux exit2**；`--allow-blocked-for-host`只检查文件身份，不是部署许可。canonical合同同样保持BLOCKED，不改成D。

剩余具体事实/准备：当前验签policy/合法信任key；AMP GPT与rootfs/用户数据安全方案；完整code/shared的U-Boot复制前保留；动态SiP/映射/cache；kernel/module升级身份与UART5实时占用/TTL接线；未来写入前raw bootloader身份重新核验。整机恢复实测用户已确认PASS，恢复后的普通boot原件已核，不再把“未下载恢复镜像/未测整机恢复”列为当前blocker。

完整串口日志已取得，SPL策略缺口部分关闭；下一项所需证据是proper U-Boot `trusty_read_vbootkey_enable_flag`的实际返回/策略，或厂商对该发布OP-TEE/信任链的确定说明。现有日志没有此调用，不能从`security partition`缺失猜测。后续任何启动链/分区/SMC诊断改动按AGENTS L3逐项批准；不重试受保护CON直接访问。**APPROVAL_GATE_BOARD_TEST仍关闭；首次未来测试仅Linux正常启动+M0 banner+HELLO/PING。**
