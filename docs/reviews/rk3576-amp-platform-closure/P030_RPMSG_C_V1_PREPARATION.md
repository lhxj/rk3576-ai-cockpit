> **历史快照，非当前集成裁决。** 本文正文保留当时的源码分析、准备、失败或阶段结果。当前最小链已经真实双向通信并冷恢复，通过情况以 [AMP_RPMSG_INTEGRATION_TIP](../../amp/AMP_RPMSG_INTEGRATION_TIP.md) 和C实际执行记录为准。下文旧BLOCKED/未上板/尚缺证据仅适用于当时或相应旧产物；旧操作指南不作为本次执行入口。

# P030 新版C准备和暂存记录

2026-10-04。**B运行和完整冷恢复已通过；新版C准备、暂存及读回完成，可由用户新一次冷启动验证。C双向通信尚未运行，不标D。**

## 具体改动

完全复用已实测v5 signed FIT：131072B/SHA348109ebecda0f00314d4dbcaaae0f8a51e2d88ac714c03ab74c1966a3b19bd6，payload28691e30790f1585782f3528dbd611c1e8005f8790947462574d73b8a75b6124。现有control-key vendor验签再次通过；不读/复制私钥、不重建固件。

从已证明B脚本生成C，仅切换paired stage-C.dtb长度0x4b455、C参数与已有virtio bus dyndbg、新包签名FIT路径、banner，保留root override/单次loader/同一prepared Linux/所有长度保护；新SCRIPT3100B/0xc1c、单组件结束项0及两CRC正确。增加仅只读的preflight-c.py，加载KO前核实时C/DT/no-map/iomem/service/KO/ring/DMA证据，缺失停止。

Host实际C DT校验：三预留区no-map/无reusable，C tag，transport/mbox0/4 enabled，link4、vdev1、固定uncached pool与RX0/TX4引用；mcu-amp没有子节点，所以该driver无amp-cpus可再次启动M0；Linux UART5 driver disabled。KO40920B/hash与vermagic匹配，内核/DT/KO保持旧paired构建。固定driver源显示ring为64描述符/方向、512字节buffer，virtio DMA总空间65536B；actual ring dev_info和buffers dma dev_dbg可作为C日志取证。KO probe和callback完成HELLO/ACK/PING/PONG；本轮不加载它。

## 实际板端操作

持共享board_lock串行、有界45/60秒SSH和scp。先确认默认6.1.99-rk3576 #8/root p3/无stage/uboot149b1c5，RAM/new目录absent，既有sudo可用。归档先上传RAM再SHA核，helper在持久写前重核完整8MiB U-Boot f9beef07…72c0b3、factory六文件/链接、旧tree内容和metadata、paired assets/空间/目的地。只新增 `/boot/amp-p029/rpmsg-c-v1`，原子no-replace，结果 `P030_RPMSG_C_V1_PASSIVE_STAGE_READBACK_PASS`。

独立SHA256SUMS五项与回执读回通过，原默认入口/旧tree保持，helper RAM目录和上传archive清理。receipt SHA `4460df5a99a7b62a84c49e27ebcce51e3728b72c7e5b3216c87a3fd7f8fe586f`。Agent没有source/M0/KO/MMIO/重启，当前仍默认系统。原始传输凭据不写文件；安全安装输出留忽略目录及Windows。

## 当前限度

新preflight尚未在C实机运行；任何缺日志/不匹配都停止，不绕过。新C source会启用Linux transport并启动M0，必须新冷会话，失败不复跑。一次成功双向通信、M0实际共享指针/cache snapshots及完整冷恢复后才裁决C实际结果；不证明整个512MiB映射窗口、长期稳定性或缓存开启一致性。没有新增测试套件或跑全CI；本轮校验针对交付字节、来源与被动读回。
