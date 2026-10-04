> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# FIT 裁决：Host 结构 PASS，最终装载 BLOCKED

## P025/P023更新

最新Host AMP FIT明确`entry=0x141`、loadPA=`0x47800000`，payload122696B，与实际ELF/BIN逐字一致；P025复核PASS。实际standalone loader使用load作CODE base、忽略entry，不能将localentry当PA。P025修复通用检查器把entry等同load的问题，并加验签policy+required-conf-key补丁、signature-enabled U-Boot Host完整构建/封装；AMP FIT与uboot package均未签名。官方amp GPT源不存在、实际boardpolicy未知，详见 [P025](P025_BOOT_LOAD_AND_VERIFY.md)。以下旧135256B/无entry描述仅为历史。

候选 `Image/amp.its` 的 `images/mcu`：`description="bus_mcu"`、`type="standalone"`、`arch="arm"`、`compression="none"`、`load=0x47800000`、SHA256；`configurations/conf:loadables="mcu"`，RSA2048 PSS `dev` signature node。**没有 `entry` 属性**。Host `mkimage -l` 显示 `Entry Point: unavailable`、`Sign value: unavailable`。U-Boot standalone handler 将 load/entry_point 交给 BL31 作为 M0 code window 的 **物理 base**；ELF `e_entry=0x141` 是 M0 Thumb reset handler 的 **M0 视图**，两者不能数字相等比较。M0 reset vector 在 M0 `0x0`，U-Boot remap 后读取。

本轮新派生 RTOS clean build 成功，Host 系统 `/usr/bin/mkimage` 生成 **非部署候选** `amp.img`；payload 135,256 B。`rttmcu.bin` 不在此 SCons/FIT 路径生成，RTOS source 的 `rtthread.bin` 才是 FIT payload。最终地址未选，未改 ITS/linker/cache decoded base；不可把旧 `0x47800000` FIT 当成最终设计。`hal_conf.h` 非 MOS 分支还把 `HAL_CACHE_DECODED_ADDR_BASE` 写死为该值，未来任何移动必须一起审查。

`images.verify=1` 不等于已证明板上 secure policy：实际 U-Boot 验签配置/公钥未知。不得为通过验签关闭 verified boot；当前没有合法签名材料。最终 FIT load/entry/签名/读取位置/分区都 BLOCKED。
