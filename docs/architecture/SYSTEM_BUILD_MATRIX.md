# SYSTEM_BUILD_MATRIX

日期：2026-10-04。源码来源是两个权威tip，互不要求一个CMake入口编译全部域。

| 范围 | 既有入口与必要输入 | 产物/运行环境 | 本轮边界 |
|---|---|---|---|
| Application Host | `bash scripts/dev/host_ci.sh`；`cmake --preset host-debug`、build/CTest；Qt5当前Host可用；外部真实后端默认OFF | x86_64 ELF、31项CTest；Python/AMP/shell检查 | 本轮重建/测试；不证明真实设备 |
| Application RK3576 | 原生CMake，启用`COCKPIT_ENABLE_V4L2_CAMERA`、`COCKPIT_ENABLE_MPP_RECORDING`、`COCKPIT_ENABLE_RKNN`、`COCKPIT_ENABLE_SHERPA_ASR`、`COCKPIT_ENABLE_ALSA_CAPTURE`；按现有板端路径提供RKNN/Sherpa头库，系统Qt5/MPP/ALSA | AArch64 UI/测试/应用库；默认#8既有35/35CTest | 本轮是否重建另看BUILD_MATRIX，不能把历史35/35算paired kernel共存 |
| RT-Thread BUS M0/HAL | `patches/rk3576-amp-integration/series.json`：固定RTOS/HAL import和累计patch，再顺序0010-M0至0015；HAL symlink匹配固定派生；SCons/GCC ARM embedded | `rtthread.elf/bin`，运行BUS M0；版本4.1.1 | 固定v5；现有入口`scripts/amp/p030_prepare_tickdiag_v3.py --revision 5`含外部依赖/签名步骤，本轮不自动执行 |
| U-Boot/AMP FIT | 固定LubanCat8f53f800→149b1c5累计patch；现有Host package/签名链；`platform/rk3576/amp-minimal/amp-signed.its` | 8MiB U-Boot、signed FIT；bootloader，不是Host可执行程序 | 冻结输入/hash；不读取私钥，不重新签名或刷写 |
| paired Linux RPMsg transport | 固定kernel521833e2+0003 patch；`scripts/amp/p026_build_isolated_kernel.sh`，匹配`.config`/Module.symvers及AArch64工具链 | `6.1.99-rk3576-m0echo-p026` Image/modules；不是默认#8字节重建 | 保留独立kernel入口，不自动改当前boot |
| Linux minimal echo KO | `scripts/board/amp_echo_linux/Makefile`/C；匹配上述kernel build与Module.symvers，Kbuild `make -C <paired-build> M=<echo-source> ARCH=arm64 CROSS_COMPILE=<prefix> modules` | AArch64 `.ko`；HELLO/PING一次性测试 | 不是`apps/rpmsg_srv`用户态业务接口；不向默认#8加载paired KO |
| C DT/SCRIPT封装 | 固定基础DT+原CAM0 overlay+`m0-transport.dtso`；`p029_make_packet.py`；`p030_prepare_rpmsg_c_v1.py`封装既有v5 FIT及`stage-C.cmd` | C DTB、3100B SCRIPT及配套部署包 | 精确输入不变；旧完整contract/JSON是历史快照，不能当当前部署许可 |

真实Application原生构建命令、模型/头库版本与baseline路径见
Application tip的`docs/bringup/vision/`、`docs/bringup/voice-runtime/`及
`docs/amp/AMP_RPMSG_INTEGRATION_TIP.json`的AMP输入/产物身份。

不进入Git：SDK整树、sysroot、模型、ELF/BIN/KO/Image/DTB/FIT/U-Boot、私钥、
原音视频、未脱敏日志、build目录。仅源码/补丁/项目配置及脱敏证据入Git。
本地冻结产物来自既有AMP worktree的忽略目录，读取/hash核验不复制进产品仓。

本轮实际退出码/构建/检查结果见[BUILD_MATRIX](../bringup/system-integration/BUILD_MATRIX.md)。
