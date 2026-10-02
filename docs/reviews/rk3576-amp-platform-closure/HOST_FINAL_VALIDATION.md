# 本轮 Host 与板端只读验证

**P023补充（2026-10-02）：** 新冷启动proposal的M0/overlay/FIT、Linux headers对象/ko、完整U-Boot bin、完整候选Kernel Image/modules/DTB及新Kernel echo均clean build PASS；M0 122,696 B且0 warning。实际DTB与板原DTB全字节相同，完整Kbuild的Module.symvers与板headers副本相同，但运行Image精确source/build仍未证。9,672合同检查、26+1 startup Host mock及Host CI 14 Python测试/CTest通过。最新产物与hash见 [HOST_PACKAGE_MANIFEST.json](HOST_PACKAGE_MANIFEST.json)，复现与限制见 [HOST_PREBOARD_PACKAGE.md](HOST_PREBOARD_PACKAGE.md)。以下135,256 B和未编DTS等结果属于历史轮次。

固定 RTOS `7c397f41751feb29b0b388dfda3d2c2225f1f87c`、HAL `277de3fd4b0e640654ee73bb3308be2ef01e3aad` 的 reference 保持 clean；项目新 worktree 基于 `c2b2a83`，派生 RTOS 基于 `1d0de06`。Host 原始日志均保存在项目忽略的 `artifacts/local/`，没有运行候选二进制或固件。

| Host gate | 命令/结果 | 限制 |
| --- | --- | --- |
| M0 clean | `bash scripts/amp/build_candidate_host.sh`：`scons -c`/`scons -j4` exit 0；Arm GNU 13.2.1；ELF32 ARM EABI5 soft-float entry `0x141`；bin 135,256 B；`file/readelf -h/-S/-l/size/nm/objdump -h` 已保存 | 仅旧候选地址 0x47800000；不可部署 |
| M0 warnings | build log 仅 `ld: LOAD segment with RWX permissions` 1 条；无 pointer/alignment/overflow/implicit declaration warning | 裸机链接属性仍待安全设计 |
| FIT 结构 | 系统 `mkimage -f amp.its -E -p 0xe00 amp.img` exit 0；load `0x47800000`，entry unavailable、signature unavailable | 无最终 PA、无签名、无板端加载源 |
| Linux echo Host Kbuild | `bash scripts/amp/build_linux_echo_against_board_headers.sh` exit 0；板上实际 headers/Module.symvers 的 Host 副本；AArch64 KO、vermagic `6.1.99-rk3576 SMP mod_unload aarch64`、service alias 正确 | GCC 11.4 vs 板 GCC 10.3.1 提示；exact source commit 未证、未加载 |
| Contract | `python3 scripts/amp/check_platform_contract.py --linux-dts .../rk3576-amp.dtsi` exit **1** / 30 failures，含 link3/MBOX3 和所有未定 final 值；`python3 scripts/amp/test_check_platform_contract.py` 3/3 PASS | 这是故意 fail-closed 的阻塞结果；变更 service/link 会被拒绝 |
| LubanCat AMP DTS | **未编译**，最终 PA/CON17/cache/clock 不可合法填写 | Gate BLOCKED |

板端只读：`scripts/board/amp_platform_readonly.sh` 通过板锁执行，查看 uname、cmdline、boot 标识、headers/config、有限 dmesg/sysfs；`amp_copy_kernel_headers_readonly.sh` 只读 tar stream 到 Host。当前无 `amp` 分区、RPMsg/AMP DT 节点；`/dev/mmcblk0p1` 对普通账号不可读。未尝试 sudo、`/dev/mem`、debugfs 特殊寄存器读、sysfs 写、boot 写或重启。

候选 artifact SHA256、构建命令、架构、状态见 `AMP_ARTIFACT_MANIFEST.json`；`rttmcu.bin`、最终 AMP DTB/DTBO、最终签名 FIT 和 BOARD_MODULE_READY KO 不存在。**D 的 clean build gate 未通过，因为最终 DTS/FIT/地址设计尚不存在。**
