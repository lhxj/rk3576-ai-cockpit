> **历史AMP阶段计划。** 此前计划的当时进度/阻塞保留；当前链已实板通过且用户要求冻结，不继续开发其中的后续业务建议。当前交接见 [AMP_RPMSG_INTEGRATION_TIP](../amp/AMP_RPMSG_INTEGRATION_TIP.md)，本轮仅整理既有源码与证据。

# P024：恢复镜像与启动链静态证据

- 目标：核用户提供的完整Debian12 GNOME 20260424 update.img，识别固件容器、loader/分区/boot文件，比较当前板原件并补齐可执行恢复方案；仍不自动部署AMP。
- 基线：`agent/amp-platform-closure / 080dda0`，Host包已通过；固定reference不改。
- 输入：用户桌面完整`.img`；本轮授权为Host读取/解析/提取到忽略目录。禁止运行镜像内代码或未知厂商工具，不连接/写板、不重启、不进入MR/REC、不写GPT/启动M0。
- 范围：自写只读容器解析/校验与Host测试、忽略的提取payload及hash、恢复/boot证据文档；原镜像不改。仅提取必要启动组件，不导出用户数据。
- 用户追加授权：2026-10-03 明确要求下载并安装合适恢复工具到桌面。允许Host准备RKDevTool/DriverAssistant及安装签名Rockusb驱动；不授权进入板端MR/REC或刷写。驱动通过Windows原生PnPUtil预装，不卸载现有驱动、不关闭签名/执行策略、不请求重启。
- 步骤：stream hash并核官方发布→按固定厂商格式/边界解析→提取loader/uboot/boot等比较现有Host备份→签名/AMP加载入口取证→明确恢复路径和剩余gate→Host CI/review/commit/push更新既有Draft PR。
- 验收：原镜像hash/完整性、容器边界、每个启动payload hash、GPT/parameter含义、与当前U-Boot/BL31/Image/DTB/uEnv比对、具体PC工具/按钮/原件恢复路径。静态身份、运行权限和实板恢复能力分别标证据级别。
- 停止：相同blocker最多2次有依据尝试；解析异常保留原件并报告，不猜offset、不运行固件。缺实际USB识别/签名策略等输入不升级D。

## 执行结果

- 完整镜像5,793,882,755 B，发布MD5与内部content MD5匹配，SHA256=`18247661a898821a751262d57f18f6edf6aab0f62b4b706cfc1b74a4ea277a66`。自写bounded parser提取五个普通长度启动项，拒绝未确认的rootfs64位扩展；原镜像未修改。
- dumpimage六个FIT子镜像与前轮板端提取文件逐字节一致；4MiBuboot包等于8MiB分区前4MiB。debugfs只读提取Image/DTB/boot/config/System.map/extlinux与前轮原件全部一致；factory v2uEnv与当前active差异只有CAM0启用。正式来源记录于P024报告/manifest。
- 恢复工具从Radxa官方HTTPS下载到桌面，RKDevTool3.32版本核实；Rockusb5.14 CAT Valid，用户UAC确认后PnPUtil预装exit0，枚举oem71.inf，无卸载/关闭签名/重启。原boot和uboot分区副本另存桌面并核hash。
- `python3 -m unittest discover -s tests/python -p test_amp_recovery_image.py -v`：8项PASS；`bash scripts/dev/host_ci.sh`：2 CTest+22 Python PASS，exit0；本轮不重复P023固件构建。无板端访问/写入；fixed references及AMP artifacts/contract不改。
- 剩余：USB/MR实际识别与用户数据备份，AMP实际loader/加载来源、动态SMC/当前有效映射、验签策略与最终changeset仍需证据；不以文件核验升级D。整体 **C. HOST_BUILD_PASS**。
- Git：审查/commit/push同一AMP分支、更新既有Draft PR，不merge main；提交身份以git log和PR为准。
