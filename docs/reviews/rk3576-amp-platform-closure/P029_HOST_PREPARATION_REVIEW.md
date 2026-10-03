# P029：Host 分阶段测试包审核

2026-10-03，基线 `agent/amp-platform-closure / af08db1`。用户要求“开始准备”。
**本轮没有访问开发板；板端暂存、配套Linux启动和M0/RPMsg均未执行。等级仍 C. HOST_BUILD_PASS，D关闭。**

## 交付与变更

桌面：`C:\Users\27432\Desktop\RK3576-AMP-P029-StagedTest-PendingApproval`。21文件逐项SHA复核与Host原包相同；身份见 [P029_PACKET_MANIFEST.json](P029_PACKET_MANIFEST.json)，操作见 [P029_STAGED_TEST_GUIDE.md](P029_STAGED_TEST_GUIDE.md)。

| 阶段 | 已生成的候选 | 实板结果 |
| --- | --- | --- |
| A | 配套Linux/initrd/模块；code/ring/pool exact no-map；mcu-amp/RPMsg/mbox0/4 disabled | UNVERIFIED；待单独批准 |
| B | 新冷启动、一次显式M0调用后立即booti；Linux transport关闭；M0入口/cache/15s等待诊断 | UNVERIFIED；待A及串口方案 |
| C | 再次冷启动、同一有界M0诊断、手工配套echo KO、实际rings/DMA/payload/HELLO_ACK/PONG观测 | UNVERIFIED；待B及单独批准 |

现有Debug USB-TTL足够A。用户确认只有这一个，B/C尚需落实UART5采集方式；不强行移动Debug或声称双串口已准备。40Pin16/18空闲及3.3V电平是USER_CONFIRMED，不是本轮接线观测。

不含新U-Boot镜像，无GPT/AMP分区改动。安装候选只新增 `/boot/amp-p029` 和独立 `6.1.99-rk3576-m0echo-p026` 模块目录；原factory六文件/链接保留。默认启动不扫描新目录，测试靠Ctrl+C后的显式脚本。新增文件暂存不会调用M0。后续L3逐阶段批准；“Host准备”不是板端许可。

## 来源与身份（SOURCE_VERIFIED / HOST_TESTED）

- P028 U-Boot源码 `f8b4554584dd475ce783c605850c5e883b0a0fd4`；此前完整8MiB读回SHA `9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e`、policy0、原Linux/CLI通过均沿用P028独立证据，本轮没有重读。
- 固定RTOS/HAL不变；P023派生RTOS `3a39b0f4f5eb25631d05b4ffd88746ce9b1b0cdb` + 0010诊断补丁，HAL派生 `bc99978c1a030ad79610e89a5780dcd0ee3bb1f2`。源码由Git archive放fresh忽略目录；scons clean build exit0，零warning。BIN123336B、SHA `40ccde0f4ec1c941fcc1acbd1437a8e4a193e89da2db0682981bd5ae224f1727`；ELF入口/reset vector `0x141`、MSP `0x80000`，两LOAD总内存extent精确到 `0x80000`。
- FIT127488B、SHA `b39f966f4946b166809bf109d35d26958e5039326162b0648f8ddd3ba1e875bf`；actual外部payload逐字节匹配新BIN。当前policy0下的未签名受控测试候选，非部署许可；不改key/OTP/验签策略。
- Linux固定 `521833e2d28decbd6473d5717f1f96cc4108e208` + 0003已审查transport patch，Image/255modules/initrd/KO来自此前独立同次构建。三DT由与factory字节一致的v2 base + 官方CAM0 overlay + proposal AMP overlay离线派生；实际reg/no-map/status检查PASS。未声称CAM0回归。
- contract SHA `216484d601025308498181dfccaf993c81d5101eb4d44d42392c3ac48e871cc7`，canonical final字段/审批门未更改；0010只增加有界运行取证，不修改linker/ITS/最终内存图。

## 缓存、地址观测的取证范围

TRM Part1 §8.3.3 p753–754给出DCACHE system-view `0x23810000`、CACHE_CTRL offset0、bit6 bypass/RW；HAL MCU_OFFSET `0x20000000`得到M0-local `0x43810000`。厂商SystemInit/HAL有实际RMW；本轮二进制反汇编也确认RMW置0x40、DSB/ISB。**仅M0自身读取这个已定义配置寄存器；不重试CON16/CON17的Linux/U-Boot读。** 表头W是word宽度，不是write-only。

0010打印实际代码函数地址、entry/link/after-pong cache值、收到的payload指针；bypass缺失在remote_init前停止。link最多15s，echo最多180s/4请求。坏指针/不足header/越界长度不执行会解引用header的free，正常buffer归还一次，free失败不send。PONG后只清本地queue/endpoint；MCU仍运行，Linux可能留stale device，退出不等于MCU复位。

Linux `virtio_rpmsg_bus.c` 在实际dma_alloc_coherent后dev_dbg输出backing；C启用该文件动态debug，实际rings由transport记录。未来实际代码执行 + Linux backing + M0 received pointer + Linux收到HELLO_ACK/PONG共同证明**使用子范围的功能映射**；`pa_proposal`自身不算原CON读回，不证明整个512MiB窗口、setter内部时序或硬件cache coherency。缓存输出只证明那些时刻的bypass状态。

## 内存保护与安装审查

实际加载/运行边界由Image头、文件长度和DT budget计算：Image文件 `[0x40400000,0x42d30200)`，运行/重定位到`0x42df0000`；initrd `[0x4a200000,0x4a84c22e)`；DT budget `[0x48300000,0x48380000)`；脚本 `[0x4c000000,0x4c010000)`。均不交code `[0x47800000,0x47880000)`、shared `[0x47d00000,0x47d20000)`及OP-TEE `[0x48400000,0x49400000)`。

实际P028 C/libfdt对B DT early preflight=0；**booti runtime fixup前 final memory check=-22，属于尚无/memory节点的已知失败，不记录为最终内存PASS。** 目标booti必须从实际gd DRAM填入/memory并经过同一no-map/coverage guard。保护范围是经审查的FS/FIT/booti路径，不是硬件firewall或任意CLI内存命令保护。

独立子代理最初指出安装TOCTOU/原子mkdir/根tar解包风险。主控修正后子代理v3终审PASS；最终v4只补A只读核验指南及重新封装脚本时间戳，安装器、DT/FIT/内核内容不变，子代理对新增命令和最终包复审PASS。主控另实测实际snapshot Python块的6类情况：正常、软链接、FIFO、超限、目标已存在、快照后源变更。所有持久输入从root0700 RAM快照，可信Host checksum-list SHA核验再核各文件；拒绝特殊文件/链接/重复及覆盖，新目录原子创建，module tar只解入核实的新release，支持合法merged-/usr，三receipt便于后续清理。**没有实际执行安装器持久写分支。**

最终可信SHA256(SHA256SUMS)：`3b7ae414d4a23474ce487d438c952bc6c1e796bb38f2611d599f7206dfbeb7c4`。
包MANIFEST SHA：`9203e78346f95ba72a1a8a9f4a033b445be2e9e2a8b3d2d3ef5282e642ac3943`。
安装器SHA：`1e007ea4fb4e51a234be9a34b520472fa9af1c2a30905bbc1ddd3327290d9f61`。

## 实际Host验证

```sh
bash scripts/amp/p029_build_m0.sh "$PWD/artifacts/local/p029-m0-clean-v3"
python3 scripts/amp/p026_build_fit.py --contract docs/amp/AMP_PLATFORM_CONTRACT.yaml --bin artifacts/local/p029-m0-clean-v3/rtthread.bin --output artifacts/local/p029-fit-clean-v3
python3 scripts/amp/p029_make_packet.py --output artifacts/local/p029-packet-v4
python3 scripts/amp/p029_test_packet.py --packet artifacts/local/p029-packet-v4 --source artifacts/local/p029-m0-clean-v3/source/bsp/rockchip/rk3576-mcu/applications/amp_echo.c --report artifacts/local/p029-test-packet-v3.json
python3 scripts/amp/p026_test_preload.py --source artifacts/local/p028-uboot-factory-boot --output artifacts/local/p029-preload-stage-B-v1 --fit artifacts/local/p029-fit-clean-v3/amp-host.itb --linux-fdt artifacts/local/p029-packet-v2/boot/stage-B.dtb
bash -n scripts/amp/p029_build_m0.sh scripts/board/p029_stage_assets.sh
bash scripts/dev/host_ci.sh
```

构建、封装、哈希、Host解析与语法PASS。67包/脚本故障项、实际M0 C的14项、实际snapshot块6项PASS；实际P028 C harness：20 reservation、113 order、16 buffer、54 real-libfdt及9 integration checks PASS；实际B early/final结果如上。B DT在v2/v3/v4 SHA相同。完整CI：2 CTest、41 Python tests、5 withdrawn-writer tests PASS。

脚本故障检查用Bash共同语法+命令stubs，M0 C用Host HAL/RTOS/RPMsg stubs；**没有执行目标U-Boot HUSH、M0 MMIO、Linux transport或固件**。这些Host测试不能当作BOARD_OBSERVED。

## 下一步及剩余门

先按AGENTS.md L3批准A：仅被动新增文件和用户冷启动配套Linux，M0/transport关闭；核release、DT/tag/no-map、iomem与日志。A通过后再解决UART5采集并分批批准B/C。异常即停，M0尝试后不热重试或用原DTfallback；完整冷断电后原factory启动回滚。

新Linux实际启动、MCU SMC setter/reset、有效code/shared映射、cache runtime、双向RPMsg、回滚后原Linux回归尚UNVERIFIED。已成功的原件恢复属于P028历史，不替代新阶段回滚实测。**P029_HOST_PREPARATION=PASS；STAGE_A/B/C=UNVERIFIED；AMP=C. HOST_BUILD_PASS；D=CLOSED。**
