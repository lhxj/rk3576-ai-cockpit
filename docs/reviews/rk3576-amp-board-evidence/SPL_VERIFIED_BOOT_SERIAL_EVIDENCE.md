# 串口启动日志：SPL 验签策略证据

2026-10-03接收用户提供的完整启动日志；日志内Linux时间为2026-10-02。证据等级 **BOARD_OBSERVED_USER_REPORT**，不是Agent执行的新启动或寄存器读取。这里只保存与本任务相关的摘录，不提交包含登录凭据、网络信息等内容的全文。

## 原始相关摘录

```text
U-Boot SPL 2017.09-g8f53f800da-241224 #jiawen (Apr 24 2026 - 16:49:57)
Trying fit image at 0x4000 sector
## Verified-boot: 0
## Checking atf-1 0x40040000 ... sha256(1c50b242b2...) + OK
## Checking uboot 0x40200000 ... sha256(c084f257e7...) + OK
## Checking fdt 0x40350578 ... sha256(2eef029c7e...) + OK
## Checking atf-2 0x400f0000 ... sha256(8afb712810...) + OK
## Checking atf-3 0x3fe70000 ... sha256(3af1bf762b...) + OK
## Checking optee 0x48400000 ... sha256(010f86355a...) + OK
Jumping to U-Boot(0x40200000) via ARM Trusted Firmware(0x40040000)
NOTICE:  BL31: v2.3():v2.3-859-gc481e5368:derrick.huang, fwver: v1.14
NOTICE:  BL31: Built : 09:37:28, Nov  8 2024
I/TC: OP-TEE version: 3.13.0-891-g9f2aca7d1
U-Boot 2017.09-g8f53f800da-241224 #jiawen (Apr 24 2026 - 16:49:57 +0800)
TEEC: Waring: Could not find security partition
Found U-Boot script /boot.scr
[boot.cmd] booti 0x40400000 0x4a200000:0x63bcef 0x48300000 ...
Starting kernel ...
```

OP-TEE一行只截取版本字段；其余行保留用户日志内容。六个SHA仅是打印的前缀，不当作完整文件hash。此前恢复包/原板payload全文件身份见P024报告；版本和短hash相符不等于本次重新取得raw分区hash。

## 已闭合与尚未闭合

| 结论 | 证据等级 | 边界 |
| --- | --- | --- |
| 记录的SPL启动返回`Verified-boot=0`，六个原厂payload SHA256检查通过后进入U-Boot | BOARD_OBSERVED_USER_REPORT | 该次SPL→U-Boot FIT未要求签名；不外推为全系统secure boot/OTP关闭 |
| 实际版本为U-Boot `8f53f800da`、BL31 `c481e5368 / v1.14`、OP-TEE `9f2aca7d1 / v1.05` | BOARD_OBSERVED_USER_REPORT | 没有显示AMP加载或MCU SMC调用，不证明它们动态可用 |
| 后续从mmc boot分区读取boot.scr/uEnv/Image/initrd/DTB，再`booti` | BOARD_OBSERVED_USER_REPORT | 没有第二个AMP FIT的验签结果，也没有AMP source读取记录 |
| proper U-Boot的AMP验签策略值、授权公钥、OP-TEE读flag是否成功 | UNVERIFIED | 本日志没有走该路径；部署门继续BLOCKED |

固定vendor pin `8f53f800da2c25d0c6ba414fb45902a01675703a`的实际源码链：

- `arch/arm/mach-rockchip/fit_misc.c:240–280 / fit_board_verify_required_sigs`：SPL可选读secure OTP；proper U-Boot在`CONFIG_OPTEE_CLIENT`下调用`trusty_read_vbootkey_enable_flag`。读失败返回required=1，成功才打印flag。SPL未编secure-OTP分支时也可保持初始0，因此日志不能证明实际OTP字节为0。
- `lib/optee_clientApi/OpteeClientInterface.c:637–657`：使用`STORAGE_CMD_READ_ENABLE_FLAG`、`is_write=false`；仅`TEEC_SUCCESS`且`bootflag==0xff`才把flag置1。下面的`trusty_base_efuse_or_otp_operation:527–602`打开TA会话、输出参数读取，任何初始化/会话/调用失败可返回错误。
- `lib/optee_clientApi/OpteeClientRkFs_common.c:21–85`：`Could not find security partition`来自查找名为security的**文件存储**分区；`OpteeClientRkFsInit`对该状态返回0。它不是验签flag输出，也不能说明读OTP/TA成功、失败或secure boot关闭。

以上为 **SOURCE_VERIFIED**。P025项目补丁显式在AMP复制/release前调用proper U-Boot policy并拒绝错误/缺required-conf-key；不是厂商发布中的同名功能已在当前板执行。日志中的`Verified-boot:0`不能代替这个独立结果。

## 地址证据的限度

用户日志显示Image装载`0x40400000`、43,188,736B；DTB`0x48300000`、initrd`0x4a200000`；可用RAM银行包含`0x40200000..0x48400000`，OP-TEE从`0x48400000`起。它支持现有Host候选code/rings处于首段RAM且与**这些记录的固定对象**不重叠。不能据此排除U-Boot动态malloc/sysmem、以后增大的Image或其它loadable覆盖候选512KiB code/shared区，也没有CON16/17当前值。

## 裁决

`SPL_TO_UBOOT_SIGNATURE_REQUIREMENT = NOT_REQUIRED_FOR_RECORDED_BOOT`。

`PROPER_UBOOT_AMP_SIGNATURE_REQUIREMENT = UNVERIFIED`；授权key和合法部署包仍未闭合。当前仍 **C. HOST_BUILD_PASS**；没有部署许可，不重复请求已经提供的启动日志，不重试MMIO，不启动M0。
