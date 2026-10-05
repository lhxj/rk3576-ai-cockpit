# MPU Sensor service Host子里程碑

2026-10-05。`C_CODEC_RTOS_SERVICE_HOST_PASS`，**NOT_DEPLOYABLE**。用户断电接线已确认；本轮没有访问实板/MPU、加载KO或部署。完整HOST_PASS、RTOS_SENSOR_PASS、RPMSG_SENSOR_PASS、UI_SENSOR_PASS及最终集成等级均NOT_RUN。

实际实现为公共44字节BE codec、独立sensor endpoint、单订阅/租约/latest背压、独立READY worker、短临界样本交接和原health同instance有界生命周期。冻结health源码/协议不改；派生health有必要入口与双owner退出hook，其原/派生hash分别登记。0004仅将RPMsg-Lite endpoint链表增删置于单核IRQ临界区，分配/释放/等待仍在临界区外；0005将sensor正常退出置于原transport deinit之前。native rpmsg_lite.o实际含IRQ保护符号，不依赖fixture推断。

验证：实际C codec/service、C与原C++ message.cpp wire roundtrip、实际endpoint七种factory/rollback/timeout/pool失败模式通过-Wall/-Wextra/-Werror及ASan/UBSan/no-pie/leak检查。覆盖golden/trunc/超长/版本/类型/reserved/config、epoch固定/owner/duplicate/续租/50ms限速/错误清pending/恢复/退订/租约/背压/900秒停止。初始化WHO/config/setup失败通过实际sensor_task fixture回归，保留具体错误元数据。主控已针对最终生产source独立复核service、endpoint七模式、task100/1000Hz与C/C++wire兼容，ASan/UBSan+Werror+detect_leaks均通过；最终v6的统一起点/初始化错误状态增量包含在复核范围。host_ci31/31 CTest、77/77 Python、5/5撤回通过，原日志保留artifacts/local/mpu-sensor-service-native-v6/host-ci.log。

原生构建：`python3 scripts/dev/build_mpu_sensor_service.py --output artifacts/local/mpu-sensor-service-native-v6`，内部固定源身份/五补丁无fuzz/SCons --useconfig/显式RTT_ROOT与gcc，原linker unchanged、共享布局/load/entry unchanged；原信任链FIT验签通过。清单见[SERVICE_HOST_BUILD.json](SERVICE_HOST_BUILD.json)。ELF entry0x141；text135236/data2672/bss525568（BSS包含固定预分配区，不能作为实际heap使用）；bss_end0x26ab4，heap364876B、主stack1024B、sampler2048B、worker4096B、control队列8；实际stack/heap high-water尚未测量。image137912B、FIT143360B，均低于冻结loader限制。生成产物与最终生产source逐hash匹配，SDK/固件/私钥均不进Git。

下一独立里程碑：配套Linux sensor绑定KO及有界read/poll/owner-hold接口→rpmsg_srv epoch/freshness adapter→VehicleCore canonical SensorState→Qt显示与退出。尚无Linux sensor driver、真实握手/NS/多endpoint/I2C事务/IRQ/样本及UI证据，当前FIT不可单独安装运行，不制作可启动sensor包。协议精确布局/频率边界见[PROTOCOL_V1.md](PROTOCOL_V1.md)。
