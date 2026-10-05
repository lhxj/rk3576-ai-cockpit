# Sensor RPMsg v1

2026-10-05：`C_CODEC_RTOS_SERVICE_HOST_PASS`。实际C codec、RTOS service/endpoint及原生派生接入已实现；配套Linux sensor KO、rpmsg_srv、Core/Qt尚未实现，不是已部署ABI或完整HOST_PASS。

## 包络与服务

独立NS `rk3576-sensor-v1`，remote endpoint `0x3005`；health `rk3576-m0-echo`/`0x3004`保留，使用同一个RPMsg-Lite instance。Linux local地址由原分配器分配。标准NS为40字节：name[32]、addr LE u32、flags LE u32，golden测试对照内核ABI；业务包所有多字节字段为BE，禁止裸struct序列化。

复用VAI1 magic `0x56414931` / version1的44字节公共header：0 magic u32、4 version u16、6 type u16、8 payload_size u32、12 request_id u64、20 session_id u64、28 Core/control epoch u64、36 deadline_ms u64。跨域deadline固定0，租约使用M0 monotonic；remote_boot_epoch独立保留在payload，不能混用Core epoch。最大wire256，严格精确长度/版本/类型/保留位检查；原应用64KiB上限保持。

新增显式类型HELLO `0x100`、STATUS `0x101`、SUBSCRIBE `0x102`、UNSUBSCRIBE `0x103`、SAMPLE `0x104`；复用ACK19/RESULT20/ERROR21。旧枚举数值及25未知类型负例保持。C++公共codec只验证包络，sensor消费者必须再调用typed C codec。

所有payload偏移0/2为schema u16=1 / sensor_id u16=1。

| 类型 | payload字节 | 其余payload字段（偏移:类型） |
|---|---:|---|
| HELLO |16|4:remote nonce u64，12:flags u16（1=query/nonce0；2=bind+READY/nonzero nonce），14:reserved u16=0 |
| SUBSCRIBE |28|4:remote epoch u64，12:subscription u64，20:rate u16=20，22:lease_ms u16=500..5000，24:reserved u32=0 |
| UNSUBSCRIBE |20|4:remote epoch u64，12:subscription u64 |
| ACK/RESULT/ERROR |24|4:remote epoch u64，12:subscription u64，20:service error u16，22:关联请求类型 u16 |

service error：0 OK、1 MALFORMED、2 VERSION、3 STALE_EPOCH、4 OWNER、5 STATE、6 LIMIT、7 DUPLICATE、8 HARDWARE。采样错误独立：0 OK、1 NACK、2 TIMEOUT、3 SHORT、4 WRONG_ID、5 BAD_CONFIG、6 UNAVAILABLE。当前HAL通用ERROR无法区分真实NACK与其它错误，adapter报告UNAVAILABLE。

## SAMPLE payload：72字节 / wire116字节

| 偏移 | 类型 | 字段 |
|---:|---|---|
|4|u64|remote_boot_epoch|
|12|u64|subscription_id|
|20|u64|sample_seq|
|28|u64|publish_seq|
|36|u64|M0成功读取完成时monotonic毫秒|
|44|u8|time_unit=1（ms；实际分辨率由RTOS tick确定）|
|45|u8|valid=1|
|46|u16|error=0|
|48|3×i16|Accel raw XYZ，显式二补码BE|
|54|i16|MPU芯片温度raw|
|56|3×i16|Gyro raw XYZ|
|62/63|u8/u8|accel_fs/gyro_fs=0（±2g/±250°/s）|
|64/65/66|u8/u8/u8|DLPF3/divider49/power1读回|
|67|u8|reserved0|
|68|u32|config_id=0x00010331|

错误只发布STATUS，清除pending，保留旧raw/seq/time但valid0；不发伪造零值或刷新旧样本时间。新raw timestamp只在14字节读取成功完成后取得。单位转换在Linux实现，不输出姿态。

## STATUS payload：72字节

4 remote epoch u64、12 subscription u64、20 sample_seq u64、28 publish_seq u64；36 sample_errors u32、40 latest_overwrites u32、44 send_failures u32；48 phase u8（0 waiting、1 ready、2 failed）、49 valid u8、50 sample_error u16；52/53 accel/gyro fs u8、54 DLPF u8、55 divider u8、56 power u8、57 reserved0；58 configured_internal_odr_hz u16=20、60 config_id u32；64 protocol_errors u32、68 control_queue_drops u32。

phase0/2的配置字段仅广告第一版目标，不能作硬件读回证据；phase1配置来自已验证初始化。58字段只表示配置内部ODR，另记录RTOS读取目标20Hz、RPMsg发布上限20Hz；实际采样间隔/抖动和发布成功率待板端测量，不能以目标频率冒充实测。

## 生命周期与背压

Linux下一步须以getrandom生成独立非Core nonce；M0本启动首次bind固定它，query只查询，重连不得更换。READY必须由Linux ownership/CCF preflight后发送，实际初始化在独立worker完成，回调只短临界复制。单订阅由peer/session/Core header epoch及remote epoch共同fence；8项request ID缓存，重复不续租。无订阅时可以换控制owner；已有订阅禁止抢占。

订阅续租不重置全局50ms发布节流，退订/重订也不能提前发送。latest-only样本、8项控制队列、非阻塞单次send；发送失败计数并消费pending，无无限重试。错误/恢复/租约失效dirty STATUS合并至最多1Hz。sample_seq表示成功采样；publish_seq表示发送尝试，gap须结合发送失败/覆盖统计分析。

共同owner起点900000ms（15分钟）；sensor endpoint stop先等待worker/采样退出，再注销endpoint，最后health释放共享instance。失败/超时/不安全pool地址保留instance至冷恢复，不释放仍可能使用的对象。连续三次采样失败停采样并发布实际错误状态，health仍可响应。

Linux后续必须记录本地monotonic接收时间、endpoint generation，只有对应待处理HELLO回复可接受新remote epoch；旧epoch/generation包不得恢复ONLINE。断流年龄以Linux接收时间计算，不能相减M0与Linux时钟计算单向延迟。该接收端fence与Core/Qt状态目前未实现。
