# P029：阶段A根分区参数修复

2026-10-03。当前C. HOST_BUILD_PASS，A登录/实际DT验收待新的冷启动，D关闭。

## 事实与根因

- BOARD_OBSERVED_USER_REPORT：v5脚本2368B正常载入；新配套内核启动、eMMC p1/p2/p3识别、运行`/init`。HDMI照片明确显示`ALERT! PARTUUID=614e0000-0000 does not exist. Dropping to a shell!`；MobaXterm无救援终端输入响应。
- SOURCE_VERIFIED：测试DT继承工厂DT的`/chosen/bootargs`短PARTUUID。P028 `f8b4554`，`arch/arm/mach-rockchip/board.c:1413/1459/1461`按DT bootargs→DT bootargs_ext→environment bootargs_ext合并；`cmd/nvedit.c:406/511`按名称替换root。原脚本只设bootargs，因此被DT覆盖。
- SOURCE_VERIFIED：配套initrd `scripts/functions:resolve_device`通过`blkid -l -t`解析PARTUUID，`scripts/local:local_device_setup`等待后进入救援终端。内核自身`init/do_mounts.c`允许PARTUUID前缀；不能仅凭短字符串断言内核原生解析非法。
- BOARD_OBSERVED_SSH：用户完全断电/默认启动后原Debian恢复；原release6.1.99-rk3576，root=/dev/mmcblk0p3，P028完整8MiB SHA9bd8cc03...71c6d5e匹配。p3完整UUID及短查询NOT_MATCHED/完整查询返回p3与照片吻合。factory六内容、旧10文件和v5四manifest/script校验PASS。实际命令保存在忽略目录factory-read.sh/stdout。

根因是测试包启动参数覆盖遗漏；不是以最后一条deferred-probe日志判断死机。尚无新A完整启动PASS。

## 最小修复与权限

生成器检查原bootargs设定成功，并增加：

```text
if setenv bootargs_ext 'root=/dev/mmcblk0p3'; then true; else echo STOP set root override; exit 1; fi
```

该environment extension在两项DT参数后合并。固定构建ANDROID_AB/ENVF/ENV_PARTITION均关闭；raw booti/mmc后续partition/fwver/Android/initrd处理没有再覆盖root。只覆盖易被继承DT改写的root；C的带引号dyndbg保留。

沿用用户本任务全部操作授权。仅准备/追加`/boot/amp-p029/root-fix-v6`三脚本+manifest/SHA，不替换旧v4/v5，不改原默认入口、kernel/initrd/DT/FIT/KO/modules/U-Boot/BL31/GPT。上传源在固定RAM目录，Host stdin安装器先核设备/原件/旧v5五文件和可信manifest，保留FD只读内存快照、新目录拒绝覆盖、fsync/读回；单个板端锁，有界一次执行。特权目录保持单写者假设，并非并发恶意root隔离。

## Host检查与独立审核

- `p029_test_vendor_bootargs.py`：57项实际vendor env_update/board merger和vendored libfdt检查；复现旧短UUID覆盖、恶意DText root、修复唯一root、A/B/C标签和quoted C保留。env和尾部服务为stub，尾部root范围另外由固定源码/config审核；不执行HUSH/固件/initrd。
- `p029_test_packet.py`：118包/脚本fault、14实际M0 C、6旧snapshot检查PASS；bootargs/envext设定失败在M0/booti前停止。
- `p029_test_vendor_filesize.py`：175项PASS；新root installer FD/no-argument8项PASS。
- `bash scripts/dev/host_ci.sh`：2 CTest+41 Python+5 withdrawn gate，PASS；git whitespace检查PASS。没有额外实板负载测试。
- 独立子代理只Host审查；核心PASS后主控审核并一次安装，随后双方检查保存的exit0/读回/RAM记录再终审PASS。报告hash见JSON；新A冷启动仍未验证。

## 执行与剩余项

独立核心审查PASS，主控审核后持锁一次执行exit0；仅新增root-fix-v6目录。旧10文件/3receipt/v5五文件和factory六内容/链接/路径自身metadata保持，三新脚本与manifest独立读回PASS，精确RAM源清理PASS，原Debian6.1.99-rk3576仍运行。A/B/C新脚本分别2525/2738/2774B；当前只交接A，不执行B/C。v4长度STOP和v5根查找失败保留为历史；新的A登录、实际DT/no-map/iomem未验收。B/C不执行，M0有效映射/cache、BL31 MCU setter动态结果、UART5采集和RPMsg回显仍待闭合。没有Agent重启/M0/MMIO/KO/刷U-Boot操作。
