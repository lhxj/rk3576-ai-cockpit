# RK3576 AMP platform closure — 2026-10-01

## 最新裁决：P028（2026-10-03）

**15:09新P028读回PASS，待正常冷启动。** 用户改选P028文件后成功导出，Host实际8MiB逐字节与候选一致，完整SHA9bd8cc03...71c6d5e。现批准范围允许用户正常冷启动到原Debian；新Linux结果仍UNVERIFIED、M0关闭，AMP仍C。此前original-only mismatch保留，未倒改历史。见 [最新读回及历史](P028_LINUX_ONLY_EXECUTION.json)。

**本次新候选读回BLOCKED：** Host新导出8MiB逐字节为原件（SHAae0a507...461de8a / payload c084f257...），不等于获批P028（SHA9bd8cc03...71c6d5e / payload2e1b965...）。尚缺本次写入日志/所选文件证据，原因UNVERIFIED，暂不进行新候选冷启动或重复刷写。原候选身份仍核对正确，本次不能判新P028写入成功/启动失败，整体C。见 [实际读回与有限批准](P028_LINUX_ONLY_EXECUTION.json)。

**Linux-only下一步已获批准，尚未执行确认。** 用户只批准新P028 U-Boot测试原Linux，M0保持关闭。候选/原件/MiniLoader再次核hash，待用户MASKROM/RAM Loader、实际EMMC/GPT核对、写入及独立读回/冷启动证据。见 [有限批准](P028_LINUX_ONLY_EXECUTION.json)。整体C，不解锁AMP部署或D。后文“待审批”保留为批准前状态。

**新候选Host审核通过、板测待审批：** 主控独立clean build及181项实际C检查、FIT/preload/full CI PASS；原BL31/TEE/control DT与实板备份一致。只交付8MiB Linux-only待测镜像，SHA `9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e`，不自动部署、不启动M0。新proper U-Boot实际policy与原Linux兼容仍UNVERIFIED，旧P026保持撤回，canonical D门关闭。见 [Host审核](P028_HOST_REPAIR_REVIEW.md)、[新候选身份](P028_CANDIDATE_MANIFEST.json)。

**恢复已验收：ORIGINAL_UBOOT_READBACK=PASS；ORIGINAL_LINUX_COLD_BOOT=PASS。** 8MiB读回SHA与原备份相同；用户原始串口日志确认 proper U-Boot `8f53f800da`、原 `/boot.scr`、原 `6.1.99-rk3576 #8` 与 Debian 12 登录界面。证据 [P028恢复身份](P028_RECOVERY_READBACK.json)。Agent未写板或重启，旧P026保持撤回，AMP仍C。下面“恢复尚未观测”保留为失败后早期过程。

**C. HOST_BUILD_PASS；P026 Linux-only = FAILED，旧 U-Boot 候选已撤回。** 用户串口显示候选 proper U-Boot `2314a3f`，随后 `FIT: No FIT image / No CLI available`，原 Linux 未启动。主控确认 FIT_SIGNATURE 引起 CLI、default bootcmd、legacy boot.scr 三项配置兼容问题；P027生产脚本无条件拒绝所有 check/write。用户已进入 MASKROM，正在 RAM-only LoaderToDDR 前置步骤，原 U-Boot 恢复尚未观测。子代理只在独立 Host 源码修复；没有 Agent 写板/重启/M0 操作。详见 [P028失败、根因和恢复证据](P028_LINUX_ONLY_FAILURE.md)。下面 P026 的 Host PASS 是历史编译/测试范围，不是原 Linux 启动通过或可继续使用的候选许可。

## 当前裁决：P026（2026-10-03）

**C. HOST_BUILD_PASS；D准入与部署门仍关闭。** 主控已审核一个子代理并复跑最终实际C测试。获批取证得到厂商OP-TEE command5的4B验签flag=0、原Ub完整8MiB、GPIO3_D4/D5无owner；未改boot/GPT/OTP/MMIO、未启动M0、未加载KO/重启。原件及MiniLoader另存Windows桌面恢复目录并核hash。

已形成：从现有boot文件系统显式加载FIT的项目入口（不新建amp/GPT、不自动启动）、复制前完整code/shared预留及Image/initrd/DT覆盖保护、Linux DT no-map前后检查、fresh成套独立Linux release/255modules/initrd/echo、精确objects/rollback。子代理不能仅凭Host PASS升级D，主控也不把TA flag读数外推为MCU映射/cache证据。

仍缺两组实际事实：CON16/17等价有效映射与cold reset/cache状态；新Ub文件入口和原BL31 MCU setter实际可用性。当前原Ub没有项目入口，静态BL31分支不是实际成功调用。canonical final保持null；Host proposal无物理overlap但不是运行状态。见 [P026完整结论](P026_CLOSURE_RESULT.md)、[实际只读取证](P026_BOARD_READ_EVIDENCE.md)、[Host验收](P026_HOST_VALIDATION.md)、[精确部署/回滚](P026_DEPLOYMENT_AND_ROLLBACK.md)、[当前manifest](P026_ARTIFACT_MANIFEST.json)。以下P025及更早内容保留其历史语境，旧“验签flag/原Ub备份/分区加载设计缺失”不再作为当前Host blocker。

## 此前裁决：P025（2026-10-03）

串口日志后续补充：已观测该次SPL `Verified-boot:0`及六个payload SHA检查PASS，SPL FIT未要求签名。proper U-Boot AMP阶段另经OP-TEE查flag；日志未走此路径，实际结果/key仍UNVERIFIED。日志已取得，无需重复提供或重新上电。见 [原始摘录与边界](../rk3576-amp-board-evidence/SPL_VERIFIED_BOOT_SERIAL_EVIDENCE.md)。

**C. HOST_BUILD_PASS；APPROVAL_GATE_BOARD_TEST关闭。** 本轮恢复后普通只读SSH成功：相同Kernel/U-Boot/BL31版本，Image/uEnv/DTB/boot脚本六项hash与原备份一致，重新取得保留软链接的boot原件；仍只有uboot/boot/rootfs，无amp或AMP/RPMsg DT。整机恢复实测用户确认PASS保持有效。

Host地址/cache方案冻结：未来checked SiP/cold reset设置B16=`0x47800000`、B17=`0x40000000`，ringsPA=`0x47d00000/0x47d08000`、poolPA=`0x47d10000`；全MCU cache bypass+Linux uncached pool/device barriers。实际CON/运行cache/SMC仍未证明，final合同保持null。

新增AMP FIT config-policy/required-conf-key/partition边界补丁；signature-enabled U-Boot fresh完整Host build和原BL31/OPTEE/DT保留封装PASS，**unsigned/nondeployable**。46项实际C测试+3顺序检查、26+1冷启动fault检查、9,672合同检查PASS。部署机器清单的文件身份PASS、准入检查Linux exit2/BLOCKED，含exact旧/新hash和rollback。完整结论：[P025_CLOSURE_RESULT.md](P025_CLOSURE_RESULT.md)、[加载与验签](P025_BOOT_LOAD_AND_VERIFY.md)、[部署/回滚](P025_DEPLOYMENT_AND_ROLLBACK.md)、[P025 manifest](P025_ARTIFACT_MANIFEST.json)。

剩余：真实验签policy/key、官方amp GPT加载目标和用户数据备份、复制前完整code/shared内存保护、运行SiP/mapping/cache、kernel/module升级身份和UART5实时/接线。下文均为历史，不用“尚未取得镜像/未做整机恢复”作为当前blocker。没有板端写入/上传候选/重启/M0启动。

## 用户实测补充（2026-10-03）

用户明确确认 **“整机恢复实测通过”**：`WHOLE_BOARD_RECOVERY_TEST=PASS / BOARD_OBSERVED_USER_REPORT`，无需重复恢复演练。模式、实际刷入镜像和恢复后 hash 未提供；旧备份身份不等于恢复后基线。整机恢复实测缺口关闭，AMP 专用 rollback/用户数据备份、boot 入口/加载源/验签/SMC/有效映射/coherency 仍需闭合，整体 **C. HOST_BUILD_PASS**。本次 Agent 只更新文档，无板端访问。见 [用户恢复证据](../rk3576-amp-board-evidence/RECOVERY_USER_CONFIRMATION.md)。以下保留历史记录。

## 最新补充：P024（2026-10-03）

**仍为 C. HOST_BUILD_PASS。** 官方Debian12 GNOME 20260424完整恢复镜像已校验，SHA256=`18247661a898821a751262d57f18f6edf6aab0f62b4b706cfc1b74a4ea277a66`，发布MD5与内部content MD5一致。恢复包U-Boot/BL31/Kernel/DTB/boot脚本与前轮板端原件逐字节一致；当前active uEnv仅相比factory v2模板启用CAM0。Windows桌面已准备RKDevTool3.32，微软签名Rockusb5.14预装成功(oem71.inf/exit0)，原boot副本已另存桌面。

完整恢复介质/物料缺口关闭；包内仍只有uboot/boot/rootfs，无amp。此次没有取得当前CON值、实际AMP加载入口/验证策略或动态SMC证据；实际USB恢复识别、用户数据恢复与最终changeset仍待闭合。没有板端访问/写入、启动M0或修改Host AMP artifact/contract。

详见 [RECOVERY_IMAGE_ANALYSIS.md](RECOVERY_IMAGE_ANALYSIS.md)、[RECOVERY_IMAGE_IDENTITY.json](RECOVERY_IMAGE_IDENTITY.json)、[HOST_RECOVERY_TOOLS.md](HOST_RECOVERY_TOOLS.md)、[ROLLBACK_FINAL.md](ROLLBACK_FINAL.md)。八项解析器边界测试及全Host CI（2 CTest、22 Python）PASS。以下保留P023及更早事实。

## 最新补充：P023（2026-10-02）

**仍为 C. HOST_BUILD_PASS。** 已完成冷reset + cache bypass的最小echo候选（122,696 B、0 warning）、专用Linux DMA pool fail-closed/barrier patch、明确setter的U-Boot完整Host build、Host overlay/FIT、完整候选Kernel Image/modules/v2DTB及对应echo。运行DT普通只读副本和原boot文件已复制到Host；硬件PDF确认UART5 16/18脚，DT无启用引脚冲突，实时pinmux因权限不足未确认。

唯一参数源已增加`host_proposal`，只描述拟用SiP CODE=`0x47800000`、shared base=`0x40000000`之后的布局：ringsPA=`0x47d00000/0x47d08000`、poolPA=`0x47d10000`。没有获取或假定当前CON值，没有部署、启动M0或改boot。9,672项Host合同检查、26+1项冷启动fault-injection及Host CI通过。

当前板的AMP boot入口/GPT来源、动态SiP可用性、签名策略、精确运行Image源码身份和完整离线恢复介质仍未闭合；不给D。最新交接：[HOST_PREBOARD_PACKAGE.md](HOST_PREBOARD_PACKAGE.md)、[HOST_PACKAGE_MANIFEST.json](HOST_PACKAGE_MANIFEST.json)、[RECOVERY_PACKAGE_EVIDENCE.md](RECOVERY_PACKAGE_EVIDENCE.md)。以下保留上一轮历史结果，旧135,256 B/cache-on固件及旧FIT/manifest不可与本轮候选混用。

**结论：C. HOST_BUILD_PASS；D 未达到，禁止上板。** 本轮只做 Host 与板端只读盘点。固定 RTOS/HAL reference 未改；派生 worktree 位于 `worktrees/rk3576-amp-platform/rtos`，项目分支 `agent/amp-platform-closure` 基于 `c2b2a83`。之前的 BSP 身份、M0 架构、callback 与 I2C 配置结论保持有效。

| Gate | 结果 | 证据等级 |
| --- | --- | --- |
| 0x47800000 | 是实际 FIT payload 物理装载地址；CPU3 参考 DTS 又用作 vring0，二者原样合用确实冲突。参考 DTS 注释明确 CPU3 link3、MCU link4，故它不适用于 M0 原样部署。当前板无 AMP DT，不存在已发生的运行时冲突。 | SOURCE_VERIFIED / BOARD_OBSERVED_READONLY |
| CON16/CON17 | TRM 映射公式、寄存器位置有据；当前板值、CON17 写入者和时序未证。 | SOURCE_VERIFIED；板值 UNVERIFIED |
| RPMsg | 候选 M0 link4、MBOX0 TX/MBOX4 RX、NS service、两 ring 参数源码可追；Linux 仅 CPU3 link3/MBOX3 参考，当前板无最终 M0 DT。 | SOURCE_VERIFIED；端到端 BLOCKED |
| Cache | M0 侧 RPMsg cache hook 为空，U-Boot 源含 uncache window CON14/15 写入，但当前板执行及实际 shared DDR 属性未证。 | UNVERIFIED |
| Boot | 匹配版本 U-Boot 源的 `AMP_PART` 是 GPT 分区 `amp`；当前 eMMC 无该分区，实际 U-Boot `CONFIG_AMP`、BL31 MCU SMC、FIT 验签未知。 | SOURCE_VERIFIED / BOARD_OBSERVED_READONLY / BLOCKED |
| Host | 派生 M0 echo clean SCons + unsigned FIT 结构生成；Linux echo 模块对板上实际 headers/Module.symvers 构建通过，但 exact kernel source commit 未证。均是 **非部署候选产物**。 | HOST_TESTED |
| DTS / UART / recovery | 未选最终内存地址，未生成可部署 LubanCat AMP DTB；UART5 当前 DT disabled 但 v2 物理/互斥未闭合；无完整离线恢复链。 | BLOCKED |

唯一参数源 `docs/amp/AMP_PLATFORM_CONTRACT.yaml` 用 JSON 语法的 YAML 1.2；未知最终值为 `null`，检查器 `scripts/amp/check_platform_contract.py` 返回非零。报告标题中的 “FINAL” 表示最终 Gate 裁决，不表示已有可部署最终地址。

下一步需取得 **批准的只读寄存器/bootloader 证据**、当前 U-Boot/BL31 镜像与配置、CON17 和缓存属性，再依证据设计 M0 专用 reserved-memory/ITS/DTS 并完成离线恢复。当前不得把 Host FIT、KO、参考 DTS 或本报告用于部署。`BOARD_TEST_APPROVAL_PLAN.md` 只定义未来审批输入。
