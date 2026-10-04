# AMP 构建与历史工具入口

当前冻结链和Git tip见 [AMP_RPMSG_INTEGRATION_TIP](../../docs/amp/AMP_RPMSG_INTEGRATION_TIP.md)。本目录保留完整调查、故障修复、Host检查和阶段封装历史；文件存在不表示它是当前部署入口。

| 当前链的来源步骤 | 脚本 |
| --- | --- |
| paired kernel/modules/echo | p026_build_isolated_kernel.sh、p026_pair_initrd.py |
| DTS分阶段输入 | generate_host_proposal.py、p029_make_packet.py（保留原厂CAM0启动兼容输入） |
| v5 M0 build/sign/package | p030_prepare_tickdiag_v3.py，明确revision5；当前固件已实测，原字节冻结 |
| 正确 C 入口/只读preflight | p030_prepare_rpmsg_c_v1.py；scripts/board/p030_rpmsg_c_preflight.py |
| U-Boot来源 | build_uboot_factory_boot_host.sh、p030_package_final_fdt.py及集成累计source patch |

旧build_candidate_host、p026/p027旧包、旧B/C脚本、v1-v4诊断均不是当前交接入口；withdrawn脚本的拒绝保护保留。原full-deployment checker用于其历史schema/packet，不能据它的BLOCKED判定已测HELLO_ACK/PONG未发生。

没有新增业务代码、自动启动或板端部署。构建脚本依赖登记过的SDK/工具链/原件，不保证新机器一键复现；本轮不运行测试或重建。当前源码/产物hash与实际板端结果以tip的机器索引为准。
