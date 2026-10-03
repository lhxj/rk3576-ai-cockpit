# P029 阶段A：被动文件暂存与读回 PASS

2026-10-03，Host基线 `6524461`。用户先回复“批准阶段A”，随后确认连接恢复并“批准所有操作”。这份授权覆盖本任务后续必要操作；仍按A→B→C先验收技术条件，不再重复审批。当前只完成A的被动文件暂存，**新Linux冷启动尚UNVERIFIED，整体C，D关闭。**

**后续实测更新：** 用户首次1892B旧A脚本在`STOP length Image`退出，filesize=0x2930200。主控已修复前缀比较/清旧值错误，只新增script-fix-v5并读回PASS。首次失败保留；新脚本2368B尚待冷启动。见 [修复与实际结果](P029_STAGE_A_SCRIPT_FIX.md)。

## 本次实际执行

| 项目 | 结果 / 证据范围 |
| --- | --- |
| 当前板端身份 | BOARD_OBSERVED：LubanCat-3-v2，原kernel6.1.99-rk3576，cmdline U-Boot f8b4554，boot/root挂载分别p2/p3 |
| 当前完整8MiB U-Boot | 只读复制，Host SHA `9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e`，匹配已实测P028；没有U-Boot写入 |
| 原六启动文件 | 前后SHA与既定原件相同；前后记录逐字节相同。原Image/initrd/DT软链接和uEnv普通文件形式保留 |
| 新boot目录 | `/boot/amp-p029` 10文件，逐项SHA PASS；另留3个精确receipt，摘要与Host v4相同 |
| 新module目录 | 独立`6.1.99-rk3576-m0echo-p026`，268文件逐项SHA PASS（含255模块及索引文件），没有替换原release |
| 暂存流程 | 已审查Host脚本经SSH stdin一次执行；root0700私有RAM快照、固定checksum-list SHA及全部文件SHA后才持久写入；exit0/明确终标 |
| RAM清理 | 已知临时源包删除；最终只读`/dev/shm`目录盘点无amp-p029前缀目录，包含root快照前缀 |
| 空间 | 安装前boot67166208B可用；安装后16113664B，保留10MiB以上余量；root安装后20601696256B可用 |
| 运行状态 | 仍原Linux；没有Agent reboot/poweroff、M0/MMIO/KO启动或RPMsg启用 |

metadata核验针对六个路径自身，`stat`没有-L；对软链接证明链接自身属性/时间，内容SHA通过链接读取目标，不将其外推为目标权限/mtime或整个boot树所有文件metadata均一致。

## 真实失败与修复记录

1. 初次网络预检：旧alias及最近已知IP的WSL SSH、Windows TCP连接超时，未到认证/远端执行。用户随后确认同一IP恢复，重连成功；没有改SSH配置、host-key检查或扫描其他IP。
2. 首次未压缩RAM传输未完成（设置55s上限，外层WSL exit1，未保存该子进程精确exit）；`modules.tar`仅67901440B。只读盘点确认两个持久目录absent、无任务tar，安装器没执行。保留这次失败，不记作完整上传PASS。
3. 一次传输修复：在task-owned固定RAM目录核可信清单和全部现存成员是已知regular白名单后清理部分文件；同一v4包压缩到42126431B。新上传和解压成功，随后才执行一次安装器，没有重跑持久安装。解压后按原SHA清单验证，Host候选内容未更改。

## 核验与交接

完整Host CI通过（2 CTest、41 Python、5撤回writer检查）；独立子代理对保存记录终审PASS。主控已封存34项最终证据摘要，补齐终末RAM盘点与exit/phase记录。Host检查和暂存审查不替代阶段A冷启动实测。

主控持有既有WSL board_lock，所有SSH严格host-key、非交互sudo，命令有截止时间。Host重新hash当前U-Boot及原件/安装输出，实际boot10、module268及3 receipt逐项匹配；前后factory记录cmp PASS；最末RAM目录盘点为空。原始输出只留忽略目录，合计54497B；主仓只有脱敏结果/hash。Machine记录见 [P029_STAGE_A_EXECUTION.json](P029_STAGE_A_EXECUTION.json)。

本轮允许新增两条既定passive目录及RAM临时包，没有改factory默认入口、GPT、IDBlock、U-Boot、BL31、OTP、网络、sudoers或全局包/服务。源包cleanup不是清理持久测试目录。

**下一步由用户正常关机、完整断电后冷上电，通过Debug Ctrl+C执行A脚本。** 操作见 [阶段A现在执行的指南](P029_STAGE_A_COLD_BOOT_GUIDE.md)。登录后返回完整串口日志、uname/cmdline/DT tag，主控再读actual DT/no-map/iomem。A通过前保持B/C待执行；UART5单适配器采集问题仍待落实。

**STAGE_A_ASSET_STAGING=PASS；STAGE_A_LINUX_COLD_BOOT=UNVERIFIED；M0/RPMsg动态证据=UNVERIFIED；AMP=C. HOST_BUILD_PASS；D关闭。**
