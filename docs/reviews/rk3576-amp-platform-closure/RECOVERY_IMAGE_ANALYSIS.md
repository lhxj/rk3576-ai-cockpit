> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# P024：官方恢复镜像静态核验

2026-10-03。基线 `agent/amp-platform-closure / 080dda0`。**C. HOST_BUILD_PASS**；本轮未访问开发板。原镜像未修改，未运行其中代码。

## 镜像身份：HOST_TESTED

用户提供 `lubancat-rk3576-debian12-gnome-20260424_update.img`，位于 Windows 桌面同名解压目录。

| 项目 | 实测 |
| --- | --- |
| 长度 | 5,793,882,755 bytes |
| SHA256 | `18247661a898821a751262d57f18f6edf6aab0f62b4b706cfc1b74a4ea277a66` |
| 整文件 MD5 | `11c82c312eb2d1746e0b380671a48a2f` |
| RKFW 内部 content MD5 | `f631e684d3ca816fe0a5ba6fa8adc868`，按实际内容边界重算一致 |
| RKFW release | 2026-04-24 16:51:19 |

整文件 MD5 与[野火官方发布记录](https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/baidu_cloud/update_history.html)一致。官方表为 Debian12 GNOME 20260424、5.39GB；实测字节数约 5.396 GiB。MD5 用于匹配发布记录；另保存 SHA256，不以 MD5 代替数字签名。

**这是 RKFW/RKAF 升级容器，不是直接可写整盘的 raw GPT image。** 恢复对象是完整原 `.img`，不是下面分析得到的 `.payload`。

## 容器与提取边界

RKFW packed header 来自 Rockchip [RKImage.h](https://github.com/rockchip-linux/rkdeveloptool/blob/304f073752fd25c854e1bcf05d8e7f925b1f4e14/RKImage.h) / [RKImage.cpp](https://github.com/rockchip-linux/rkdeveloptool/blob/304f073752fd25c854e1bcf05d8e7f925b1f4e14/RKImage.cpp)，固定 `304f073752fd25c854e1bcf05d8e7f925b1f4e14`。其 `reserved[14:16]="HI"` 和 high32 字段用于 >4GiB firmware length；本镜像 high32=1。**SOURCE_VERIFIED**。

- header：102 B；outer loader：offset 102 / 770,553 B。
- RKAF：offset 770,655 / 5,793,112,068 B；结束于 5,793,882,723。
- 末尾仅 32 B ASCII MD5；没有 RKFW 签名尾部。
- RKAF 固定 table 参考独立实现 [rkafp.h](https://github.com/neo-technologies/rockchip-mkbootimg/blob/2348690523faee6ce3cea9eb9ff47e8b8d5e1df6/rkafp.h)，固定 `2348690523faee6ce3cea9eb9ff47e8b8d5e1df6`，**不是 Rockchip 官方格式保证**。自写解析器限制表数、范围、重叠、输出名和大小；没有执行该项目工具或复制其程序实现。
- rootfs filename 尾部存在非零扩展字节，完整 item length **不解码**，不按低32位截断提取 rootfs。RKAF CRC 与 loader CRC 没有验证；完整文件发布 MD5、SHA256及内部 content MD5覆盖了原文件全部字节。只提取普通长度的五个启动项。

| Item | 原镜像 offset | 提取 bytes | SHA256 |
| --- | ---: | ---: | --- |
| package-file | 772703 | 139 | `cc66533aa57b94eb00f80f7c510a5a54ec160a2f50f9472bb5ff825d0267ca62` |
| parameter（PARM wrapper） | 774751 | 411 | `60a3934f2e0f061e84703c74f1f1bd17e9728b1b4222a1f95929eea1899e8724` |
| MiniLoaderAll.bin | 776799 | 770553 | `4c0d04c6234b6e783aeedf317f4bb661700d9ef2ef97f876e67597807aa86206` |
| uboot.img | 1548895 | 4194304 | `06f48ed3716ccdde6adcb3187ca3eff766927e27bf6b7f432046539b782c4a56` |
| boot.img | 5743199 | 134217728 | `28ac0f575bd538afdff63fb15917b685a335ee1d0b88de019ee84d29c84f9fd0` |

outer loader 与 RKAF bootloader 项 hash/bytes 一致。`parameter` 的 PARM header 声明399 B文本，实际399 B；文本SHA256=`e93b841868d8c07991b4ac47875ce77a5692807ccf247ceb5d5e062303d8e193`。

## 分区与 AMP 来源

`parameter.txt` 明确 `TYPE: GPT`：

| GPT name | Start LBA | Sector count | 用途 |
| --- | ---: | ---: | --- |
| uboot | `0x4000` | `0x4000` | 8 MiB partition，升级payload为4MiB |
| boot:bootable | `0x8000` | `0x40000` | 128 MiB ext filesystem |
| rootfs:grow | `0x48000` | grow | 剩余rootfs |

`package-file` 仅 package-file、parameter、bootloader、uboot、boot、rootfs。没有 `amp`。与此前当前板三分区盘点一致，**HOST_TESTED + 先前 BOARD_OBSERVED_READONLY**。这个官方包没有补出 M0 FIT 加载入口；固定公开 `amp_cpus_on()` 仍按 GPT 名 `amp` 查找，当前实际 `CONFIG_AMP` 与其它受支持加载路径仍未证明。此次不修改分区或设计替代 loader。

## 与当前板原件比较：HOST_TESTED

`uboot.img` 4MiB **逐字节等于**此前板端8MiB uboot分区的前4MiB。标准 Host `dumpimage` 提取六个FIT项，均与板端既有提取文件逐字节一致，包含 U-Boot、atf-1/2/3、OP-TEE、control DTB。BL31 主段仍为 `1c50b242b2c19722003b1e7423fff432c6450d6341b8cb7f3bf0ca5fdbf9ce07`，U-Boot payload 为 `c084f257e761b898c45d681a2d1b739ca2cdd99b3916b64f3e9b34109d7a865d`。

不带 `-w` 的 `debugfs` 读取独立 Host `boot.payload`，提取文件与板端原件tar比较：

| 文件 | 结果 |
| --- | --- |
| Image-6.1.99-rk3576 | 完全一致，SHA=`e3b7fcc1102102f31de56ffe5d2f82f9ab5ccb8c2843bc972b716d3222b2233e` |
| rk3576-lubancat-3-v2.dtb | 完全一致，SHA=`76089b93bb40a2ff1045b9d4a0511f0cb57d9b9f25fccfc5e2ba314c309f1f90` |
| boot.cmd / boot.scr | 完全一致 |
| config / System.map / extlinux.conf | 完全一致 |
| Image symlink | factory指向 `Image-6.1.99-rk3576`；与当前原件目标相同 |
| factory V2 uEnv vs 当前 active uEnv | 仅当前取消 CAM0 OV8858 1632×1224 overlay 的注释；恢复应优先用当前原件 |

factory `/uEnv/uEnv.txt` 是329 B通用初始配置，**不是**当前6061 B active uEnv，不能直接覆盖。factory V2模板为6062 B，SHA=`92cace8b29b727b7336ddc3412915bc48f2d0d37e3c02a021883ead25f27567c`；当前active SHA=`4f7fec696792517b6f6db7c5aabc77a2242c7c2c15f2708aab5207e03caaef64`。

Image 内版本字符串为 `6.1.99-rk3576 #8 / 2026-04-24`、GCC10.3.1、binutils2.36.1，与此前板端记录一致。**运行Image与该官方发布payload身份闭合**；不因此宣称固定kernel pin521833的完整源码/config/toolchain已经bit-for-bit复现该Image。

## loader 与签名：不混同运行权限

按 Rockchip [RKBoot.h](https://github.com/rockchip-linux/rkdeveloptool/blob/304f073752fd25c854e1bcf05d8e7f925b1f4e14/RKBoot.h) 解码LDR表：UsbHead、rk3576_ddr_lp4_1848M、rk3576_usbplug_v1、FlashHead/Boost/Data/Boot；sign flag=0、RC4-disable flag=1、version raw=`0x164`。这些是包元数据，不能替代板上OTP/secure-boot policy。没有运行loader。

SDK rkbin `58a39b47f77a26a1e110fa1a1ce80bcfcb0b3505` 的 DDR v1.09 bin（75,936 B、SHA=`c03d09204ce267ffd55023a600b9ad81fc00b5447d4db87fec8f5f5308f75b6f`，Git blob=`10539a78c7009ee49f01f857d2a9442c52522e34`）与Usb DDR entry前75,936 B完全一致。FlashData有另外封装，未作同一文件断言；也未取得当前eMMC first-stage的完整原始副本。

U-Boot FIT conf 的 `signature` 只有 `algo/key-name-hint/sign-images`，没有 `value/hashed-nodes`；control DTB 没有 `/signature` key节点。**不能将 `sha256,rsa2048:dev` 占位名称当成实际签名**。这些只证明包内容，没有证明BootROM OTP、SPL或实际U-Boot的AMP FIT强制验签策略；该gate仍BLOCKED。不关闭verified boot。

## 可复现检查

```sh
python3 scripts/amp/inspect_recovery_image.py /mnt/c/Users/27432/Desktop/lubancat-rk3576-debian12-gnome-20260424_update/lubancat-rk3576-debian12-gnome-20260424_update.img \
  --output artifacts/local/p024-recovery-image/payloads \
  --expected-md5 11c82c312eb2d1746e0b380671a48a2f \
  --expected-sha256 18247661a898821a751262d57f18f6edf6aab0f62b4b706cfc1b74a4ea277a66
dumpimage -l artifacts/local/p024-recovery-image/payloads/uboot.payload
dumpimage -T flat_dt -p 1 -o artifacts/local/p024-recovery-image/fit-files/image-1.bin artifacts/local/p024-recovery-image/payloads/uboot.payload
debugfs -R 'ls -l /' artifacts/local/p024-recovery-image/payloads/boot.payload
python3 -m unittest discover -s tests/python -p test_amp_recovery_image.py -v
bash scripts/dev/host_ci.sh
```

输出目录须新建；解析器拒绝覆盖已有payload/manifest。实际容器/比较结果见 [RECOVERY_IMAGE_IDENTITY.json](RECOVERY_IMAGE_IDENTITY.json)。八项解析边界测试PASS；全Host CI为2项CTest、22项Python测试PASS。loader decoder在首次提取后另执行并加入manifest，原文件只读；没有再次提取固件。原始payload/log仅留忽略目录。

闭合：完整原恢复包、loader/parameter身份、与当前boot/firmware的全字节关系、Host工具准备。仍缺：实际AMP启动/加载入口、动态SiP/映射、验证策略、最终可部署changeset、USB恢复识别和用户数据恢复范围。因此不升级D。
