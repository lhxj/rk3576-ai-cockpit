# P025 Host 验证记录

2026-10-03。BASE=`7bd7d9f5aefe8aa65c9b405183782b33d34aec9f`，branch=`agent/amp-platform-closure`。全部firmware/二进制只检查或生成，没有运行。

| 检查/命令 | 结果 | 范围 |
| --- | --- | --- |
| `bash scripts/board/amp_post_recovery_readonly.sh`，既有身份验证、项目flock、针对当前IP的命令行route override | SSH exit0，三份证据复制PASS | 普通只读文件，未写板；日志忽略；旧alias timeout一次后用已知Host TCP relay，没有改全局SSH/关闭hostkey验证 |
| `validate_host_proposal.py` 对实际P023 M0/DT/FIT/headers及原DT | exit0，9672检查PASS | 固件122696B，0M0warning；实际payload/address函数65536B双向和越界检查；不是本轮重新编M0 |
| `verify_con17_boot_evidence.py` 对固定U-Boot源码和旧实际Ub/BL31子镜像 | exit0 | CON17 setter静态分支/地址，不是runtime寄存器读数 |
| `test_uboot_mcu_startup_draft.py --source .../p025-uboot --cold-proposal` | exit0，26fault+1weak-hook PASS | 只执行自写Host C mock，未执行U-Boot/SiP |
| `test_uboot_fit_policy.py --source .../p025-uboot --output NEW_OUTPUT` | exit0，23×2=46条件+3顺序检查PASS | actual C提取、cc `-Wall -Wextra -Werror`；required signature/no key、bad config、partition overflow等 |
| `build_uboot_fit_policy_host.sh .../p025-uboot .../p025-uboot-policy-clean-v2 enabled` | fresh完整make exit0 | source `96c9a009eed997c318cd247943e5fc88e8e1bcc6`，AArch64 gcc11.4.0；AMP/ROCKCHIP_AMP/FIT_SIGNATURE/OPTEE_CLIENT=y |
| `package_uboot_host_fit.py --original .../uboot.payload --build .../p025-uboot-policy-clean-v2 --output .../p025-loader-package-final` | exit0 | mkimage2022.01；4MiB双slot、FIT2056192B；原ATF/OPTEE/DT及metadata完全保留，未签名 |
| `git am 0007`从P023 Ub基线的独立重放 | exit0，whole source tree一致`0e70bd3bfe6e00c50a7dd94ec96d63b308c010b6` | 二commit mailbox可复现；固定reference未改 |
| `make_deployment_packet.py` | exit0 | 核真实只读备份状态/版本/GPT/files与已知产物hash，生成manifest/changeset |
| `check_deployment_packet.py`默认 | Linux exit2，integrityPASS / deploymentBLOCKED | 合同未ready、未知partition/policy等拒绝；不是编译失败 |
| 同脚本`--allow-blocked-for-host` | exit0，integrityPASS / deployment仍BLOCKED | 仅身份校验，不能进入D |
| `test_check_platform_contract.py` | 3PASS/exit0 | 原合同mutation检查，未知值拒绝 |
| `bash scripts/dev/host_ci.sh` | exit0，2CTest、32Python、shell syntax PASS | 新增entry语义/false capability/packet hash与blocked gate regression；不连接实板 |
| `git diff --check` | PASS | patch/文档空白检查 |

## Warning 裁决

fresh U-Boot build共23个warning，完整log及hash在manifest：

- 22条 `tools/../lib/rsa/rsa-sign.c` OpenSSL3 deprecated API：**UPSTREAM_WARNING**，Host签名helper，与目标AArch64代码无关；本轮不生成合法签名，实际签名流程仍需确认key/tool compatibility。
- 1条 `tools/rockchip/bmp2gray16.c:121` `%s`非NUL terminated：**UPSTREAM_WARNING / 不执行此Host工具**，不用于本轮任何封装。若后续要运行bmp转换器必须先修，不写成普遍BENIGN。
- 新修改的loader/验签/地址代码与target link无新增warning；无callback类型、截断、implicit declaration、alignment/overlap/overflow警告。M0原clean build无warning，本轮重验BIN而非重编。

## 原件保护

固定RTOS HEAD=`7c397f41751feb29b0b388dfda3d2c2225f1f87c`；HAL HEAD=`277de3fd4b0e640654ee73bb3308be2ef01e3aad`。两仓`git status --porcelain`为空。canonical contract未修改（SHA=`216484d601025308498181dfccaf993c81d5101eb4d44d42392c3ac48e871cc7`）。旧P023/P024manifest保留历史，本轮最新产物见P025 manifest，不混用p025旧v1 U-Boot/package。

**结论：Host packet完整性PASS，BOARD readiness BLOCKED，整体C。** 本轮没有上传固件/KO、修改/boot/uEnv/GPT、MMIO/SMC访问、reboot或M0 release。当前准入失败不是恢复镜像缺失，详见P025_CLOSURE_RESULT。

## 用户串口证据补充后的检查

串口相关摘录及JSON仅记录用户观察；SPL flag=0与proper U-Boot AMP policy=null分开。重新生成P025 manifest/changeset exit0，加入该阶段证据文件hash，固件及canonical contract字节未改。4项deployment packet regression PASS，实际完整artifact hash检查`--allow-blocked-for-host` exit0；deployment gate仍BLOCKED。`git diff --check` PASS。本补充不重复编译或访问板，SPL证据不能解锁AMP的验签/加载/映射门。
