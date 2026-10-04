# 当前状态：AMP/RPMsg 最小链集成事实

2026-10-04。**AMP/RPMsg 最小链 BOARD_TESTED，READY_TO_INTEGRATE；按用户要求冻结。** 当前集成入口是 [AMP_RPMSG_INTEGRATION_TIP](amp/AMP_RPMSG_INTEGRATION_TIP.md)，精确源码/产物/证据在配套 JSON，最终 Git SHA 由交付 tip 文件给出。

## 当前已证事实

| 项目 | 当前事实与证据 |
| --- | --- |
| 板/核 | LubanCat-3 v2 / RK3576 BUS Cortex-M0，RT-Thread 4.1.1；COM6真实运行 |
| Boot | proper149b1c5、当前8MiB SHA f9beef07…72c0b3；开发公钥conf签名AMP FIT、单次amp_m0load及C Linux启动均实测 |
| 配套系统 | 独立6.1.99-rk3576-m0echo-p026，Image/modules/initrd/DT/KO同源配套；保留默认#8入口 |
| 内存/通知 | 三段no-map；实际rings47d00000/47d08000、DMA base47d10000；link4、RX MBOX0/TX MBOX4 |
| 时基 | v5条件本地LOAD239998→326，ISR/tick各+54，首次RT延时返回 |
| 双向通信 | Linux收到HELLO_ACK/PONG；M0接收27d18010/len5、27d18210/len4并PONG sent；三处cache bypass1 |
| 冷恢复 | 用户完整断电再上电，默认6.1.99-rk3576 #8/root p3/boot p2/无stage身份 |
| 源码/Git | 当前纯AMP分支agent/amp-platform-closure；相对bootstrap基线没有UI/Voice/Media代码或提交增量 |

实际命令、来源附件 hash、重复第二次insmod/File exists偏离及范围限制见 [C执行记录](reviews/rk3576-amp-platform-closure/P030_RPMSG_C_V1_EXECUTION.md) / [JSON](reviews/rk3576-amp-platform-closure/P030_RPMSG_C_V1_EXECUTION.json)。原件保持和被动安装读回见C准备记录。

## 当前范围与未知项

用户明确不开发新RTOS业务，不合UI/Voice/Media。现有最小链无需重跑C。本轮仅整理源码/配置/提交/脱敏证据，无板访问、构建、签名或新测试。

原始CON16/17寄存器值、整个512MiB窗口、长期/缓存开启稳定性、热重连/业务心跳/用户态ABI/MPU6050仍未实测，不冒充本次完成项。原厂默认内核的逐字源码匹配未知；实际链使用独立paired kernel，此旧问题已不阻塞已测最小链集成。旧 full-deployment/D schema不作为本tip的集成门，也不据短测扩大生产验收。

## 历史与其他项目

此前STATUS全文和非AMP板端/相机/音频等事实保留于 [历史状态](reviews/rk3576-amp-platform-closure/STATUS_HISTORY_20261004.md)。这些模块不在本次集成范围；保留其原证据等级，不从AMP结果推断其完成。

旧AMP Markdown已明确标注历史或转向当前tip；旧JSON保留当时快照，[历史索引](amp/AMP_RPMSG_HISTORY_INDEX.json)登记范围。失败、撤回、旧包BLOCKED仍为真实历史；不能继续当作“当前最小链尚未启动/尚未通信”的结论。当前恢复后的系统处于默认Debian，M0启动/KO由用户已完成的测试记录支持，不代表默认上电自动运行。
