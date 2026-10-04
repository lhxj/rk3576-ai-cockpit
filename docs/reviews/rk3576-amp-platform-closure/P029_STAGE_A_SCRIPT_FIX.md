> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# P029 阶段A：filesize 前缀错误修复与读回

2026-10-03，基线2c929c3。**旧A脚本实测STOP；新脚本已追加并读回PASS，新的Linux冷启动仍UNVERIFIED，整体C，D关闭。** 沿用用户“批准所有操作”的本任务授权。

## 根因与证据

| 证据 | 等级 / 结论 |
| --- | --- |
| 用户load A=1892B，source STOP length Image；printenv filesize=0x2930200 | BOARD_OBSERVED_USER_REPORT；停在Image payload load前，A无M0入口 |
| f8b4554 cmd/nvedit.c env_set_hex使用0x%lx；fs/fs.c的size/load均调用它 | SOURCE_VERIFIED；旧生成器及Bash stub误用bare hex，字符串比较必败，主控生成错误 |
| cmd/test.c的=走strcmp，-eq走十进制simple_strtol | SOURCE_VERIFIED；不能用-eq比较0x值，否则不同值可能同被解析为0 |
| cmd/part.c的partition number直接%d env_set | SOURCE_VERIFIED；分区编号2检查保持 |
| SSH原kernel6.1.99-rk3576/modelv2/U-Bootf8b4554/bootp2 | BOARD_OBSERVED；用户boot回原Linux，未进入新的A内核 |

## 修复和验证

A/B/C六个size/readback期望改为精确0x小写字符串；每次size/load前成功清旧filesize，防止vendor fs忽略env_set_hex错误后沿用旧正确值。保留文件长度预检、有限load、短读检查、原地址/DT/FIT和A→B→C门。

实际vendor C formatter/do_size/do_load/do_test对9个生成输入共175检查PASS；112包/脚本故障项、14 M0实际C、6既有snapshot及8新FD/noargs检查PASS；完整Host CI PASS。FS/环境/HAL为Host stubs，未执行目标HUSH/固件/MMIO，不外推为新A实板启动成功。独立子代理核心源码与一次新增目录流程审查PASS；主控固定tar、root代码、manifest SHA后执行。

仅新增 `/boot/amp-p029/script-fix-v5`：A2368B、B2581B、C2617B及manifest/SHA。root stdin Python使用-I，FD NOFOLLOW/NONBLOCK/regular/nlink/大小门后先读成不可变bytes并核固定manifest/所有SHA，再创建私有暂存目录并发布新目录。旧10文件/3receipt、factory六内容/链接/路径自身metadata前后相同；原内核/DT/FIT/KO/模块、默认入口及U-Boot未改。旧v4包、脚本和失败历史保留，不把它重标为成功。

实际安装一次exit0；独立SSH旧10、新4逐项SHA成功，manifest与Host相同，RAM源白名单清理。仍原Linux，没有Agent重启、M0、RPMsg或MMIO操作。原始输出只在ignored目录；精确hash和范围见 [machine记录](P029_STAGE_A_SCRIPT_FIX.json)。

独立子代理对最终保存的exit0、14条checksum成功、精确RAM清理及原kernel终审PASS；主控已读报告并接受其受控单owner/保护父目录及metadata/HUSH证据限制。报告摘要已封存在machine记录，子代理未访问板或运行安装器。

## 现在交接

用户正常关机、完整断电、冷上电，Debug Ctrl+C后：

```text
load mmc 0:2 0x4c000000 /amp-p029/script-fix-v5/stage-A.scr
```

须成功读取2368B再执行`source 0x4c000000`。登录后核release=6.1.99-rk3576-m0echo-p026、amp_test_stage=A、chosen标记A，并由主控SSH验实际DT/no-map/iomem；B/C尚待A和UART5采集安排。不使用原v4 A/B/C路径继续测试。具体步骤见 [当前冷启动指南](P029_STAGE_A_COLD_BOOT_GUIDE.md)。
