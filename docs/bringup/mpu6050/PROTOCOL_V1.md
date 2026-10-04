# Sensor RPMsg v1 — 协议草案

2026-10-05。**DRAFT_NOT_IMPLEMENTED**。后续codec与golden bytes须实现后验证。
NS候选`rk3576-sensor-v1`；health仍`rk3576-m0-echo`，共享同一RPMsg-Lite instance。
health remote地址0x3004保留，sensor候选0x3005；Linux local地址由现有RPMsg
分配器分配，不假定固定值。source/destination与NS匹配要验证，多endpoint上限待核。

## 公共包络与范围

复用`libs/protocol` VAI1 magic0x56414931、version1、**big-endian**44字节header。
偏移0魔数u32、4版本u16、6类型u16、8payload_size u32、12request_id u64、
20session_id u64、28boot_epoch u64、36deadline_ms u64，类型增量不改变原枚举数值。
SENSOR_HELLO/STATUS/SUBSCRIBE/UNSUBSCRIBE/SAMPLE拟放显式0x100–0x104范围；
复用现有ACK/RESULT/ERROR并使用显式稳定error编码，不直接序列化C++ enum/struct。
旧is_known_type按连续上限检查，后续必须显式兼容扩展并保留旧负例，不能放宽任意类型。

sensor service最大wire候选256B（低于冻结496B RPMsg payload），最大payload212B；
旧Linux应用64KiB上限不改变，跨域service独立执行更小上限。长度必须全匹配，
不能零填充截断包/忽略尾部。C端与C++端同一字段定义/逐字段codec，共享golden bytes。

header boot_epoch继续表示经握手传来的Linux控制上下文epoch，session_id为客户端
会话，request_id为请求/关联ID；不得将这三者复用为remote epoch或subscription。
header Unix deadline语义适用于原Linux同主机，M0没有同步Unix时钟，因此sensor
跨域header deadline_ms=0，租约使用payload lease_ms与M0自己的monotonic计时。
跨域不调用带Unix now的原RequestFence来判断M0有效期；由专用transport fence处理。
remote_boot_epoch是payload独立字段。其可靠启动唯一性来源尚待BSP审查，
在该来源闭合前不得宣称重启旧包隔离已实现。

## SAMPLE payload候选固定布局

| payload偏移 | 类型 | 字段 |
|---:|---|---|
| 0 | u16 | sensor_schema_version=1 |
| 2 | u16 | sensor_id=1（MPU6050） |
| 4 | u64 | remote_boot_epoch |
| 12 | u64 | subscription_id |
| 20 | u64 | sample_seq |
| 28 | u64 | publish_seq |
| 36 | u64 | m0_monotonic_time |
| 44 | u8 | time_unit（明确枚举，例如us；实际时基分辨率另外报告） |
| 45 | u8 | valid_flags |
| 46 | u16 | error_code（0 OK；1 NACK；2 TIMEOUT；3 SHORT_READ；4 WRONG_ID；5 BAD_CONFIG；6 UNAVAILABLE） |
| 48 | 3×i16 | accel raw XYZ |
| 54 | i16 | chip_temp raw |
| 56 | 3×i16 | gyro raw XYZ |
| 62 | u8 | accel_fs_sel |
| 63 | u8 | gyro_fs_sel |
| 64 | u8 | dlpf_cfg |
| 65 | u8 | smplrt_div |
| 66 | u8 | pwr_mgmt_1读回 |
| 67 | u8 | reserved=0 |
| 68 | u32 | config_id |

总payload72B / wire116B。错误状态通过STATUS发布，可携带最后样本年龄/计数，
不能让旧sample_seq/raw/time被“采样错误SAMPLE”刷新。所有bit/enum/error都要有
明确允许集合/保留位检查，int16按二补码显式编解码。

HELLO/STATUS携带能力、boot epoch、实际ODR/read/publish rate、配置读回、
采样错误/覆盖/发送失败计数与资源就绪；SUBSCRIBE携带请求rate、lease_ms、
新subscription_id；UNSUBSCRIBE携带匹配id；RESULT确认实际接受rate/lease。
第一版单订阅者，20Hz发布上限且不高于实际成功采样能力，候选lease5000ms、
续租间隔≤2000ms、限制最小/最大lease与请求ID缓存容量。正式范围/重订幂等
规则将在实现前固定并覆盖测试，当前只是最小计划，不能当已部署ABI。

## 背压与恢复规则

样本缓冲latest-only，control/status有独立固定容量；trysend失败不无限重试。
sample_seq只在成功新采样增加；publish_seq在发布尝试增加并记录发送失败，
主动降采样用统计标注；接收gap必须结合这些计数分析，不能全部叫传输丢包。
未订阅/lease到期停止SAMPLE，采样任务可保持有限20Hz，health仍可响应。
释放客户端时停止续租、尽力退订、线程唤醒join；退订ACK丢失时以远端lease到期
作最终停止边界，并记录未确认，不能假称已退订成功。

Linux额外记录local monotonic_rx、endpoint generation、subscription状态、
duplicate/gap/protocol error/drop/age。endpoint失联/new remote epoch立即使旧数据
STALE/OFFLINE并废弃旧id；新HELLO→SUBSCRIBE→有效样本才ONLINE。
仅新endpoint generation里通过握手确认的remote epoch可信，旧epoch重放不能
将离线状态恢复。不得用任意SAMPLE自动切换epoch或把时间相减算单向延迟。
