# 二、实验环境

### 2.1 软硬件基础设施配置

| 配置项 | 环境规范 / 实测版本 | 作用说明 |
| :--- | :--- | :--- |
| **宿主操作系统** | Ubuntu 22.04 LTS (WSL2 / Linux Kernel 5.15+) | 基础交叉编译构建与内核运行宿主环境 |
| **架构模拟器** | QEMU emulator version 7.0.0 (`qemu-system-riscv64`) | 模拟 RISC-V 64 Virt 虚拟硬件开发板平台 |
| **交叉编译工具链** | `riscv64-unknown-elf-gcc` (11.4.0+) | 编译 S 态裸机内核与引导汇编源码 |
| **底层运行时固件** | OpenSBI v1.0 (QEMU 7.0 内置) | 提供 M 态运行时支持及 SBI 服务调用接口 |
| **底层调试器** | `riscv64-unknown-elf-gdb` / `gdb-multiarch` | 裸机断点挂载、寄存器状态检查及指令级单步跟踪 |

### 2.2 团队协同与 AI 研发工具矩阵

团队成员在实验过程中严格遵循“人类主导系统架构规划与时序审计，AI 工具辅助逻辑验证与代码排障”的协作准则，工具与模型使用分布如下：

| 成员 | AI 编程工具 | 底层模型 | 备注 / 核心负责场景 |
| :--- | :--- | :--- | :--- |
| **张远 (openfar, 2411264)** | Claude Code（终端 Agent）/ Far CLI | Claude 3.5 Sonnet | T0 阶段 Makefile 改造重构，T2 阶段 GDB 启动流程跟踪与时序推演 |
| **李云鹏 (lyp, 2414099)** | VS Code (GitHub Copilot / Cursor) | GPT-4o / Claude 3.5 Sonnet | T1/T3/T4 阶段内核 entry.S 汇编分析、模块理解与知识点对照 |
| **刘昀皓 (nagilix, 2413074)** | VS Code (WSL: Ubuntu) + Web 协同交互 | Claude 3.5 Sonnet / DeepSeek-R1 | T6 阶段引导时序对比架构推演、实验环境核验与非代码交付物工程化落地 |

**说明：**
- **AI 编程工具**：指各成员具体使用的终端智能体工具、编辑器插件或桌面交互环境。
- **底层模型**：指工具调用的大语言模型及具体版本。