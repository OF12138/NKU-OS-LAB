# 二、实验环境

### 2.1 软硬件基础设施配置

为了确保团队成员在物理机异构环境下的协同开发效率，同时保障自动化评测结果的严格可复现性，本组在区分成员本地独立开发环境的基础上，统一建立了严格的评测基准：

#### 1. 全组统一评测与仿真基准（Grade & Reference Baseline）

| 配置项 | 环境规范 / 统一版本 | 作用说明 |
| :--- | :--- | :--- |
| **宿主操作系统** | Ubuntu 24.04 LTS (WSL2 / Linux Kernel 5.15+) | 交叉编译构建、自动化评测与内核运行的基准宿主环境 |
| **基准架构模拟器** | QEMU emulator version 4.1.1 (`qemu-system-riscv64`) | 统一自源码编译（`--enable-debug` 保留调试符号），模拟 RISC-V 64 Virt 虚拟硬件平台 |
| **基准运行时固件** | OpenSBI v0.4 (QEMU 4.1.1 内置) | 提供 M 态运行时支持，采用 `fw_jump` 模式固定跳转至 0x80200000 引导内核 |
| **交叉编译工具链** | `riscv64-unknown-elf-gcc` (10.x～15.x) / `binutils` | 编译 S 态裸机内核、链接生成 ELF 及剥离裸二进制镜像 (`ucore.img`) |
| **底层调试器** | `riscv64-unknown-elf-gdb` / `gdb-multiarch` | 指令级单步跟踪、内存状态巡检、软硬件联合调试（含 QEMU 源码调试） |

**关于 QEMU 4.1.1 统一评测基准的必要性说明：**
框架的 [`code/Makefile`](file:///home/nagilix/Code/NKU-OS-LAB/code/Makefile) 与 [`code/tools/grade.sh`](file:///home/nagilix/Code/NKU-OS-LAB/code/tools/grade.sh) 均使用 `-device loader,file=bin/ucore.img,addr=0x80200000` 参数加载内核，其执行高度依赖 OpenSBI 在 `fw_jump` 模式下向 0x80200000 的确定性跳转。在 QEMU 8.x+ 等较新版本中，OpenSBI 切换为 `fw_dynamic` 模式，该参数会导致 OpenSBI 的 Next Address 被清零而无法启动内核。全组统一采用带调试信息的 QEMU 4.1.1（`~/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64`），并通过 `export QEMU=...` 导出，既保障了全组最终 `make grade` 评分与自测结果的绝对一致性，又可直接用于分支任务中 GDB 对 QEMU 模拟器源码的断点跟踪。

#### 2. 三位成员本地独立开发环境版本矩阵（Individual Local Environments）

在本地算法原型推导与前期调试阶段，三位成员分别保留了各自宿主机上的本地工具链与模拟器配置，实测版本分布如下：

| 成员 | 宿主系统 | 本地 QEMU 版本 | 本地 OpenSBI 版本 | 本地 GCC 工具链版本 |
| :--- | :--- | :--- | :--- | :--- |
| **张远 (openfar, 2411264)** | Ubuntu 24.04 (WSL2) | QEMU 8.2.2 | OpenSBI v1.3 (`fw_dynamic`) | `riscv64-unknown-elf-gcc` 15.0 / 13.3 |
| **李云鹏 (lyp, 2414099)** | Ubuntu 22.04 LTS | QEMU 6.2.0 | OpenSBI v0.9 | `riscv64-unknown-elf-gcc` 11.2.0 |
| **刘昀皓 (nagilix, 2413074)** | Ubuntu 24.04 (WSL2) | QEMU 7.0.0 | OpenSBI v1.0 | `riscv64-unknown-elf-gcc` 12.2.0 |

> **环境差异调和策略**：所有成员在各自环境下完成单元编码后，提交前必须统一在 WSL 编译环境切换至 QEMU 4.1.1 基准镜像进行 `make`、`make qemu` 与 `make grade` 验收，严防因固件版本导致的“本地能跑、评测挂起”隐患。

### 2.2 团队协同与 AI 研发工具矩阵

本实验遵循“人类主导系统级架构规划、算法不变性推导与答辩防御审查，AI 智能体辅助契约化代码生成与异常排障”的深度协作原则。三位成员确认的使用工具及底层大语言模型分布如下：

| 成员 | AI 编程工具 | 底层模型 | 备注 / 核心负责场景 |
| :--- | :--- | :--- | :--- |
| **张远 (openfar, 2411264)** | Claude Code（终端 Agent） | Claude Opus 5.5 | T0 环境框架统一与切换、T1 page_init 物理内存初始化、T4 SLUB 实现、T5 QEMU 页表跟踪、T6 整体逻辑、T9 报告集成与验证 |
| **李云鹏 (lyp, 2414099)** | Codex（命令行 Agent / CLI） | Codex | T2 Best-Fit 算法实现、T3 Buddy System 实现、T4 SLUB 单元测试、T5 TLB 硬件行为分析、T6 OS 原理对照 |
| **刘昀皓 (nagilix, 2413074)** | Antigravity CLI / Antigravity IDE (VS Code 协同交互) | Gemini 3.8 Flash (High) / Claude Opus 4.5 / Claude 3.7 Sonnet | T7 First-Fit 算法机理剖析与内存探测思考题、T8 实验公共章节初稿（目的、环境、测试与总结） |

**说明：**
- **AI 编程工具**：指各成员实际操作的终端自主智能体命令行工具、集成开发环境扩展或桌面交互环境。
- **底层模型**：指各工具背后驱动逻辑推理的大语言模型版本。
