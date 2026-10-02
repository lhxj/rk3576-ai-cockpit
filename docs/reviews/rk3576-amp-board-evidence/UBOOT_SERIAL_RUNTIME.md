# 当前 U-Boot 串口观察（收到于 2026-10-02）

证据来源：用户通过 MobaXterm 在实机 `=>` 提示符下手动输入，并上传串口原始文本。证据等级 **BOARD_OBSERVED_USER_LOG**；本次 Agent 未连接板端或发送串口命令。

```text
=> echo OK
OK
=> version
U-Boot 2017.09-g8f53f800da-241224 #jiawen (Apr 24 2026 - 16:49:57 +0800)

aarch64-none-linux-gnu-gcc (GNU Toolchain for the A-profile Architecture 10.3-2021.07 (arm-10.29)) 10.3.1 20210621
GNU ld (GNU Toolchain for the A-profile Architecture 10.3-2021.07 (arm-10.29)) 2.36.1.20210621
=> printenv bootcmd bootdelay
bootcmd=run distro_bootcmd;boot_android ${devtype} ${devnum};boot_fit;bootrkp;
bootdelay=0
=> base
Base Address: 0x00000000
```

`?` 正常列出的命令包括 `version`、`printenv`、`base`、`md`、`boot`、`boot_fit`、`download`、`rockusb`。此前列表内命令报 Unknown command、手动输入后正常，支持输入/粘贴问题的解释；隐藏字符或串口 TX 的确切根因未测，不写为已修复某项硬件故障。

当前 U-Boot 的版本/工具链、命令可用性、默认启动环境与 memory-display offset 均有实机证据。`base=0` 使完整 `md.l 26004060 1` 的请求地址不带额外偏移；CON16/17 是否可读及其值尚无串口结果。`boot_fit` 命令存在不证明 AMP loader 编入或启用，默认 `bootcmd` 也不能单独排除更早的 AMP 初始化。

后续按用户既有授权逐条读取两个目标地址，第一条异常即停止；禁止空行重复 `md`（该实现会前移上次显示地址）。不推算 CON17、不改 AMP linker/ITS/DTS。正常返回后 `boot` 可执行未修改的默认 `bootcmd`。整体仍 **C. HOST_BUILD_PASS**。
