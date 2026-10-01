# Gate 6 — Kconfig 与 I2C 配置来源

原固定 RTOS 的 `.config` 开 `I2C3/6/7/8`，但 `board/evb/defconfig` 和 `rtconfig.h` 只有 `I2C7`；原 SCons 真正只编译 I2C7 路径。原因是 `.config` 曾被另一配置过程更新、没有重新生成头文件；仓库历史未提供确切时间/操作者，不能推断四条 I2C 都实际生效。

真实流程：`board/evb/defconfig` 是可复现的 board 默认配置；复制为 `.config`，`scons --useconfig=.config` 调 `tools/menuconfig.py:mk_rtconfig()` 生成 `rtconfig.h`；`tools/building.py:PrepareBuilding()` **只解析 `rtconfig.h`** 决定 SConscript 依赖。`gcc_link.ld.S` 预处理也包含 `rtconfig.h`。因此实际构建 source of truth 是生成后的 **`rtconfig.h`**，长期配置来源为 **defconfig → .config → rtconfig.h**。不能手工在 `.config` 和头文件各改一次来伪造一致。

本轮仅修改派生 `board/evb/defconfig`，关闭 I2C/PWM/SPI/benchmark/common tests；随后 `cp board/evb/defconfig .config`、`scons --useconfig=.config` 生成头文件，再 clean build。最终三个文件一致：I2C3/6/7/8 **全部未启用**。`board/evb/iomux.c` 去掉无条件 `gpio1_d5_iomux_config()`，且 `RT_USING_I2C` false，不触发 I2C7 m1 pinmux。未来 MPU6050 仍不选总线。

当前 Linux DTB 静态状态：I2C3 `okay`，其下 RTC `hym8563@51`；I2C6/7/8 `disabled`。I2C3 不可直接抢占。I2C6/7/8 只是后续资源评估候选，需要逐项核 pinctrl、clock/reset/外接器件与 Linux overlay。当前 echo 固件不启用任何 I2C。
