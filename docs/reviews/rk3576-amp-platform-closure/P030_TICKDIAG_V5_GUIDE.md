> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

> 本次v5 B已执行并观察到延时返回与预期超时。用户已确认完整冷恢复默认系统；本次B通过，不重跑该脚本。旧C保持暂停，待新版入口准备。详见 [本次结果](P030_TICKDIAG_V5_B_EXECUTION.md)。

# P030 v5 B 已执行流程存档

2026-10-04。**首次延时返回和B冷恢复实板通过。下文保留已执行步骤供追溯，不再次运行。旧C入口保持暂停。** 当前结果见上方链接。

## 本次改动与观察

当前怀疑按24MHz设置LOAD，而实际外部输入接近SDK支持的32K。若为32768Hz，旧239999周期约7.324秒，100 tick的一秒延时约732秒，因此短时看不到返回。这是条件计算，频率尚未确认。

v5先用已分配UART5发送6144字符作独立参照（115200、8N1名义0.533秒），基线计数符合32K候选才改本地LOAD326，再核ISR/tick约增长53。打印共128条 `P030 pace ...` 是正常的两段参照输出，约12KiB。条件不符就STOP，不进入RPMsg/RT delay；未改共享时钟/映射或Linux transport。

## 1 保存双日志并完整冷启动

COM5：1500000、8N1、无流控；COM6：115200、8N1、无流控。开始保存后，正常关机、完整断电再上电，在COM5中断自动启动。

确认soc cold boot、SPL U-Boot7d8fe670d9/control DT43164981ef均OK、proper g149b1c5、policy=0；不匹配或abort/panic就停止。

## 2 加载脚本并核对后执行一次

```text
load mmc 0:2 0x4c000000 /amp-p029/tickdiag-v5/stage-B.scr
printenv fileaddr filesize
```

应为 **3055 bytes**、`fileaddr=0x4c000000`、`filesize=0xbef`。不符停止；不要设置变量绕过门，也不要插入其他load/size。

匹配后仅执行一次：

```text
source 0x4c000000
```

应出现 `P030 M0 tick diagnostic v5 B - factory default unchanged`，MCU payload SHA `28691e30790f1585782f3528dbd611c1e8005f8790947462574d73b8a75b6124`，loader成功后boot配套Linux。SCR SHA `60f8dcb149e4e5a948dcdafe12638856e30013b7a6cb569428e8850049c01181`，FIT SHA `348109ebecda0f00314d4dbcaaae0f8a51e2d88ac714c03ab74c1966a3b19bd6`。

## 3 查看COM6关键结果

- `baseline`：counts约17476、wraps0或1；结果符合门才出现 `slow reference matches SDK 32K candidate`。
- `corrected-begin/end`：LOAD应326；corrected ISR和tick增量约53（接受40..75）。
- `P030 tick rate gate PASS` 后，关注 `first_mdelay end`，tick delta约100，实际约一秒。
- B的Linux transport仍关闭，`link up=0`及约15秒 `STOP link timeout 15s` 是本阶段预计出口；它不等于RPMsg通信已通过。

保存完整COM5/COM6。**若任何STOP rate gate、panic/abort，或first_mdelay begin后10秒仍无end，停止此次会话并恢复默认系统；不重跑source、不进入C。** 无异常时观察至link timeout（最多30秒），正常关机，完整冷断电恢复默认Debian。

恢复后从COM5保存：

```text
uname -a
cat /proc/cmdline
```

应为默认6.1.99-rk3576 #8/root p3且无amp_test_stage。把双日志及恢复结果发回本对话；这一步才能确认时基/首次延时是否解决。当前没有宣布完整AMP/RPMsg通过。
