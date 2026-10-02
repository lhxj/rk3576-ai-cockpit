# P025：精确部署与回滚清单（BLOCKED，未执行）

2026-10-03。机器清单 [P025_DEPLOYMENT_CHANGESET.json](P025_DEPLOYMENT_CHANGESET.json)、文件身份 [P025_ARTIFACT_MANIFEST.json](P025_ARTIFACT_MANIFEST.json)。本文件给未来可审查动作，**不是已获批准部署方案**。分区/验签/运行gate未闭合，所以不写含猜测device或sector的刷写命令。

## 当前 Linux-only 原件

恢复后重新只读复制：`artifacts/local/p025-post-recovery-20261002T173913Z-975579/boot-originals.tar`，SHA256=`ed3ef563e21be440de84f68f57093d1e977e6cc10db25d41e6b682ed312b0b7c`。tar保留原文件类型、权限、symlink；含当前initrd/config/System.map、uEnv/extlinux、v2DTB/boot脚本。

| 板端原件 | 类型/目标 | SHA256（file content） |
| --- | --- | --- |
| `/boot/Image` | symlink → `Image-6.1.99-rk3576` | target file `e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e` |
| `/boot/rk-kernel.dtb` | symlink → `dtb/rk3576-lubancat-3-v2.dtb` | target file `76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90` |
| `/boot/uEnv/uEnv.txt` | regular file 6061B，含既有CAM0 overlay | `4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64` |
| `/boot/boot.cmd` | regular file | `9f0262e807a8188ec5dffe52411401bd82b5fb0d8e81af8107284725e2a7db55` |
| `/boot/boot.scr` | regular file | `c498d9be3e8dc91883124cc734be54c42c271fad0501555c1a69f7ea7fc38325` |
| `/boot/initrd.img-6.1.99-rk3576` | regular file | `425d2a68a4a67e807518c067cfefc1111d0a33af02236761ddaa3794c3205397` |

当前U-Boot fwver `8f53f800da-04/24/2026`、BL31`v1.14`；原8MiB uboot分区备份在P023和桌面Original-Board-Boot-20261002，恢复后raw hash本轮没有重新读取。未来写入前必须重新匹配，不能把版本串当成raw hash。

## 拟修改对象（审批前不得执行）

| 对象 | Host source | 拟板端destination | old → new identity | 风险/rollback |
| --- | --- | --- | --- | --- |
| Linux Image | P023完整kernel build/Image | `/boot/Image-6.1.99-rk3576-m0echo-host` | 新文件，需写前确认不存在；new `688561ba8f1bc4c201e21126a5907c8ab397c2d69ec0e7860401be32caa619bf` | built-in transport变更，modules/ABI升级计划未定；旧Image保留 |
| AMP DTB | P023 amp-host.dtb | `/boot/dtb/rk3576-lubancat-3-v2-m0echo-host.dtb` | 新文件；new `5369f9cb77831afc9d94763cb53ee9fbe9bd4ab406b8c5b1a0bf7a31e84059c1` | reserve+MBOX0/4启用；旧DTB保留 |
| 默认Image/DTB入口 | symlinks | `/boot/Image`、`/boot/rk-kernel.dtb` | exact old/new link text见JSON，不能用file content hash标symlink | 原两个target文件验hash后切回 |
| AMP-enabled U-Boot | P025 uboot-host-unsigned.img | 现有label `uboot`，观察为mmcblk0p1；写前重新核身份 | old raw hash UNKNOWN；new `c6448d11b130fab7154be4edebc6b3d8ee9508f056a7f86d0f8a04b832c98488` | early loader、未签名；完整官方恢复包回滚 |
| RTOS FIT | P023 amp-host.itb | **UNRESOLVED**，官方loader要求GPT label `amp` | 当前分区不存在；new `885ab2a7e14c58f6d8891158c027dfa9e38c81e0c51508fc95172209c542ef97` | 不把FIT存到/boot当作已支持路径，不猜GPT offset；需正式分区/数据方案 |
| echo module | prospective kernel对应P023 echo ko | `/opt/rk3576-amp-test/rk3576_amp_echo_test.ko` | 新文件；new `3d06c2c0427cc66d0b7e9b7bc079b1129e8a8d50f840a16118eff87d6a18023c` | 第一次未来测试手动加载，无autoload；仅HELLO/PING |

所有source完整路径和size/hash在manifest；P023 M0 BIN122696B，SHA=`a07e208836f5024fbc93674b62d4571dbb7ac6241789575228e40492626f2c78`。不是本轮重编M0。uEnv、boot脚本、原initrd及原件文件保留；既有Camera/Audio/Wi-Fi/Display DT属性经Host检查保留，不宣称新kernel已回归这些功能。

未来流程必须是：核gate和实际签名/授权→完整用户数据备份与exact GPT计划→核旧hash/原件在两个独立位置可用→安装新文件但不切入口→核new hash和boot空间→逐次批准bootloader/AMP source和入口切换→用户控制重启/串口→仅Linux+M0 banner+HELLO/PING。**当前阻塞时停在第一步**。不能通过先改GPT再“试是否支持”规避gate。

## 回滚分支（本轮仅记录）

### 仅两个 symlink 改变，Linux还能进入

先核原target hash，再把两个symlink切回（以下仅为未来获批且本清单确实执行过之后的命令）：

```sh
cd /boot
if printf '%s\n' \
 'e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e  Image-6.1.99-rk3576' \
 '76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90  dtb/rk3576-lubancat-3-v2.dtb' | sha256sum -c -; then
 sudo ln -sfn -- Image-6.1.99-rk3576 /boot/Image &&
 sudo ln -sfn -- dtb/rk3576-lubancat-3-v2.dtb /boot/rk-kernel.dtb &&
 readlink /boot/Image /boot/rk-kernel.dtb
else
 echo '原件校验失败，停止恢复写操作' >&2
fi
```

不删除新文件也可恢复入口；无需覆盖整个/boot。active uEnv若未来另行改动，单独恢复tar里的6061B文件并核原SHA，不能用factory329B通用模板。

**切回Linux Image/DTB不会停止已经运行的M0，也不能阻止早于prompt的AMP loader。** 如果U-Boot/GPT/FIT也被改变，必须恢复该早期入口；不能把上述symlink命令当作AMP完整rollback。

### 无Linux，或早期AMP/U-Boot/GPT改变

已有Debug串口1500000 8N1用于保存启动日志；不依赖能到prompt。已核恢复文件：

`C:\Users\27432\Desktop\lubancat-rk3576-debian12-gnome-20260424_update\lubancat-rk3576-debian12-gnome-20260424_update.img`

SHA=`18247661a898821a751262d57f18f6edf6aab0f62b4b706cfc1b74a4ea277a66`。RKDevTool路径：

`C:\Users\27432\Desktop\RK3576-Recovery-Tools\RKDevTool_v3.32_for_window\RKDevTool.exe`

用户执行：断开供电→OTG数据口连PC→按MR上电→识别MASKROM→“升级固件”选择该完整update.img→升级恢复官方三分区/Linux-only；正常U-Boot的REC/LOADER入口可作另一路，但early失败优先MR。完整步骤与工具来自 [已核官方恢复说明](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/flash_img/flash_img.html) 和P024工具报告。用户已确认整机恢复实测PASS，不重复演练；没有声称两模式均测试。

这是RKFW/RKAF完整恢复包，**不能当raw GPT镜像dd到整盘**。它包含所需DDR/loader/U-Boot/BL31/OP-TEE/boot/rootfs，不依赖未知恢复payload。该包回到官方U-Boot与无amp GPT，从而阻止下一次AMP自动加载。整包恢复会重写rootfs；**用户数据备份仍未完成**，不能因boot tar齐全说应用/模型/数据已备份。

恢复后先只验Linux版本/model、三个partition与原boot hash；恢复active uEnv和用户数据另行批准，禁止自动重装AMP。整机恢复路径/物料与用户实测已闭合；针对尚未存在的amp GPT及验签配置的最终部署rollback仍BLOCKED。

## 自动拒绝错误清单

`python3 scripts/amp/check_deployment_packet.py`默认Linux exit2、deployment_gate=BLOCKED；所有文件hash/contract匹配时Host integrity仍PASS。`--allow-blocked-for-host`只允许CI验证清单本身，不能表示D或部署授权。本轮没有执行上述回滚命令或板端写动作。
