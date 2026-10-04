> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# P023 构建失败定位与warning分类

| 对象 | 结果 / 分类 | 处理 |
| --- | --- | --- |
| M0最新clean v5 | exit0，0 warning | callback已按MBOX_CMD_DAT*修复，`-Werror=incompatible-pointer-types/implicit-function-declaration`；实际ELF两个RX/RW LOAD、无overlap/RWX |
| M0旧v1/v2/v4尝试 | 不是最终产物 | `--defconfig`非此SCons参数→实际`--useconfig`；关闭GPIO后未用PERI5V helper仍引用GPIO→按原配置guard；关闭TIMER11后vendor调试timestamp引用SYS_TIMER→guard。均定位根因，不随机修复 |
| Linux headers Kbuild | compiler differs，UPSTREAM_WARNING | 当前headers为GCC10.3.1，Host11.4.0；transport对象和echo ko均exit0，无callback/implicit/overflow等必修warning。保留差异，不叫BOARD_MODULE_READY |
| U-Boot完整候选build | Host `bmp2gray16.c:121`字符串缺nul警告，UPSTREAM_WARNING | 原Host图像工具`version[4]`作`%s`；本轮未执行它，不属于AMP target路径，保留原warn及源码，不宣称全树0warning |
| DT overlay编译、fdtoverlay | exit0，overlay 0 warning | DTB反编译会输出原板DT已有的检查提示，非候选overlay新增warning；不能以此称整个vendorDT全树无warning |
| FIT | exit0、payload提取与BIN全字节匹配 | SHA256 image hash，不是签名；当前verified-boot策略未证，不关校验来绕过 |
| 完整候选Kernel依赖准备 | 初次缺flex；后续flex缺m4 | 取Ubuntu22.04官方flex/bison/libelf/m4 `.deb`，核本机APT索引SHA，解包到任务目录，无全局安装或配置修改；通过M4/PATH/BISON_PKGDATADIR等供本次Host构建使用 |

完整日志保存在忽略的`artifacts/local/p023-*`目录；最新目录、log hash、source/config/toolchain与产物记录见manifest。不得复用失败/旧目录中的文件作为最终产物。
