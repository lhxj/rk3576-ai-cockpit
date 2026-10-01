# 来源与核验边界

## A. 本会话事实（本包最主要来源）

用户从实板贴出的Linux/ALSA/PCIe/Wi-Fi/V4L2/media输出，以及后续确认：
Camera A/B交叉测试、BTB缺件、MPU6050已有、LED/按键/蜂鸣器用软件模拟、
HDMI显示与触控可用、WSL Ubuntu22.04、GitHub lhxj登录、SSH别名配置完成。
原始私密日志不随包分发。docs/STATUS和bringup/KNOWN_FACTS保留摘要及未知项。

## B. 本轮核对的官方工具文档（2026-10-01）

[O1] OpenAI Configuration Reference：
https://learn.chatgpt.com/docs/config-file/config-reference

[O2] OpenAI Subagents：
https://learn.chatgpt.com/docs/agent-configuration/subagents

[O3] OpenAI Config Basics：
https://learn.chatgpt.com/docs/config-file/config-basic

[O4] OpenAI AGENTS.md：
https://learn.chatgpt.com/docs/agent-configuration/agents-md

[G1] GitHub CLI gh repo create：
https://cli.github.com/manual/gh_repo_create

[G2] GitHub CLI gh issue create：
https://cli.github.com/manual/gh_issue_create

[G3] actions/checkout v4.2.2官方发布页（CI固定其commit，而非声称最新版）：
https://github.com/actions/checkout/releases/tag/v4.2.2

[C1] CMake 3.22 Presets：
https://cmake.org/cmake/help/v3.22/manual/cmake-presets.7.html

工具文档指导配置，不证明用户已安装对应Codex版本、拥有某模型、已建GitHub仓库、
或GitHub Actions已实际运行。模型名默认未固定。

## C. 用户指定的项目参考（本轮未重新审计完整源码）

[R1] 野火RK3576快速入门：
https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/README.html

[R2] 野火RK3576摄像头：
https://doc.embedfire.com/linux/rk3576/quick_start/zh/latest/doc/camera/camera.html

[R3] Qt车载应用参考：
https://blog.csdn.net/black_sneak/article/details/131889750

[R4] 语音项目参考：
https://github.com/superxiaobai-1/LLM_Voice_Flow

[R5] Rockchip Linux内核：
https://github.com/rockchip-linux/kernel

[R6] Rockchip RKNN / RKLLM：
https://github.com/airockchip/rknn-toolkit2
https://github.com/airockchip/rknn-llm

[R7] Rockchip MPP / RGA：
https://github.com/rockchip-linux/mpp
https://github.com/airockchip/librga

以上只是查阅入口；在实施任务中锁定用户实际SDK/Runtime版本，不把latest直接当兼容基线。
