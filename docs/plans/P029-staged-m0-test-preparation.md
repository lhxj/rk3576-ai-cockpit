# P029：分阶段 M0 测试包准备

2026-10-03，基线 `agent/amp-platform-closure / af08db1`。

## 目标与已有证据

用户要求开始准备：在已实测 P028 U-Boot 上，形成独立的 Linux 保留内存检查、单次 M0 启动及 RPMsg echo 测试物料。P028 完整8MiB读回、proper策略0、原Linux启动、Ctrl+C/help/boot均PASS；help只证明命令注册。原件恢复实测PASS。实际MCU setter、有效mapping/reset/cache和RPMsg仍未验证，整体C。

## 范围与权限

- 主控负责打包、分阶段脚本、manifest、验收/回滚指南和最终审核。
- 用户此前明确授权的一个子代理仅独立只读审查源码流程、保护和观测缺口；新报告放忽略目录，不改主控文件，不接触板端。
- 本轮仅Host准备，不上传/运行板端程序、不写boot/eMMC/GPT、不重启、不接线、不启动M0。后续L3操作按AGENTS.md形成具体范围后批准。
- 保留固定reference、旧P026撤回物料、已实测P028镜像和默认factory启动文件；不为打包重新刷U-Boot或重设计地址。

## 一个里程碑，三项任务

1. 核P026配套kernel/modules/initrd/M0/FIT与P028编入contract的一致性、来源、哈希、地址边界；选择分阶段测试对象，识别缺少的运行输出。
2. 形成独立passive文件包、阶段A（不调用M0）与阶段B（明确单次启动）流程；默认启动入口保留，失败后完全断电回默认。生成机器manifest、明确安装范围及恢复对象，不使用旧withdrawn writer。
3. 主控审核子代理结论，运行必要Host检查/实际解析与fault测试，交付桌面物料和简明指南；更新STATUS/SUMMARY，commit/push既有Draft PR，不merge，不升级D。

## 验收与失败处理

包中每个可执行/启动对象有size/SHA/source，禁止混入旧U-Boot；实际Image/initrd/DT/FIT边界由脚本核算，Linux no-map与模块release匹配。阶段A无M0命令；阶段B只有一次显式入口，检查失败不继续boot旧DT或在同次启动重试。超时/异常停止，保留有限日志，恢复先完整断电；default factory文件不需修改则不增加恢复动作。

若发现源码/物料无法提供必要mapping/cache观测，先完成可独立审查的Host修补或记录具体阻断，不把setter返回值/帮助文本当作有效运行映射。所有现场结果保持UNVERIFIED；Host PASS不作为M0/D证据。

## 执行与交接

开始：工作区干净，最新af08db1；一个既有子代理已接只读审查任务。主控读取工作规则及P026/P028证据。本轮结果、实际命令和剩余门在结束前更新。


## 实际结果（2026-10-03）

三项Host任务完成；桌面最终v4交付21文件SHA逐项一致，无U-Boot镜像。fresh M0 v3 build零warning、ELF向量/LOAD边界、FIT外部payload、三份实际DT/no-map/geometry核验PASS。67包/脚本 +14实际M0 C +6snapshot故障项PASS；实际P028 C/libfdt与full CI PASS，最终/memory coverage仍需目标runtime fixup，不虚记板端PASS。独立子代理审查找出的非法指针free及安装器TOCTOU/根tar问题由主控修复，复审PASS。细节、准确命令/identity及限度见P029_HOST_PREPARATION_REVIEW和P029_PACKET_MANIFEST。

没有SSH/板端文件写入/MMIO/KO/M0/重启/接线。canonical final fields和D门保持未开放，旧P026撤回状态保留。下一步A的被动新增文件/冷启动需要AGENTS.md L3明确批准；用户唯一Debug USB-TTL足够A，B/C UART5日志采集仍待安排，不假设第二适配器。实际有效映射/cache/MCU setter/RPMsg与新回滚回归保持UNVERIFIED。
