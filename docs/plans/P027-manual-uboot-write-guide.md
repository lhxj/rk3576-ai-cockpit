> **历史AMP阶段计划。** 此前计划的当时进度/阻塞保留；当前链已实板通过且用户要求冻结，不继续开发其中的后续业务建议。当前交接见 [AMP_RPMSG_INTEGRATION_TIP](../amp/AMP_RPMSG_INTEGRATION_TIP.md)，本轮仅整理既有源码与证据。

# P027：候选 U-Boot 的手工操作指导

基线 `agent/amp-platform-closure / 6651410`。用户要求写入步骤和文件路径；本轮只形成指导及 Host 检查，不将询问步骤解释成代理执行写板授权。

## 范围

1. 核 P026 候选4MiB、原8MiB备份和精确分区身份，准备 fail-closed 手工脚本。
2. 在 Host 普通文件模拟块设备，仅在测试副本替换设备环境；验证拒绝错误输入、写入长度、尾部保留和读回错误处理。生产脚本不开放测试设备覆盖。
3. 独立审查，提供 WSL 上传、板端 check/write、串口验收步骤；Host test→review→commit→push→现有 Draft PR。不修改 P026 artifact/合同、fixed reference、实板。

## 操作权限与验收

Host脚本与文档为L0。脚本上传为L2，手工 `--write` 是L3 bootloader改写；需要用户选择明确执行，本轮Agent不SSH、不上传、不写板、不关机或启动M0。

候选SHA `4b6e615beba307d95beff1cbbff76b06bfe794f9a2f269599d2ccd5026ca4f1f`；原8MiB SHA `ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`。新4MiB+原零尾4MiB的整分区预期SHA `2f6d9a427bbde0d658fbd55e9617a8d119f4d9e442f400ca126a21a095c08eea`，不是候选文件SHA。

脚本默认无写入动作，须明确check/write参数；限定真实块设备/label/start/size、原Linux boot对象和原分区SHA；write不触及GPT/boot/rootfs、不会重启或调用M0。写或读回异常停止，不自动重试/回滚/重启。已有Windows原raw8MiB及用户整机恢复PASS是恢复来源。

整体仍C；本轮不填D门或运行证据。Linux-only首次验证需另行获批实际执行。

## 实际结果

只在Host准备了脚本、14项普通文件模拟测试和手工指南。真实4MiB候选、8MiB原件、新整分区预期SHA已核，脚本硬编码吻合；复制镜像/脚本到Windows桌面后再次核hash。独立子代理审查PASS，主控实际运行14项PASS，无skip；既有Host CI exit0（2 CTest+40 Python+shell语法）。脚本默认无动作，check/write必须显式选择；不会触发reboot/M0，失败时停止不自动重试。未SSH/上传/写板，C不变。具体对象和命令见P027_MANUAL_UBOOT_WRITE_GUIDE.md。
