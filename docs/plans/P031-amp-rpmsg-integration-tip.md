# P031 · AMP/RPMsg 最小链集成整理

2026-10-04。用户要求冻结已经实测打通的 AMP/RPMsg 最小链，整理代码、DTS/FIT/RTOS/Linux 提交和证据，消除旧 BLOCKED 与当前事实冲突，输出 AMP_RPMSG_INTEGRATION_TIP；不开发 RTOS 业务，不合 UI/Voice/Media。

## 一个里程碑，三步

1. 审查当前纯 AMP 分支相对 bootstrap main 的提交/文件范围，锁定实测 U-Boot、RTOS/HAL、Linux/KO、C DTS/FIT/script 来源和字节身份；补齐源码入口/补丁顺序索引。
2. 建立当前集成事实清单，更新 STATUS、任务/合同/组件入口和各旧审查文档的历史标记。保留当时失败与 JSON 快照，当前 BLOCKED 不能继续被解释为“最小链未运行”；未实测范围仍注明。
3. 保存 Windows 集成说明、机器索引、提交清单和证据，离线审查后 commit/push 当前 AMP 分支。最终 tip 是实际提交 SHA；不重写历史、不 merge、不开板、不访问密码/私钥、不新增测试或重建实测二进制。

## 验收与权限

本轮 L0 源码/文档整理，无 SSH/板端启动/刷写。以 P030_RPMSG_C_V1_EXECUTION 为收发与冷恢复证据。签名、DT no-map、实际 rings/DMA、M0 指针/cache 与 HELLO_ACK/PONG 分别记录。公开 vendor 基线和项目派生 commit 分开；旧 SDK 编译产物/未验证入口不混入当前链。

审核 git diff 和来源/hash/路径/提交范围属于整理校验，不运行 test suite。最终提交增量必须没有 UI/Voice/Media；Windows 副本与仓库逐字节一致。历史产物仍不可当新包自动部署；真实 min-chain BOARD_TESTED 与业务/长期稳定性分开，未知 raw CON16/17 不填成实际读取值。

进度：整理完成。已锁定50个既有纯AMP提交及配套产物，补齐累计source export/组件顺序和实际DTS/FIT/C正文输入；当前文档重整，179项历史正文/JSON/计划身份保留并索引。离线来源/hash审查和48个当前相对链接核对通过，实测二进制未重建。Windows按仓库结构保存说明/源码/证据；最终Git SHA由实际commit/push后写入Windows tip文件和最终回复，PR保持Draft不合并。
