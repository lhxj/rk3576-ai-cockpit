> **历史AMP阶段计划。** 此前计划的当时进度/阻塞保留；当前链已实板通过且用户要求冻结，不继续开发其中的后续业务建议。当前交接见 [AMP_RPMSG_INTEGRATION_TIP](../amp/AMP_RPMSG_INTEGRATION_TIP.md)，本轮仅整理既有源码与证据。

# P025：映射、coherency、加载与部署回滚闭合

- 用户目标：继续完成 M0 地址映射/缓存一致性、AMP 加载入口/验签，以及最终部署/回滚清单。
- 基线：`agent/amp-platform-closure / 7bd7d9f`；整机恢复实测已由用户确认通过，P023 Host proposal 已构建。固定 RTOS/HAL reference 不改。
- 范围/权限：Host 源码和已有固件分析、普通文件只读 SSH 与向 Host 复制证据、合同/changeset 检查、Host 构建。禁止本轮部署、启动 M0、写 GPT/boot/固件、调用配置 SMC、重试直接 MMIO或重启；不得将拟配置值记作实机当前值。
- 三项任务：①核恢复后启动基线，并复核显式 setter/reset/cache bypass 的完整条件；②追实际固件和固定源码的 AMP 加载入口、FIT验签/boot policy；③生成带旧/新 hash、目标路径、执行顺序与恢复动作的可审查 changeset，未知项拒绝开放审批门。
- 验收：命令/返回码/输入输出 hash 可追溯；无重叠 Host layout、双侧 barrier/cache 属性有来源；官方 loader 路径与新增项目路径分开；manifest 与部署清单自动校验；Host CI/review/commit/push 更新现有 Draft PR，不 merge。
- 停止：同一阻塞最多两次有依据尝试；SSH/权限拒绝不绕过、不索取密码。仅源码不能证明的运行映射、SMC和验签保持 BLOCKED；优先交付能独立完成的 Host 工作，再提出具体最小证据/审批请求。
- 恢复：本轮无板端写入；保留恢复前备份和用户恢复 PASS。任何未来改动必须先匹配恢复后的板端版本/hash，使用已核完整恢复包和原件备份。

## 执行记录

本轮Host/普通只读取证完成；实体板准入BLOCKED，整体C。

1. 旧alias timeout一次；针对用户已知地址的Host TCP relay保持原SSH公钥/knownhost校验，普通SSH成功。重新核恢复后的版本/DT/GPT/六项boot hash并复制完整选定原件及symlink，raw firmware/MMIO未读。原件SHA与旧boot一致。
2. 9672项ELF/FIT/DT/header一致性和65536B PA/M0边界测试重跑PASS；cold reset/bypass+Linux uncached proposal保持，不补写当前CON值。检查器修正M0 local entry与physical load分离，并拒绝false/textual capability。发现U-Boot复制前RAM保护不足，单独标BLOCKED。
3. 实际vendor AMP loader只找GPT amp，当前不存在。派生新增FIT policy/required-conf-key/partition边界fail-closed补丁，46+3 Host检查PASS、完整fresh signature-enabled U-Boot build exit0、保留原ATF/OPTEE/DT的unsigned双slot FIT封装PASS；二commit patch重放whole tree一致。
4. 机器manifest/精确changeset与有限symlink rollback、完整MR恢复步骤形成；恢复后原件齐全，整机恢复用户PASS保持。缺实际policy、合法key/GPT数据方案、runtime setter/cache等不放行。默认checker Linux exit2/BLOCKED，Host身份检查PASS。
5. 全Host CI exit0（2CTest/32Python），原contract mutation 3PASS，reference两仓SHA/clean保持，diff check通过。报告P025_CLOSURE_RESULT、P025_BOOT_LOAD_AND_VERIFY、P025_DEPLOYMENT_AND_ROLLBACK、P025_HOST_VALIDATION及JSON清单。

6. 用户已补完整DDR→Linux串口日志。只保存脱敏相关摘录：SPL Verified-boot=0及六个SHA检查PASS；proper U-Boot另经OP-TEE flag路径，日志未触发，不据SPL值或security分区提示推定AMP策略。机器证据保持阶段区分，无新的板端访问。

没有board writes/boot changes/reboot/M0启动，APPROVAL_GATE_BOARD_TEST关闭。完整串口日志已取得；下一缺口为proper U-Boot实际策略/返回值或厂商等价说明，不再重复请求启动日志。任务分支按review→commit→push更新既有Draft PR5，不merge main。
