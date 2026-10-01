# WSL ↔ 实板工作流

## 已有连接

WSL内 `ssh lubancat`，默认用户cat，主机与密钥保留在用户自己的`~/.ssh`。
本包不会覆盖该文件或复制私钥。保持主机指纹校验，不能使用StrictHostKeyChecking=no。
手机热点DHCP可能变更；SSH超时时先核对同热点和当前地址，不先改sshd。
GitHub登录、板端SSH公钥、sudo是三类不同权限。

## 附带的两个只读脚本

```bash
bash scripts/board/probe.sh
bash scripts/board/inventory.sh
```

前者查看身份/型号/内核，不写板端；后者将只读盘点输出保存到
`artifacts/local/`（默认Git忽略、目录权限限制）。所有命令固定，不接受任意远端脚本参数。
脚本不执行sudo，不安装软件，不开始视频/音频采集，不改网络或启动配置。
日志访问失败时记录SKIP，不为了盘点自动提权。
本包生成环境未执行这两条的真实SSH连接，必须在用户WSL上运行验证。

## 设备独占

两脚本共用 `${XDG_RUNTIME_DIR:-$HOME/.cache}/rk3576-ai-cockpit-locks/lubancat.lock`。
未来L2测试wrapper也必须用同一锁。锁用于跨worktree的本机协作；
手工直连、另一台电脑或绕过脚本的进程不受它约束，主控仍负责协调。

## 设备测试前

读取运行配置/当前真实节点；检查是否有现有用户进程占用。
主控核验目标是CAM0及已知正常排线；不通过video-camera0名字推断物理端口。
为测试规定：输入、设备、时长、退出码、stdout/stderr上限、内核错误采样、恢复方式。
Camera/ALSA/NPU本轮只允许一个测试。采流参数不主动增加分辨率/帧率。

计数与timeout不能确保内核故障下退出；需要保留串口并在错误风暴时停止测试。
不要在错误风暴里持续执行全量`dmesg -w`；不要无限重试或清空日志。
输出有限的tail片段，同时保留首次错误前的启动序列。
涉及dmesg console等级修改须说明这是临时诊断变化，不是问题已修复。

## 部署

提议目录`/home/cat/cockpit/releases/<commit>/`，由后续任务实现。
不把x86_64 host产物发给aarch64板端；先核对ELF架构、动态依赖、sysroot。
不使用破坏性的rsync --delete；不要覆盖板端唯一已知正常程序。
首次部署和启动由用户批准；不自动注册systemd常驻服务。

## 高风险变更

修改 /boot/uEnv、软链接、DTB、Kernel、U-Boot、AMP核/内存、时钟/电源、
网络/权限或系统包，必须逐次批准并准备恢复方案。
特别注意uEnv历史上是软链接；GNU sed -i默认行为可能改变链接结构。
准备补丁时先确认真实目标并备份，不给一条命令盲改远端boot。

## 证据

测试报告包含commit、board kernel/DTB、映射、命令、起止时间、退出码、结果与残留状态。
原始照片/录音/IP/SSID/主机序列号不自动推到GitHub，提交脱敏摘要。
