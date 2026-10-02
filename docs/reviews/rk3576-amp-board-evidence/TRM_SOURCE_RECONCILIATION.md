# 用户提供的两份 TRM：原文核验（2026-10-02）

已直接读取、全文检索并检视相关 PDF 页面，网页聊天报告仅作为检索线索。本轮未访问实体板。结论仍为 **C. HOST_BUILD_PASS**。

## 文件与方法

| 文件 | 页数 | SHA256 |
| --- | ---: | --- |
| `C:\Users\27432\Desktop\Rockchip_RK3576_TRM_Part1_V1.2_20240624.pdf` | 1412 | `6094ae5874d8494e73fa363d9cf35dd65acbd54a9a9d633b1ba5e4bea289f0a8` |
| `C:\Users\27432\Desktop\Rockchip_RK3576_TRM_Part2_V1.2_20240624.pdf` | 2969 | `8a16671d228ee206012d9a3b8a976aec113f7ea2ea45fcabe8565b2d85e4f6cf` |

使用 PDFium 提取完整文本；检索目标寄存器名称、物理地址、SYS_SGRF/FW_SYSSGRF、cache 初始化与 coherency 描述；视觉核对相关表格。本文引用的 PDF 页码与印刷页码一致。原 PDF、提取文本与渲染页不提交仓库，Host 中间产物位于 `artifacts/local/trm-upload-reconciliation/`。

## 已核实的关键证据

| 原文位置 | 核实内容 | 证据等级与边界 |
| --- | --- | --- |
| Part1 p15，§1.1，表 1-1 | SYS_SGRF 基址 `0x26004000`、大小 4 KiB；FW_SYSSGRF 基址 `0x26005000` | **SOURCE_VERIFIED**；无 CON16/17 offset/detail |
| Part1 p762，§8.6.2，表 8-6 | BUS MCU code/RAM 窗口使用 CON16/17 `[31:10]`；基址低 10 位为零；两窗口各 512 MiB；remap 变化需 MCU soft reset 才生效 | **SOURCE_VERIFIED**；没有给出 reset value、寄存器 Attr 或具体采样边沿 |
| 固定 HAL `277de3fd…`，`lib/CMSIS/Device/RK3576/Include/soc.h:651-652` | BUS MCU getter 地址为 `0x26004000 + 0x20000000 + 0x60/0x64`，注释指明 CON16/17 | 对固定候选源码 **SOURCE_VERIFIED**；offset 来源是 HAL，不是 TRM 寄存器明细表 |
| Part1 p618，§6.3.2，表 6-1 延续页 | SYS_SGRF 列在 PD_SECURE 虚拟电源域 | **SOURCE_VERIFIED**；这是电源域归属，不是 non-secure 访问许可表 |
| Part1 p750，§8.1，表 8-1 | BUS MCU 配置列出 16 KB unified I/D cache；2-way、32-byte line、支持 WT/WB 和 maintenance | TRM 设计配置 **SOURCE_VERIFIED**；概述有实际产品功能配置的厂商确认提示，不能视为本板 cache 状态实测 |
| Part1 p765，§8.6.4–8.6.5 | reset 后 cache bypass；软件初始化并清 bypass 后启用；支持 clean/invalidate/flush | **SOURCE_VERIFIED**；不能由复位默认状态推断候选启动代码没有启用 cache |
| Part1 p766，§8.6.8，表 8-14 | BUS MCU 非缓存 peripheral space 由 CON14/15 配置起止地址 | **SOURCE_VERIFIED**；这些寄存器的当前值、范围边界解释与实际生效状态未确认 |
| Part1 p772，§11.1.1 | CCI500 明确列 A 核两 cluster 与 PCIe/GMAC/SATA/USB 的一致性功能 | **SOURCE_VERIFIED**；未给 BUS MCU 与 A 核端到端 DDR coherency 保证 |
| Part1 p770，§10.1 | SHRM 的描述针对 System SRAM，未承诺跨 master 的一致性检查 | **SOURCE_VERIFIED**；不能移作 DDR sharing 的确切属性 |
| Part2 p1368，§11.3.9.1 | SYS_SGRF_SOC_CON1 用于 VOP security port 控制 | **SOURCE_VERIFIED**；不是 CON16/17 访问属性 |

两份文件中 CON16/17 名称仅匹配 Part1 p762；`0x26004060/64` 均无文字匹配。所有 SYS_SGRF 匹配页已核查：Part1 p15/241/244/618/762/766，Part2 p1368。p241/244 是其 clock/reset 控制，不是 SYS_SGRF 自身寄存器明细；未取得 CON16/17 的软件读属性、复位值或读副作用说明。缺少文字匹配是检索结果，不是硬件语义保证。

## 地址证据的正确归属

**仅凭 TRM，offset `0x60/0x64` 未闭合；结合固定 HAL，源码定义的系统物理地址可以计算。**

TRM 外设公式 `System_PA = M0_addr - 0x40000000 + 0x20000000`，配合 HAL 的 M0 getter 地址 `0x46004060/64`，得到系统 PA `0x26004060/64`。这不是依据 CON 编号等间距猜测，而是 HAL 明确宏与 TRM 地址译码公式的组合。它仍不证明当前安全域允许 Linux 访问。

令 `B16 = CON16 & 0xfffffc00`、`B17 = CON17 & 0xfffffc00`：

- code：`System_PA = B16 + M0_addr`。
- RAM 窗口：`System_PA = B17 + M0_addr - 0x20000000`。
- 候选 vring0：`B17 + 0x07d00000`；vring1：`B17 + 0x07d08000`。

TRM 表把第二窗口称为 On chip RAM / Normal WBWA；项目的 shared DDR 是对该机制的候选用途，不能写成 TRM 原文名称。**B17 未知，仍不填数值 Linux PA。** 即使未来取得寄存器当前值，还需核对其与最近一次 MCU soft reset 的配置时序，避免把未生效的新配置当作有效映射。

## 对当前决策的影响

1. 原地址依据需明确写为 **TRM 基址/映射 + 固定 HAL offset**；网页报告的 TRM-only 缺口成立，源码组合证据仍保留。
2. cache 是 MCU 子系统真实设计的一部分；硬件 coherent sharing 没有文档保证。最小 echo 的候选处理方向应是证实完整共享路径 bypass/uncached，或实现正确的 maintenance；空 cache hook 与 `dsb` 不能完成 cache 可见性证明。
3. reset bypass 与 CON14/15 提供了源码调查方向，没有证明现用固件共享区已 uncached。缓存方案仍 **UNVERIFIED**。
4. TRM 不提供当前 CON17 实值，也不能把 Linux 的 SIGBUS 单独归因于 read-clear 或某个 firewall。已实际读取路径的失败以 [运行时诊断](CON16_CON17_RUNTIME_DIAGNOSTICS.md) 为准，不再靠缺少 read attribute 重复推断。
