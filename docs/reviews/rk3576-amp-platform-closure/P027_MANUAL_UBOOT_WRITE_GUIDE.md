# P027：候选 U-Boot 的文件路径和手工写入步骤

> **P028撤回通知（2026-10-03）：不要执行本页历史上传/写入命令。P026真实原 Linux-only 启动失败，镜像已隔离，生产脚本已无条件拒绝 check/write。先按 [P028恢复记录](P028_LINUX_ONLY_FAILURE.md) 恢复原 U-Boot。下面仅保留历史审查证据，不能作为当前操作指导。**

2026-10-03。用户询问操作步骤；Agent 本轮未SSH、未上传文件、未写板、未重启。整体仍 **C. HOST_BUILD_PASS**。这是独立的候选bootloader Linux-only验证流程，不是D级AMP echo部署批准。

## 1. 文件

已复制到Windows桌面，并与Host原件核SHA：

```text
C:\Users\27432\Desktop\RK3576-AMP-P026-Uboot\uboot-host-unsigned.img
C:\Users\27432\Desktop\RK3576-AMP-P026-Uboot\p027_manual_uboot.sh
```

镜像4194304 bytes（4MiB），SHA256：

```text
4b6e615beba307d95beff1cbbff76b06bfe794f9a2f269599d2ccd5026ca4f1f
```

脚本6668B，SHA256 `18a07fa22b7b2c90f417906686814956fea6e2d0de094bc165bcc84800fc325a`；上传后也可用`sha256sum /dev/shm/p027_manual_uboot.sh`核对。不会要求安装新工具。

实际Host原件：`/home/ywx/rk3576-work/worktrees/rk3576-amp-platform/project/artifacts/local/p026-loader-package-final-v6/uboot-host-unsigned.img`。

原Ub备份：`C:\Users\27432\Desktop\RK3576-Recovery-Tools\P026-Originals-20261003\uboot-original-8MiB.raw`，8388608B，SHA `ae0a507485edd8e3a392dd7989de9c979ad744a9cd1d8b1813dbfe27e461de8a`。保留已核官方整机恢复镜像和恢复工具。完整整机回刷会覆盖未备份rootfs用户数据。

## 2. 本次实际改动

经用户明确选择执行后，只写当前既有label=`uboot`（解析必须为`/dev/mmcblk0p1`）前4MiB。脚本确认起始16384个512B扇区、整个分区8MiB及原SHA；不把偏移加到分区节点上。

脚本保留尾4MiB，对新整个8MiB的预期SHA是：

```text
2f6d9a427bbde0d658fbd55e9617a8d119f4d9e442f400ca126a21a095c08eea
```

这是候选4MiB+已核原尾4MiB的hash，不能和候选文件hash混用。不写GPT、rootfs、/boot；不改变ATF/OP-TEE payload内容，不更换原Linux/DT选择，不自动重启或启动M0。只保留现有Debug串口，1500000 baud，开始保存串口会话。此次不需要UART5接线。

项目AGENTS的L3规则要求bootloader改写及关机/重启明确批准。Agent没有把“询问如何做”当作自动执行授权；用户实际选择下面`--write`才是明确的手工写入动作。不并发运行其它刷写/部署工具；脚本锁只能排斥本脚本实例。

## 3. 上传到RAM暂存（Windows PowerShell）

```powershell
Set-Location 'C:\Users\27432\Desktop\RK3576-AMP-P026-Uboot'
Get-FileHash .\uboot-host-unsigned.img -Algorithm SHA256
scp .\uboot-host-unsigned.img .\p027_manual_uboot.sh cat@10.232.249.223:/dev/shm/
ssh cat@10.232.249.223
```

PowerShell里不要输入Linux写入命令。SSH连接失败或文件上传失败先停，确认网络/当前IP，不关闭主机指纹校验、不安装软件。已有MobaXterm SSH会话也可用：把两文件上传到该板`/dev/shm/`，随后在该板Linux终端执行后续命令。

## 4. 先检查（板端Linux终端）

```sh
sudo bash /dev/shm/p027_manual_uboot.sh --check /dev/shm/uboot-host-unsigned.img
```

必须退出0，输出`CHECK_PASS`及`CHECK_ONLY`。这个模式只读取flash/boot和核hash，复制临时证据到root私有`/run` RAM目录，退出清理；不写eMMC。通过`bash`读取脚本不需要从noexec的`/dev/shm`直接执行程序。

它检查board model、原Linux release、boot/root的实际mount source、partition label/实际block device/start/size/sector/read-only/mount状态、原Image/initrd/DT软链接及六项boot hash、候选size/SHA、原完整8MiB Ub SHA。任何`STOP`就停，把完整输出交给主控；不改脚本常量/注释检查，不换成裸dd。

## 5. 手工写入（只有明确决定执行此次bootloader验证时）

```sh
sudo bash /dev/shm/p027_manual_uboot.sh --write /dev/shm/uboot-host-unsigned.img
```

再次完整检查，root私有RAM staging核candidate后才打开目标写入，仅4个1MiB块，fsync。随后O_DIRECT读回8MiB，验证前4MiB与候选一致、尾4MiB逐字节不变、整体SHA。无自动重试、回滚或重启。

成功必须输出：

```text
WRITE_VERIFIED
partition_sha256=2f6d9a427bbde0d658fbd55e9617a8d119f4d9e442f400ca126a21a095c08eea
```

**写入开始后若失败、超时、中断、读回异常或没有WRITE_VERIFIED，保持当前Linux运行，不重启/断电、不重复写。保存完整输出，先审查恢复动作。** 当前可运行的Linux有助于恢复原8MiB；不能假定再次启动可成功。

## 6. 确认写入成功后，验证Linux-only启动

保存写入输出并启动Debug串口日志。在已明确批准的关机/冷上电范围内：

```sh
sudo poweroff
```

等待Linux正常关机完成后由用户完全断电，再上电；注意额外USB等是否仍给板供电，不用软reset充当已确认cold cycle。出现autoboot提示时Ctrl+C，进入`=>`，只执行：

```text
help amp_m0load
boot
```

`help`只查询命令；**不要实际执行amp_m0load，不读SGRF、不加载echo KO**。原Image/initrd/DT选择全部保留。Linux登录后：

```sh
uname -a
cat /proc/cmdline
```

把完整串口和两命令输出交给主控。第一次只验候选Ub出现项目命令、原Linux6.1.99-rk3576正常启动；不把它当MCU SMC/mapping/cache/RPMsg成功。

若Ub不能运行，停止操作，按P026原8MiB/官方恢复介质方案恢复；不猜RKDevTool地址单位、不选整机img按raw写。部分分区恢复是来源方案、未实测；用户整机恢复已有PASS但会覆盖rootfs。详见`P026_DEPLOYMENT_AND_ROLLBACK.md`。

## 7. Host验收

生产脚本没有fake device/跳过校验的测试开关。Host测试仅对临时副本替换硬件环境、fixture hash及块设备判断，实际用普通文件执行读/写路径；不访问MMIO或实板。已验证check无写、错误hash/大小/label/start/size/boot/mount拒绝，4MiB前缀写入、尾部不变，写失败和前缀/尾部读回失败不会宣布成功。此测试不证明真实eMMC I/O、U-Boot能启动或动态BL31调用。

- `python3 scripts/amp/test_p027_manual_uboot.py`：14项PASS，其中本地P026候选/原件/完整新分区SHA核对实际文件，没有skip。
- `bash scripts/dev/host_ci.sh`：exit0，2 CTest +40既有Python及shell语法检查PASS；新14项独立执行，不冒称已纳入该40项。
- 子代理独立静态审查PASS，主控审查/运行测试；`git diff --check` PASS。Desktop两文件与Host原件逐一核SHA。
- 未SSH、未执行板端脚本、未更改原AMP artifacts/合同或fixed reference；当前C、D门不变。
