# P021：BUS M0 启动补丁草案与 Host 验证

- 目标：在固定 U-Boot 的独立派生 worktree 修复 MCU SMC 返回值及 standalone 错误传播；定义必须显式提供的 CON17 window-base 参数入口，缺参数时禁止 release。不填未知物理地址。
- 基线：主项目 `agent/amp-platform-closure` / `1d5cef9`；U-Boot `8f53f800da2c25d0c6ba414fb45902a01675703a`。实际构建 config、CON17 运行值、cache/物理 reservation/恢复均未闭合，等级 C。
- 权限与范围：L0 Host；派生 U-Boot worktree 仅放主项目忽略目录。原 pinned U-Boot、固定 HAL/RTOS 不改；无板端访问、MMIO/SMC、部署或 M0 启动，不改 linker/ITS/DTS。
- 文件范围：本计划、补丁/metadata、Host 故障注入验证、board-evidence 新启动草案及 SUMMARY/THIRD_PARTY；不 vendor 整个 SDK。
- 步骤：保护 Git 状态/创建派生 worktree → 最小 hook/参数验证与失败传播 → 针对真实修改函数进行 Host C mock 故障注入 → 有条件做 U-Boot config/对象编译 → 生成可审查 patch、hash、状态和限制。
- 验证：缺参数/长度不对/零/不对齐/SMC CODE 失败/shared 失败均不能 gate/reset release；正常模拟路径顺序 CODE→shared→gate→reset；上层失败必须返回。编译测试只运行自写 Host harness，固件不执行。
- 构建范围：标准 make/config/交叉对象编译前审相关构建入口；使用已有标准工具，不安装软件。失败先定位基线/依赖，不随机修厂商代码。Host mock/对象成功不等于当前板 bootloader ready。
- 恢复与停止：没有板端变更；保留原源码与历史证据。未确定 shared PA/cache/reservation 时不生成可部署地址/启动镜像，不升级 D。

## 实际结果

- 派生 U-Boot `agent/rk3576-mcu-startup-draft` 提交 `7daeb0fc8ad0818a833b162405a35d0511d767bb`；四文件补丁已保存主仓 `patches/rk3576-amp-platform/`，SHA256 `2129f84351f07c03f6f4c87d3b3258cfaa37bd5f92b335f9fe73d7353f2b1ecb`。
- Actual C function Host mock：23 项 startup/错误传播检查，加 1 项 weak hook 兼容检查，exit 0。严格编译发现原非 standalone 分支 u8 sentinel 恒假；测试明确排除该既有 warning，不称为全工程 Wextra 无 warning。
- 全新输出目录 `artifacts/local/uboot-mcu-startup-final-check/`：defconfig、AMP fragment merge、olddefconfig、三个 AArch64 对象编译、readelf 全部 exit 0；标准对象编译日志无 warning。工具链 GCC 11.4.0，与板端 GCC 10.3.1 不同；没有 full link/bootable image。
- 原 pinned tree `git apply --check` exit 0，仅检查不应用。主项目 `bash scripts/dev/host_ci.sh` exit 0：2 CTest + 6 Python tests 通过。
- 审查记录及可复核 metadata：`docs/reviews/rk3576-amp-board-evidence/UBOOT_MCU_STARTUP_DRAFT.md`、`UBOOT_MCU_STARTUP_DRAFT_RESULT.json`。
- 未接触开发板，无 MMIO/SMC；linker/ITS/DTS/contract 地址不变。shared DDR/cache/reservation、实际 boot config/load source 和 recovery 仍阻塞，保持 **C / DRAFT_NOT_DEPLOYABLE**。
- `.gitattributes` 仅将本目录 format-patch 作为带空格前缀/邮件 footer 的 diff 数据处理，避免主仓将合法 context 判为源码缩进错误；派生 C 的 `git diff --check` 和 pinned tree `git apply --check` 单独通过。
