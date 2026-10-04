> **历史AMP阶段计划。** 此前计划的当时进度/阻塞保留；当前链已实板通过且用户要求冻结，不继续开发其中的后续业务建议。当前交接见 [AMP_RPMSG_INTEGRATION_TIP](../amp/AMP_RPMSG_INTEGRATION_TIP.md)，本轮仅整理既有源码与证据。

# P019：LubanCat SDK manifest 与当前启动镜像对齐

- 目标：解析用户提供的官方 manifest，定位 RK3576 通用 SDK 的固定项目版本、MCU/AMP 资料及 rkbin/BL31 来源。
- 基线：`agent/amp-platform-closure`，`3c69b3a`；用户确认 Linux 恢复，当前 CON17 未知，等级 C。
- 范围：Host 读取公开 manifest/include、版本历史和所指关键项目。下载资料只进忽略的 `artifacts/local/`；固定 RTOS/HAL reference 不改。不全量同步 SDK，不安装或执行下载的脚本/二进制，不访问板端。
- 步骤：冻结 manifest SHA → 解析 symlink/include/project revisions → 查与 2026-04-24 镜像相关的 release → 核 U-Boot/rkbin/BL31 来源与匹配限制 → 记录下一条可执行的 Host 调查路线。
- 验证：记录来源 SHA/文件 hash；XML 解析；如找到候选 BL31，静态 hash/内容与已有板端提取文件比较；`git diff --check`。
- 交接：manifest pin 不等于运行镜像逐字匹配；未匹配项目或未公开 ATF 源码如实标注。不尝试新 MMIO/SMC，不部署，等级不因 SDK 地址而升级。

## 实际结果

- 固定 manifest SHA `db55f9658b2460b40e6e873d391e86d1b29e2916`，验证 symlink/blob，选择历史 20260424 清单；当前默认 release 链已选 20260903。
- 已解析 generic/full/Buildroot include 和 remotes：相关固定项目 pin 已取得，未见 RTOS/HAL/ATF 项目；full include 的 internal remote 未定义，未尝试 repo sync。
- rkbin 的 RK3576 BL31 ELF，三个非空 PT_LOAD payload 与板端已提取文件全字节匹配；Host helper 验 Git blob、ELF header/segments，exit 0。readelf 正常，nm 报 no symbols；未执行下载固件。
- 新增 SDK_MANIFEST_EVIDENCE.md、SDK_BASELINE_IDENTITY.json 并追加 SUMMARY/BOOT_CHAIN 证据。kernel 精确运行 build match 和 CON17 仍未闭合，等级 C。没有板端操作。
- 在 THIRD_PARTY 登记固定来源、仅本地静态取证的使用范围和未核验的许可证边界；机器可读身份记录与实际验证结果一致，`git diff --check` 通过。
