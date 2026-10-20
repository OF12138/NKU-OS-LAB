# 二、实验环境

### 2.1 软硬件基础设施配置

为了确保团队成员与评测环境的完全一致性，避免因模拟器与固件版本不匹配引发的引导失败（如高版本 QEMU/OpenSBI 的 `fw_dynamic` 跳转地址不匹配问题），本组严格统一了底层的架构模拟器与工具链环境：

| 配置项 | 环境规范 / 实测版本 | 作用说明 |
| :--- | :--- | :--- |
| **宿主操作系统** | Ubuntu 24.04 LTS (WSL2 / Linux Kernel 5.15+) | 交叉编译构建、自动化评测与内核运行的宿主环境 |
| **架构模拟器** | QEMU emulator version 4.1.1 (`qemu-system-riscv64`) | 统一自源码编译，加入 `--enable-debug` 保留调试符号；模拟 RISC-V 64 Virt 虚拟硬件平台 |
| **底层运行时固件** | OpenSBI v0.4 (QEMU 4.1.1 内置) | 提供 M 态运行时支持，采用 `fw_jump` 模式固定跳转至 0x80200000 引导内核 |
| **交叉编译工具链** | `riscv64-unknown-elf-gcc` (10.x～14.x) / `binutils` | 编译 S 态裸机内核、链接生成 ELF 及剥离裸二进制镜像 (`ucore.img`) |
| **底层调试器** | `riscv64-unknown-elf-gdb` / `gdb-multiarch` | 指令级单步跟踪、内存状态巡检、软硬件联合调试（含 QEMU 源码调试） |

**关于 QEMU 4.1.1 统一规范的特别说明：**
框架的 [`code/Makefile`](file:///home/nagilix/Code/NKU-OS-LAB/code/Makefile) 与 [`code/tools/grade.sh`](file:///home/nagilix/Code/NKU-OS-LAB/code/tools/grade.sh) 均使用 `-device loader,file=bin/ucore.img,addr=0x80200000` 加载内核，依赖 OpenSBI（`fw_jump` 模式）固定跳转至 0x80200000。在 QEMU 8.x+ 等较新版本中，OpenSBI 切换为 `fw_dynamic` 模式，该参数会导致 OpenSBI 的 Next Address 被清零而无法启动内核。全组统一采用带调试信息的 QEMU 4.1.1（`~/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64`），既保障了 `make grade` 评分的基准一致性，又可直接用于分支任务中 GDB 对 QEMU 源码的断点跟踪。

### 2.2 团队协同与 AI 研发工具矩阵

本实验遵循“人类主导系统级架构规划、算法不变性推导与答辩防御审查，AI 工具辅助契约化代码生成与排障测试”的分工原则。三位成员确认的使用工具及模型分布如下：

| 成员 | AI 编程工具 | 底层模型 | 备注 / 核心负责场景 |
| :--- | :--- | :--- | :--- |
| **张远 (openfar, 2411264)** | Claude Code（终端 Agent）/ Far CLI | Claude 3.5 Sonnet | T0 环境框架统一与切换、T1 page_init 物理内存初始化、T4 SLUB 实现、T5 QEMU 页表跟踪、T6 整体逻辑、T9 报告集成与验证 |
| **李云鹏 (lyp, 2414099)** | VS Code (GitHub Copilot / Cursor) | GPT-4o / Claude 3.5 Sonnet | T2 Best-Fit 算法实现、T3 Buddy System 实现、T4 SLUB 单元测试、T5 TLB 硬件行为分析、T6 OS 原理对照 |
| **刘昀皓 (nagilix, 2413074)** | Antigravity CLI / Antigravity IDE (VS Code 协同交互) | Gemini 3.8 Flash (High) / Claude 3.5 Sonnet / DeepSeek-R1 | T7 First-Fit 算法机理剖析与内存探测思考题、T8 实验公共章节初稿（目的、环境、测试与总结） |

**说明：**
- **AI 编程工具**：指各成员实际操作的终端智能体命令行工具、集成开发环境扩展或桌面交互环境。
- **底层模型**：指各工具背后驱动逻辑推理的大语言模型版本。
