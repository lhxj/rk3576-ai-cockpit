# I2C9 BSP 派生修复（2026-10-05，Host）

状态：`I2C9_ADAPTER_HOST_TESTED`。四项已识别源码问题已修复及 Host 回归，
不等于 `HOST_PASS` 传感器业务完成、ownership 闭合或可部署固件。
`WIRING_NOT_READY / OWNERSHIP_NOT_CLOSED` 与两个用户门保持。

## 补丁与边界

`patches/mpu6050/0001-i2c9-deferred-held-clock.patch` 应用于冻结 RTOS
3a39b0f4f5eb25631d05b4ffd88746ce9b1b0cdb + 0010..0015/v5，HAL 为
bc99978c1a030ad79610e89a5780dcd0ee3bb1f2。四个文件保留 Apache-2.0 声明。
原 RTOS/HAL/最小 AMP/SI_HEALTH 源码与产物不改；完整 SDK 副本仅在忽略目录。

- `MPU_SENSOR_V1_I2C9_OWNERSHIP` 限制只允许 I2C9、CRU，拒绝其他 I2C 与 PM。
- PREV 初始化跳过 I2C9，不读 MMIO/clock、不注册 IRQ/设备。
  `rockchip_i2c9_resource_ready(timeout_ticks)` 仅供将来的控制 worker 在经批准的
  Linux clocks/pins/access preflight 后调用；该函数本身不是权限或 READY 证明。
  WAIT -> INITIALIZING -> READY/FAILED；并发调用返回 BUSY，成功幂等，失败锁存到冷启动。
- Linux-held clock 路径不调用 RTOS gate enable/disable，严格要求源 clock 24MHz。
  `HAL_I2C_Init` 及 bus register 错误传播，失败不会 READY，IRQ 保持 mask。
- evb 通用 I2C 分支不再额外配置 I2C7；目标 pinmux 仍交 Linux 归属方案。
- bus.timeout 使用 RT ticks；整个 master_xfer 共享最多 20ms tick 预算。
  无符号 tick 差支持 rollover，configure/start 后重新计算剩余预算，耗尽不启动下一消息。
  超时 ForceStop，最后 Close/mask；NACK 返回错误，不冒称成功短读。

IRQ 次序是 mask -> configure -> completion/error/active -> HAL Transfer -> unmask -> wait。
冻结 HAL `lib/hal/src/hal_i2c.c:682` 的 Transfer 调用 I2C_Start，后者在第 186 行
先 I2C_CleanIPD 再启用控制器中断；Close 第 728 行关闭 IEN。
整个 configure/start 窗口保持 IRQ mask，避免旧 pending 在新 completion 上报成功。
active 门忽略完成后的重复/迟到 IRQ；completion 在启动前重置，立即完成不会被覆写。
这仅是源码与 Fake 证明，尚无 INTMUX/硬件 IRQ 证据。

## 验证

```sh
python3 tests/mpu6050/run_i2c9_driver_tests.py \
  --rtos /home/ywx/rk3576-work/worktrees/rk3576-amp-platform/rtos
python3 scripts/dev/mpu_i2c9_native_build.py \
  --rtos /home/ywx/rk3576-work/worktrees/rk3576-amp-platform/rtos \
  --hal /home/ywx/rk3576-work/worktrees/rk3576-amp-platform/p023-hal \
  --toolchain /home/ywx/.local/toolchains/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi/bin \
  --output artifacts/local/mpu-i2c9-native-NEW
bash scripts/dev/host_ci.sh
```

实际 patched drv_i2c.c 在 100Hz/1000Hz 下以 ASan/UBSan 执行通过：PREV 零访问、
ready 门、无效预算/ISR 拒绝、初始化中重入、成功幂等、HAL/注册/clock 失败锁存、
无 gate 调用、pending 清除次序、立即/重复/迟到 IRQ、NACK、超时、恢复成功、
configure 耗时、两消息共享预算及 tick rollover。
实际 patched board iomux.c 以 Fake pinmux 执行确认零 I2C7/传感器 mux。
新 Kconfig 两个条目的真实文本在依赖 fixture 中验证 I2C/CRU 依赖。
Host CI 31/31 CTest、47/47 Python、5/5 withdrawal 与 shell syntax 通过。

原生 `scons --useconfig=.config` exit 0，实际走 vendor tools/building.py:321-325
的 mk_rtconfig，生成配置保留 I2C9/ownership 且没有其他 I2C；随后原生 SCons 链接通过。
诊断配置加 `-Wl,-u,rockchip_i2c9_resource_ready` 保留 API，否则无业务调用会被 gc 移除。
未调用 API，没有 MPU6050 驱动/task/endpoint，也没有新 FIT/DT/KO。

全图 Kconfiglib 解析在原 baseline 和 patched SDK 都因已有 RT_USING_LEDS 自依赖循环
失败；不修改无关 LED 来规避，不声称全量配置 solver 已通过。
原生 linker 原有 RWX LOAD warning 仍在。构建命令和产物完整 hash 在
[I2C9_ADAPTER_BUILD.json](I2C9_ADAPTER_BUILD.json)。ELF text 136068/data 2648，
BSS 汇总含共享内存及 linker heap 预留，不能把 4588032 字节当实际 M0 静态分配。
DDR bss_end 0x252dc，heap 0x252dc..0x7fc00（370980 字节），main stack 1024，
RT main 2048/finsh 4096，512KiB DDR 布局未改。运行期 stack/heap 高水位未测。

日志仅本地：artifacts/local/mpu-i2c9-fix/{driver-tests,host-ci,full-kconfig-baseline,
full-kconfig-patched}.log 与 artifacts/local/mpu-i2c9-native-isolated/native-*.log。

## 仍需闭合

M0 I2C9 firewall/访问、Linux 持续时钟引用与 clock parent、reset/PD/INTMUX、运行
pinctrl/hog 均需获批前置方案及实板证据。当前 API 只提供显式门，不能凭调用成功
证明 Linux 实际遵守独占。全量传感器与 RPMsg/Core/Qt 仍待实施及独立测试。
信号线保持未连接；本轮无板端命令、MMIO、I2C、部署、KO、boot 或重启操作。

## Review 后补齐构建隔离与来源门

测试/原生构建共用 scripts/dev/mpu_i2c9_sources.py，读取
patches/mpu6050/source-inputs.json 精确核验四个补丁输入、原 .config、配置生成入口、
RTOS HEAD 与 HAL HEAD/I2C/CRU/RK3576 source hash；修改的 source 先拒绝再复制，
patch 使用 --fuzz=0。有效 RTOS HEAD 为 1d0de06c394f89be35a4b6966e766b56f035c19d，
它是既有派生目录的 Git HEAD，工作树冻结 0010..0015 输入另由文件 hash 固定。

构建显式 RTT_ROOT=新副本/rtos、RTT_CC=gcc、RTT_EXEC_PATH=指定工具链，
移除继承 BSP_ROOT/PKGS_ROOT，检查 HAL symlink 最终 resolve 为新副本/hal，
所有复制 SDK symlink 均不能逃逸输出树。没有写 frozen SDK。
5 个独立 Python 回归验证 source hash/RTOS/HAL identity 拒绝、恶意外部 RTT_ROOT/RTT_CC
被隔离及 symlink 逃逸拒绝；已放 tests/python 供 host_ci 自动发现。
在外部 RTT_ROOT 指向 frozen RTOS、RTT_CC=hostile 的条件下 fresh native 重建通过，
输入 source/HEAD 再次核验不变，实际 build env 与新 artifact hash 已更新到 JSON。
本轮仅重跑有影响的 driver sanitizer/5 个预检/native 回归，原 host_ci 结果仍保留。

## 主控最终审核

2026-10-05 主控独立复核实际 driver ASan/UBSan 100/1000Hz、board fixture、5/5 构建
安全预检、native 各项 artifact hash、source preflight 与 diff check，均通过。
最终主控重跑 host_ci exit 0：31/31 CTest、52/52 Python、5/5 withdrawal。
`artifacts/local/mpu-i2c9-fix/reviewer-host-ci.log` 为本地审核证据。
此前 47/47 是新增 5 个预检前的结果，保留其当时事实，不改写历史记录。
