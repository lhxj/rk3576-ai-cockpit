# 构建说明

## 目前真正实现的内容

只实现一个没有硬件访问的C++17基础库与host烟测程序。
apps里的服务目前是模块契约README，不是伪造的可运行服务。
没有把占位函数返回成功当成Camera / RPMsg / AI能力。

## Host

```bash
cmake --preset host-debug
cmake --build --preset host-debug --parallel 2
ctest --preset host-debug
python3 -m unittest discover -s tests/python -v
```

`bash scripts/dev/host_ci.sh`执行同样的构建/测试以及Bash语法检查。
使用CMakePresets v3，最低CMake 3.21；可兼容用户Ubuntu22.04的常见CMake版本。
不需要Qt、RKNN、MPP或Python venv，不访问网络。
缺少基础工具时可人工选择安装：

```bash
sudo apt update
sudo apt install build-essential cmake python3
```

这是主机环境修改，需要由用户执行或明确批准。不要在开发板批量照搬SDK依赖。

## 交叉编译模板

`cmake/toolchains/aarch64-linux-gnu.cmake`仅提供Linux AArch64模板。
必须先确认SDK工具链与板端sysroot，不混用WSL的x86_64库。
在本次打包检查中没有运行交叉编译，也没有把host二进制上传开发板。

```bash
cmake -S . -B build/aarch64 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake -DCOCKPIT_TARGET_SYSROOT=/absolute/path/to/verified/sysroot
cmake --build build/aarch64 --parallel 2
```

不要照抄占位sysroot路径。未来Qt/ALSA/ZeroMQ/MPP/RGA/NPU库必须按目标SDK实际版本接入。
Host CI成功不等于AArch64链接/运行通过。

## WSL空间

WSL `df` 的虚拟磁盘容量不能替代Windows宿主卷剩余空间。
下载大型SDK前同时核对宿主空间、下载大小、校验值及解压倍数；完整SDK不进Git。
完整rootfs构建/USB烧录的WSL可行性尚未验证，遇到环境问题单独调查。
