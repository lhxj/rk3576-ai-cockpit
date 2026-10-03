# P029 阶段A：已通过的冷启动步骤

2026-10-03。**最新v6阶段A已通过：配套Linux进入Debian，最终root=p3/stage=A；主控SSH只读核三段no-map及无System RAM重叠、五个相关节点disabled、无RPMsg设备/echo模块。无需重复A。** 用户已批准本任务操作，按A→B→C验收推进。下一步先落实UART5早期日志采集，再另一次冷启动阶段B。以下保留已经实际使用的A步骤，M0/RPMsg关闭。

## 1. 保持 Debug 串口并保存日志

MobaXterm 保持原 Debug 接线和 **1500000 / 8N1**。开始保存串口日志，覆盖DDR、U-Boot到Linux登录。本阶段现有一个USB-TTL足够，UART5采集稍后安排。

## 2. 正常关机，再完整冷上电

在当前Linux中执行：

```sh
sudo poweroff
```

等待串口显示关机完成，完全断开所有会给开发板供电的连接，再恢复供电。物理断电/上电由用户完成。

## 3. 进入 U-Boot 命令行

看到启动日志时按 **Ctrl+C**，进入 `=>`。应看到 proper U-Boot `f8b4554`、运行时policy=0。身份不同或出现异常时停止，保存日志。

先执行这一条：

```text
load mmc 0:2 0x4c000000 /amp-p029/root-fix-v6/stage-A.scr
```

应读到 **2525 bytes**；读取失败或长度不同就停止，不执行下一条。

读取成功后执行：

```text
source 0x4c000000
```

脚本核boot分区编号、Image/initrd/DT大小及有限读取，并用checked bootargs_ext让最终root保持`/dev/mmcblk0p3`，再显式booti。A脚本没有M0加载命令，A DT的mcu-amp/RPMsg/mbox0/4均disabled。

## 4. Linux 登录后给出身份输出

```sh
uname -a
cat /proc/cmdline
tr -d '\000' </sys/firmware/devicetree/base/chosen/project,p029-stage
printf '\n'
```

预期kernel release为 **`6.1.99-rk3576-m0echo-p026`**，cmdline包含 **`root=/dev/mmcblk0p3`** 和 **`amp_test_stage=A`**，最后DT标记为 **`A`**。随后保持当前Linux运行，把完整启动日志及这些输出发到本聊天；主控继续用SSH只读核实际DT/no-map、iomem及无AMP transport状态。

## 5. 停止条件与恢复

如果脚本返回STOP、Linux未进入登录、出现abort/panic或身份不符，保存日志并停止本阶段。完整断电后再上电，让未改的factory默认入口自动启动原Linux，核原release `6.1.99-rk3576`。阶段A未执行M0，异常不跳过到B/C。

本次新增文件身份已核验：10个boot文件、268个module文件及3个receipt；原六启动文件内容和原启动链接/路径自身metadata前后相同。暂存不等于新内核启动PASS；相机/音频等功能也不由Linux登录单独证明。

证据：[根分区修复和实际读回](P029_STAGE_A_ROOT_FIX.md)、[前缀修复和实际读回](P029_STAGE_A_SCRIPT_FIX.md)、[P029_STAGE_A_RESULT.md](P029_STAGE_A_RESULT.md)、[P029_STAGE_A_EXECUTION.json](P029_STAGE_A_EXECUTION.json)。**A冷启动与内存预留PASS，整体 C. HOST_BUILD_PASS，D关闭；B/C动态映射、缓存和通信待实测。**
