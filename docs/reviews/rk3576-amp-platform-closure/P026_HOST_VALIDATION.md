# P026 Host 验收

2026-10-03，主项目基线dba800e，branch `agent/amp-platform-closure`。最终派生U-Boot `2314a3f9f5795b88c7c53a805e9d59d82c6715b3`，parent96c9a009，vendor8f53f800；0008 patch SHA=`075c553f0812bbb2cfeb01cb6c7a3b1489bbfaf18e3a7452441903ee08ba2773`。派生源clean，fixed RTOS/HAL reference未动。机器结果 [P026_HOST_VALIDATION.json](P026_HOST_VALIDATION.json)。下面全部 HOST_TESTED，固件调用有明确fault stubs，不是实际SMC/MMIO。

## 主控复核

| 实际命令 | exit/result |
| --- | --- |
| `python3 scripts/amp/p026_test_preload.py --source artifacts/local/p026-uboot-preload --fit artifacts/local/p026-fit-clean-v2/amp-host.itb --linux-fdt artifacts/local/p023-dt-fit-clean-v3/amp-host.dtb --output artifacts/local/p026-primary-reviewed-preload` | 0；strict reserve20、preflight/order/file113、initrd/FDT16、real libfdt54、实际Linux FDT早期预检PASS；9顺序检查 |
| `python3 scripts/amp/test_uboot_fit_policy.py --source artifacts/local/p026-uboot-preload --output artifacts/local/p026-primary-reviewed-policy` | 0；两种signature编译分支共46 case，3顺序检查；要求签名却无required conf key时拒绝 |
| `python3 scripts/amp/p026_generate_preload_contract.py --contract docs/amp/AMP_PLATFORM_CONTRACT.yaml --output artifacts/local/p026-host-inputs-final --uboot-source artifacts/local/p026-uboot-preload --check` | 0；canonical SHA/compiled header/ITS一致 |
| `python3 -m unittest discover -s tests/python -p test_amp_p026_contract.py -v` | 0；3项包含改base传播、冲突拒绝、完整保留尺寸 |
| `python3 scripts/amp/p026_finalize_kernel.py`；`p026_pair_initrd.py`；`sha256sum -c hashes.sha256` | 0；255模块、孤立release、无Host source/build链接、6对象hash通过；gzip/newc解包校验PASS |
| `python3 scripts/amp/p026_make_packet.py --uboot-source artifacts/local/p026-uboot-preload --uboot-build artifacts/local/p026-preload-clean-final-v6 --uboot-package artifacts/local/p026-loader-package-final-v6` | 0；源码版本/header、package nodtb hash、原firmware保留、payload/contract身份全部约束 |
| `python3 scripts/amp/check_deployment_packet.py --allow-blocked-for-host` | 0仅表示Host integrity PASS；D/部署仍BLOCKED。严格不加参数返回2 |

实际stock/Host DT在arch_fixup_fdt前可以没有`/memory`，所以早期gd DRAM + no-map检查PASS；final-memory-check-before-runtime-fixup=-22是预期拒绝。Host真实libfdt已覆盖填bank后的正例和错误bank反例；实际U-Boot arch_fixup/booti尚未运行，不能把这个结果写成实板通过。

## 完整构建与 warnings

- Fresh U-Boot：`build_uboot_fit_policy_host.sh ... artifacts/local/p026-preload-clean-final-v6 enabled` exit0，GCC11.4/binutils2.38，signature支持保留。`nm`实际编入file command、strict reserve、boot LMB、initrd、FDT及Linux-DT检查。目标代码/linker零warning；23条Host工具warning=22 OpenSSL3 RSA deprecated +1 bmp2gray16 format，分类UPSTREAM_WARNING，保留完整build.log。
- Fresh Linux：`bash scripts/amp/p026_build_isolated_kernel.sh` exit0，固定521833e2 +0003 transport patch，GCC11.4，官方scripts/config→olddefconfig。Image/modules/v2 base DTB/echo Kbuild/Host modules_install/depmod均成功，目标和echo零warning。新release配套使用，不声称原厂GCC10镜像精确重建。
- 原board模块255项vermagic匹配但MODVERSIONS关闭、CRC全0，审计返回`VERMAGIC_ONLY_NOT_ABI`（exit2），因此不混用原模块；不是失败就忽略。
- Fresh FIT：`p026_build_fit.py` exit0，hash/root totalsize/external payload一致；新包两2MiB slot，各FIT2060800B，余36352B。BL31/OP-TEE/controlDT逐字节不变；Ub main code变更。
- M0 BIN/ELF/MAP与合并AMP DTB沿用P023已完成clean build并重新核hash；本轮未改RTOS/HAL/linker，未冒称重新编译。新Linux initrd沿用Debian脚本，只更新模块release/内容和metadata。
- `readelf -h`确认Ub AArch64 EXEC entry40200000、echo AArch64 REL；`file`确认Image ARM64与initrd gzip。M0既有ARMv6-M/EABI5/soft-float身份不重复审查。

## 最终主要对象 SHA256

| 对象 | SHA256 |
| --- | --- |
| 新U-Boot双slot4MiB package | `4b6e615beba307d95beff1cbbff76b06bfe794f9a2f269599d2ccd5026ca4f1f` |
| 新AMP FIT | `0c745d05a5404e004500445787c0319cee2edc7970e761508fc263fb1dbef6a2` |
| RTOS BIN/rttmcu.bin | `a07e208836f5024fbc93674b62d4571dbb7ac6241789575228e40492626f2c78` |
| AMP DTB | `5369f9cb77831afc9d94763cb53ee9fbe9bd4ab406b8c5b1a0bf7a31e84059c1` |
| paired Image | `8c82342d740c9aefe0a4f0be8d84c436d14ebf7edb5f462e14631ce355cf3e8d` |
| paired initrd | `c000c9ab2126b0a2e64581514077a7ca79aa80660989ebfbcb3a200366a1346d` |
| paired255 modules.tar | `413dd5260c8a664f40c996fa3b7e9139b5539d87046b64f1a5cee9bf838435e3` |
| paired echo KO | `cd82f24659ba4a5a09fc314f5d9d3372b28451c8b21c7f64a868b11d11b5fe43` |

全部路径、size、源码与命令在manifest。`AMP_ARTIFACT_MANIFEST.json`当前指向P026；P025历史文件未覆盖。原件备份和test结果也纳入hash检查，不能因备份缺失仍报告完整性通过。Host CI/最终diff检查记录于P026结果收尾。
