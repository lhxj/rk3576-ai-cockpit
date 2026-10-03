# P029 阶段B验签修复：可信control DT与签名AMP配置

2026-10-03。整体 **C. HOST_BUILD_PASS**，D关闭。新包只在Host生成，尚未写入或在目标硬件验签。原v6 B/C指南已暂停。

## 实际失败与根因

用户单次执行2738B v6 B脚本：`Verified-boot: 0` 后 `No RSA key found`、`Unable to verify required signature ... mcu`、`ret=-13`。COM6无输出。实际P028厂商 `image-sig.c` 在trusted control DT缺 `/signature` 时直接返回错误，无论policy0；AMP wrapper虽允许policy0/no-required-conf，后续真实image verifier仍拒绝。报错发生在reserve、copy、M0 release之前；不是UART故障或实际M0/cache/mapping测试。

验签实际取 `gd->ufdt_blob`（保留的U-Boot控制DT），不是传给loader的Linux B_DT。原板备份、实测P028读回及封装提取的控制DT均9795B/SHA `2eef029c7e599dc695b48f2bfdeabe0f2a87dda8588973750adaa8462edf9013`，无signature树。只给外部FIT/Linux DT增加key不解决当前入口。旧Host policy helper测试使用stub，遗漏了真实vendor image验证；本次补真实密码学与loadables验证，主控承担准备缺口。

用户随后完整冷断电默认回到桌面。主控持板锁、30秒外部界限，Host Python stdin只读核默认kernel6.1.99-rk3576、root p3/boot p2、P028完整8MiB/SHA9bd8cc…、原六个启动文件均一致。没有板端文件写入、M0、MMIO、模块或Agent重启。

## 修复范围

保留P028 `f8b4554` 的程序代码、配置、三段BL31、OP-TEE、loader元数据与原尾4MiB。仅在内置control DT增加单个开发测试RSA2048公钥 `key-p029dev`、`required="conf"`，然后重新封装两份2MiB slot。AMP config显式 `sign-images="loadables"`，签root/configurations/conf、mcu全部元数据及SHA256 hash；M0二进制、地址contract和配套Linux/DT不改。

所有 `FIT_SIGNATURE`、`images.verify=1`、required-conf/image与payload hash检查保留。私钥仅位于忽略Host目录、权限0600，不提交/不上传板/不复制桌面。此锚约束本开发AMP配置；没有烧OTP或更改ROM secure boot，外层loader仍是已实测policy0的SHA256容器，不能声称整条启动链已认证。

固定vendor `mkimage` 不支持系统工具的 `-B`，其 `IMAGE_ALIGN_SIZE=512`。使用 `-E -p0x1000` 在签名前外置payload，工具在签前设置root totalsize。扩展公钥DT会在FIT文件尾保留有界workspace；实际签名后的root totalsize与完整文件长度130560相等，不能签后截断/改属性。首轮v7工具选项失败与v8后置长度断言修正均留忽略记录，未接板执行；当前v8产物未在签后改认证内容。

## 当前具体身份

| 对象 | 长度 | SHA256 |
| --- | ---: | --- |
| 新完整uboot分区 | 8388608 | ef857b106476cb6fc775cd3837db3b5d254d6ff10103a9281669dffd7e67f1e4 |
| 新内置control DT | 12867 | 43164981efd8869432e42fab2411d99941d17476700f3fc03ab76948621049ce |
| 签名AMP FIT | 130560 | 331431fcf1f914c91efd54ffb52a50f9e0ef17ea9df7ba2ed8ef7c675ed6c041 |
| 不变M0 binary | 123336 | 40ccde0f4ec1c941fcc1acbd1437a8e4a193e89da2db0682981bd5ae224f1727 |
| 回滚到已实测P028 | 8388608 | 9bd8cc03f0ec7485a26a8dd94f5e18082ba58c2197f1299ce3dbf6d6971c6d5e |

Host产物：`artifacts/local/p029-signed-v8/`。生成器 [p029_sign_amp.py](../../../scripts/amp/p029_sign_amp.py) 固定来源/工具/contract和原8MiB哈希，只允许忽略Host输出，绝不写设备。具体审核状态、负例矩阵和证据哈希以 [机器记录](P029_SIGNED_REPAIR.json) 为准。

## 验证与边界

真实vendor `fit_check_sign -s` 已验证required-conf签名并走loadables payload验证；默认无 `-s` 的checker不加载纯MCU loadable，不能把其成功当固件hash验证。新增测试对实际签名包做错误key、签名缺失/损坏、固件/hash/地址/映射/totalsize篡改与坏DT，独立检查vendor硬件RSA参数算术。Host软件RSA不等于RK3576硬件crypto成功；目标冷启动/实际AMP签名入口仍待下一步。

新control DT与原factory脚本经过实际C/CRC/libfdt/default env Host回归；新FIT与实际B_DT经过既有reserve/preflight几何回归；full Host CI通过。代码/TA/硬件边界stub的测试只证明其明确覆盖范围，不冒充完整U-Boot或实板启动。

## 接下来

先按 [新U-Boot写入与回滚指南](P029_SIGNED_UBOOT_GUIDE.md) 完成**新8MiB写入→完整读回核对→默认Linux冷启动**。程序banner仍为f8b4554，不能仅靠banner证明新控制DT已写入。新硬件回归后再由主控暂存签名FIT及新单次B脚本，确认双串口采集后才重开B；旧B路径仍禁用。本轮不执行B/C或echo KO。
