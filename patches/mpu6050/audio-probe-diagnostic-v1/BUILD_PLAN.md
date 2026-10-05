# ES8323初始读取诊断：待审核构建方案

状态L0_SOURCE_ONLY / NOT_BUILT / NOT_DEPLOYED。只有负返回日志增加ret、adapter号和addr；保留`i2c recv Failed`串、原单次i2c_master_recv和原return ret，无clock get/enable、retry、读写事务或功能改变。此日志仅确定负errno，不能单独证明物理NACK原因或MCLK假说。

输入为vendor kernel commit521833e2d28decbd6473d5717f1f96cc4108e208，精确codec源hash及patch/hash见SOURCE.json。冻结p026配置58c9891ae953922ced41508ad4e1f91a480d1100d42c012b6be8038d1ddd9436；冻结Module.symvers bebc886c6f4ef09ae0d38fc077cb25877f1d70c32e760736814bd032d596b19c仅输入身份/历史参考，不复用于新KO。

审核后在本任务artifacts/local/audio-probe-diagnostic-kernel-v1创建全新src/build/stage目录（存在则拒绝），只读复制精确base，不改SDK；源hash先检查，patch -p1 --fuzz=0。复制冻结p026 .config，用官方scripts/config设置独立LOCALVERSION `-rk3576-audioprobe-d1`并禁LOCALVERSION_AUTO；olddefconfig差异只允许release字段，不新增功能。沿已用主控scripts/amp/p026_build_isolated_kernel.sh原生入口：ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-，保留其p023-standard-tools环境，但src/O/INSTALL_MOD_PATH全部指向本新目录，不能直接运行原硬编码脚本。构建Image、modules、原board DT；记录gcc版本、完整commands/exit/log、config diff、kernel.release。

新build必须保留生成include/generated、arch/arm64/include/generated、scripts、Makefile、.config、Module.symvers、System.map和模块集合，并独立headers/staging清单。rk3576_sensor源码从apps/rpmsg_srv/kernel精确copy；health源码/Makefile从已审核system-board-validation-20261004/health-ko-v2/module精确copy。两KO均用同新src/O和实际新Module.symvers重新Kbuild；原build_sensor_ko.py固定旧prepared/vermagic不能原样用于新release。验证modinfo实际新release、架构、undefined imports在新symvers导出；CONFIG_MODVERSIONS保持原值，若启用必须核CRC，禁止以force加载绕过。

modules_install只新Host stage，depmod只该stage；新module archive不得带build/source逃逸symlink。若独立initrd需要则仅用该release模块集合生成，禁止混旧p026/stock模块。manifest记录base/patch/source/config/build tools、Image/initrd/DT/System.map/symvers/headers/module archive/两KO所有实际bytes+SHA，未实际构建的hash写NOT_BUILT。DT仅构建来源校验，不改冻结sensor ownership DT；M0/FIT/boot frozen/controllers/native app不动。后续启动script若需要新kernel/新KO应另审identity+hash整套，不能让旧manual6自动使用新release。

恢复组合始终为原默认6.1.99-rk3576或完整保留的冻结p026组合，绝不覆盖它们。部署/新启动窗口需root另行审阅完整新manifest；本计划无板操作。诊断窗口只观察启动错误errno，若codec仍缺失保全一次bounded UART/身份/绑定证据即停止，不同组合连续boots、不reprobe、不进入Qt/采样；正常root清理关机后恢复默认。无正常shutdown则如实记录。MCLK初次ACK必要性尚未有厂家primary资料证实，不根据该假说实施clock修复。

可执行入口：`python3 scripts/dev/build_audio_probe_diagnostic.py --output artifacts/local/audio-probe-diagnostic-kernel-v1`（root最终review后才运行）；`--check-only`只核输入和tiny scratch patch，不make。源码copy上限3GiB/150000 files/300s、16GiB最低空闲磁盘；内核make1800s、全command预算2400s、各command log2MiB/合计8MiB超限立即终止owned process group。6项production Host测试和实际check-only通过；matching generated header逐文件hash清单另存。initrd尚未构建因此manifest deployable=false；不能拿该Host结果直接上板或混旧initrd/module集合。

复核补强：patch前fresh源码复制逐文件流式SHA核对copy一致，source-before-patch-files.json<=32MiB及catalog SHA写manifest；它是实际树身份，不以commit名代替。所有hash为1MiB流式块，headers逐文件copy/hash及模块/最终artifacts后处理循环均检查whole deadline，失败仍写FAILED_OR_INCOMPLETE manifest。新增后处理过期负例及禁止read_bytes的大文件hash fixture。
