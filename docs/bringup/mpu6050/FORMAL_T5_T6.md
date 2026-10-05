# 2026-10-05 MPU6050 T5/T6 attempt6：SOURCE起点超时，默认启动已确认（当前）

冻结collector-v2未修改，也未创建v3；attempt3/4在UBoot等待人工确认期间120秒startup窗口到期，均未LOAD/SOURCE/新M0。用户明确要求保留原v2并重测。attempt6实际一次SOURCE成功（2026-10-05T08:42:37.0239293Z），readonly preflight PASS，主控正常加载原health KO（PONG14/14、elapsed14505ms、timeout/error0、window720000ms）及原sensor KO。

loaded入口因传入SOURCE age>390秒触发守卫而fail-closed；入口精确age未记录。失败后主控追加诊断，于2026-10-05T09:01:09.4982628Z测得距SOURCE 1112.4766秒，该值不是入口精确age。Qt、WHO及T5/T6业务未执行，本次无新MPU业务读取；不是错误风暴或MPU硬件失败。collector95656按DIAGNOSTIC900秒正常到期，source_once=True，COM5=83197B/M0=105948B；退出前末health PONG693、timeout/error0，但phase=DONE/elapsed1132408ms/last_pong_age412459ms，不能称运行时health仍READY。

主控正常卸载两KO并确认节点释放exit0；随后纯被动collector37207记录正常shutdown及UART1291.859648 Power down，CANCEL exit0释放端口。用户随后默认冷上电；主控首次SSH No route，等待10秒后第二次SSH成功，实际uname -r确认默认6.1.99-rk3576已启动。按用户要求未重复保护hash，本次仅默认启动identity确认，不能记录35项新核验完成。主控释放marker已写、锁会话75137 exit0，全部串口已关闭。历史HOST_PASS、RTOS_SENSOR_PASS、RPMSG_SENSOR_PASS保留；UI_SENSOR_PASS、五分钟共存及最终集成仍NOT_RUN。证据artifacts/local/mpu-root-review/t5t6-attempt6-{preflight,runtime,normal-release,final-shutdown}.log及Windows ready-attempt6/attempt6-shutdown。

下一窗口仅提出主控审核的有界单入口：在SOURCE前准备好用户目录编排与日志，保持原collector-v2/identity/人工LOAD→INSPECT→SOURCE门；SOURCE后一次入口读取实际SOURCE时间、核age与冷启动身份、执行冻结readonly preflight、确认无旧模块、正常加载原health KO并有界等HELLO_ACK/PONG>=3，再正常加载原sensor KO并立即调用同SHA loaded脚本。入口需在每阶段重新核剩余时间、异常停止且保留引用供主控正常退出，不能重试/自动SOURCE/延长Linux720000ms或M0900000ms。loaded仍SOURCE age<=390、health elapsed<=200000、总480秒；Qt静止窗后方向观察需用户实际确认。此项是计划，尚未实现/执行。

---

# MPU6050真实Qt与五分钟共存：loaded下一窗口（T5/T6尚未执行）

最新用户已授权继续且不需要再次审批。具体产物仍由主控审核并唯一持共享锁操作板；本文件不将Host构建算作实板T5/T6。接线USER_CONFIRMED；用户刚重新固定模块，不能据此补写上一轮静止。冻结AMP/Application/SI_HEALTH及已部署正式包v3保持原样。当前默认6.1.99/rootp3/bootp2，首guard停止后主控冷恢复25+旧sensor十文件共35 hash/metadata PASS，该首窗口锁/双UART已关闭。下一retry主控已重新持锁78175：最新只读before25与旧十文件PASS（t5t6-retry-default-before.log、t5t6-retry-previous-v3-before2.log）；仅部署新用户目录loaded脚本并核SHA及冻结包hash读回PASS，未启动新固件/KO/应用（t5t6-retry-loaded-stage.log）。

## 精确组合与Host证据

清单：[FORMAL_T5_T6_BUILD.json](FORMAL_T5_T6_BUILD.json)。Host新包为`artifacts/local/mpu-sensor-coexistence-package-v2/packet`，11成员，独立目标`/boot/amp-p029/mpu-sensor-coexistence-v1`；原v3路径`/boot/amp-p029/mpu-sensor-v1`不覆盖。新包v1因UART预算不足被审查否决，保留历史，不能安装使用。

应用由主控实际AArch64原生编译四ELF、核hash/readelf/ldd依赖resolved；源码归档v2 SHA454342eb36a6fd1f27255aeeae163e9fd518fdc86a05df7cd3a37308a5b1647f含288成员，e1b29b81为导出基点，包含工作树增量。新目录`/home/cat/cockpit/mpu-sensor-app-v3`。构建只编译，未运行测试/应用或访问设备。M0原生v8仍MPU_SENSOR_V1服务协议，配置诊断拆成MPU_CONFIG与MPU_RATE短行，最坏格式<=126字节且newline完整；raw限100成功样本。FIT/map/签名验证通过；entry0x141，heap364348B，sensor/control/main栈2048/4096/1024B，高水位未测，owner窗口900秒不延长。传感器KO-v4、health KO-v2、owner DT和配套kernel/initrd复用已验证组合。

最新Host CI34/111/5 PASS（t5t6-resumed-host-ci.log）；此前34/97/5为历史；主控实际sampler八模式ASan/UBSan/LSan1 PASS。新collector生产factory八模式覆盖占用非零退出、cold首次/重复、SOURCE一次、deadline、错prompt与CANCEL；Windows实际.NET验证live reader可见Flush内容且第二writer拒绝。新packet exact11与篡改/缺失/附加/符号链接回归通过。以上不是板端结果。

## 窗口、停止与正常退出

双UART COM5=1500000/COM6=115200、8N1/no flow/DTR/RTS=false，每路总256KiB。两端口成功打开才READY；人工等待300秒，首次真实cold marker开始startup120秒，SOURCE一次后采集900秒，总绝对1320秒。UART只是诊断/控制，产品数据仍真实I2C→M0→独立RPMsg→Linux→VehicleCore→Qt。当前loaded controller总480秒、T5 UI90秒（进程守卫110秒）、T6真实全组件共存至少300秒；冻结child controller420秒（父进程守卫450秒）。Linux health720000ms、M0 900000ms，起点不同分别核验；SOURCE真实age<=390秒，已加载health elapsed<=200000ms。旧冻结root runner630秒只作历史且禁止再次运行。主控SOURCE后立即核身份并启动，不能耗到窗口末尾。采集期限到达不会自动恢复板；任何失败停止扩大测试、保留日志，主控执行正常停机与用户cold恢复，不自动warm reboot/重试。

启动前root fuser对CAM0 `/dev/video11`及ALSA capture `/dev/snd/pcmC0D0c`检查全部UID持有者；应用再核实际CAM0 graph。录像合计512MiB、空闲盘>=1GiB；每日志2MiB，4KiB读取/16KiB完整行，禁止分片插入health。T6共享一个MediaService/Camera/MPP实例，Qt preview/recording/RTSP+真实RKNN/Voice持续工作；capture/encode/record packets/vision inferred/audio captured计数每5秒严格增长，Qt preview最终>0，sensor序号每5秒增长且RUNTIME/VALID、协议/采样/发送错误零，health计数/年龄持续过门。RTSP loopback真实UDP decode>=8000帧且无decoder错误；录像ffprobe；CPU/RSS/PSS/MemAvailable/CmaFree/温度/线程FD在resources.jsonl。UI合并更新单独计数，不计传输丢包。

T5固定静止窗：首次有效UI观测后10秒稳定，取[10,25)秒至少10组1Hz实际换算值。每组加速度模长0.85..1.15g，各轴陀螺均值绝对值<=10°/s；这是预定义功能容差，不称精度校准/零偏校准，不减去上一轮X偏置。窗结束输出SENSOR_STATIC_WINDOW与T5_DIRECTION_CHANGE_ALLOWED后，主控才提示用户轻轻改变固定模块方向，确认六轴数值/芯片温度/状态/年龄真实变化；不移动整板/MIPI/风扇。自动记录永远保留USER_CONFIRMATION_PENDING，人工确认独立登记。

M0错误发布STATUS、连续三次停止；Host controller看到错误状态/health失败立即停止推进，缺样本由有界期限失败，不声称每个I2C错误都立即停M0。T6结束先停止媒体消费者，再runtime.stop内部UNSUB/有界worker join，最后Core/Qt应用退出；只有matching UNSUB RESULT才确认退订，不能称退订先于媒体停止。退出后health继续真实PONG，正常卸载sensor/health KO，确认node/模块/媒体设备释放。M0在无订阅状态不发SAMPLE，采样仍可继续至其900秒窗口；正常关机后用户冷恢复，不做额外故障注入。

## I2C与codec依据

固定kernel源码commit521833e2d28decbd6473d5717f1f96cc4108e208：`artifacts/local/p023-kernel-source/kernel-521833e2d28decbd6473d5717f1f96cc4108e208`位于AMP worktree。drivers/soc/rockchip/rockchip_amp.c SHA91e9d19e0517527b991fe7f1f0dec2665cb65c7b136c0b1795fc2c3f300e05ad，L671..677 probe bulk_get_all/prepare_enable永久保留DT七clock引用，L734仅remove释放，driver无runtime PM回调。sensor KO卸载仅释放其额外两clock和rate-exclusive引用，不移除rockchip-amp基础引用；窗口不unbind AMP、不系统suspend。preflight核DT七clock字节/owner绑定，KO owner-ready核两clock enabled/24MHz；没有读取debugfs实际reference count，不能这样声称。上轮卸载后M0 seq3401/error0支持该生命周期，但不是唯一依据。

同kernel arch/arm64/boot/dts/rockchip/rk3576-lubancat-3.dts L146..161 soundcard rockchip-es8388引用SAI1和es8388；L391..400 I2C3 codec@11 compatible包含everest,es8388及everest,es8323。sound/soc/codecs/es8323.c L873 regmap_i2c与L880 probe recv可产生真实事务错误；卡名ES8388与驱动ES8323不是两块codec。owner DT保留I2C3/CAM0节点原样。默认启动也有ES8323 3-0011 -6，7条与上轮12条覆盖范围不同，不能归因新sensor或直接比总数。T6启动进程前记录kernel时间cursor及有限原有错误；同一运行窗任何新ES8323 -6即停止、留证。仅ALSA枚举不能通过音频验收；还需Voice frames持续增长/XRUN/overflow零等实际门。

## 主控执行与恢复

1. 审核exact清单/installer pin/启动cmd文件hash及当前25+旧十保护记录；被动安装只新目录，读回全部11文件，不自动启动。
2. 使用新包external collector（hash在清单），确认双UART READY后持锁正常shutdown；用户拔主电等10秒后上电。检查真实SPL版本8f53f800da-241224、checked Uboot image hash前缀7d8fe670、controlDT43164981、Uboot149b1c5、cold/policy0。
3. 向ControlFile依次写LOAD/INSPECT；确认load3225字节/filesize=c99/fileaddr4c000000及日志身份后写SOURCE一次。stage脚本逐一核Image/initrd/DT/FIT大小，原load/entry/mailbox/CON16/17不变。任何loader异常冷恢复，不盲retry。
4. 原冻结boot runner因实板psmisc不接受fuser `--`而guard停止，不能再次用它执行测试。下一组合先由主控执行固定包readonly preflight，核两KO尚未加载，再定向加载相同health KO并有界等待HELLO_ACK/PONG>=3、timeout/error=0，再加载同sensor KO；不重载、无强卸载。独立Host已审源码`run_sensor_t5_t6_loaded.py`复制到新用户目录（不写原11文件），核SHA2b957e2cf88360ee694f780275fa0c3fc81a6d98d276be73a8a95b484d9240ba，root执行`python3 /home/cat/cockpit/mpu-sensor-runtime-loaded-v2/run_sensor_t5_t6_loaded.py --source-age-seconds FRESH_ROOT_MEASURED_AGE`。SOURCE实际age<=390秒、已加载health PONG>=3/elapsed<=200秒、总480秒；live GNU build-id、56字节ABI1/owner ioctl与相同应用hash核验通过才启动UI。时间不足直接停止扩大、正常冷恢复，不延长M0900秒或绕过child300秒门。其root fuser已实测不带`--`时rc1且stdout/stderr空为FREE，仍拒绝usage/持有者。25秒静止窗完成后人工方向确认，记录真实观察；保存应用/RTSP/resources/M0日志。
5. 正常模块退出/媒体释放/health后续响应确认后，仍持锁正常shutdown，COM5 Power down后CANCEL释放串口。用户主电冷恢复默认，不由warm reboot替代。默认恢复生产reader与validator：`bash scripts/board/mpu6050/read_resource_boot_baseline.sh`保存after；`python3 scripts/board/mpu6050/verify_resource_cold_recovery.py artifacts/local/mpu-root-review/t5t6-default-before.log AFTER.log`核默认rootp3/boot2、无项目/RPMsg模块及25bytes/hash/resolved/symlink。同时root执行`python3 scripts/board/mpu6050/read_sensor_protected_v3.py --read-only-protected-v3`保存AFTER-V3.log；Host `python3 scripts/board/mpu6050/verify_sensor_protected_v3.py artifacts/local/mpu-root-review/t5t6-previous-v3-before.log AFTER-V3.log`核十hash/metadata。恢复确认才释放锁。

当前T5/T6 NOT_RUN、UI_SENSOR_PASS NOT_RUN、最终等级NOT_RUN。凡尚无板端计数（独立I2C NACK/timeout分类、全窗interval完整分布、stack高水位等）明确UNKNOWN，不能用Host Fake或目标20Hz代填实测。

实际准备记录：主控已被动安装v2并核11hash PASS，双UART READY后正常shutdown/Power down，该历史记录时等待用户cold动作；后续首guard停止及默认恢复见下文，T5/T6仍未执行。证据t5t6-passive-install.log，源构建manifest的board_deployed=false未改写。

事务统计边界：实际native-v8的mpu6050 rd/wr每次一次IO，RTThread i2c_core.c master_xfer调用一次，派生drv_i2c.c只处理组合消息/等待IRQ，无内部retry；HAL I2C Transfer单次Start，NAK未IGNORE时Stop(HAL_NODEV)，adapter仅WR/RD不启用IGNORE。因此aggregate sample_errors=0不会隐藏内部失败后重试成功；但未独立导出NAK/timeout计数，初始化逐笔错误分类/物理未上报异常不可从该计数推导。实际HAL_NODEV及其他非成功结果经adapter泛化UNAVAILABLE，不伪造具体NACK原因。

准备历史：首collector44583在人工准备阶段300秒自然结束，SOURCE=false/COM5=4223B/COM6=2B，仅Power down，无新固件/MPU访问；不是固件失败/额外M0重启。用户准备后主控同hash新collector95611 READY启动attempt2，复用原窗口锁，待真实cold日志。

## 首个正式T5/T6窗口guard停止（不算MPU硬件失败）

主控记录original runner约4秒在Qt/WHO之前退出：设备节点检查后psmisc实际`fuser -- /dev/video11 /dev/snd/pcmC0D0c`返回1、stdout空、stderr中文usage“未指定进程”，触发fail-closed。定向兼容验证`fuser /dev/video11 /dev/snd/pcmC0D0c`实际rc1/stdout/stderr空，确认FREE。证据t5t6-runtime-live.log与t5t6-fuser-compatible-diagnostic.log。两KO正常保留clock直到主控退出；修正controller第一版虽已准备，但SOURCE/health起点预算已错过，未执行，不延长原窗口。主控随后health407→409、timeout/error0，正常rmmod两KO/nodeFREE，shutdown255且实际Power down，CANCEL关闭collector。用户随后冷上电默认Debian；主控生产validator已确认默认/冻结/SI 25及旧sensor-v3十文件共35 hash/metadata PASS（t5t6-guard-stop-default-after.log、t5t6-guard-stop-previous-v3-after.log），锁及双UART关闭。未Qt/WHO/T6，不伪造Sensor或人工PASS。冻结11文件/四ELF/FIT不改；上文独立loaded-controller只改必要用户态兼容与剩余时限，live GNU-note/ABI须下一真实窗口确认。
