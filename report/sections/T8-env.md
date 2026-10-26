## 软件环境

lab1 中三人的 QEMU 版本各不相同（8.2.2、6.2.0、7.0.0），而 lab2 框架的 `Makefile` 和 `tools/grade.sh` 都用 `-device loader,file=bin/ucore.img,addr=0x80200000` 加载内核。这种方式依赖旧版 OpenSBI 的 fw_jump 固件固定跳转到 0x80200000；lab1 已经实测，在 QEMU 8.2（OpenSBI v1.3，fw_dynamic）上内核根本不会运行。为了让 `make grade` 的结果在三人之间可比，lab2 起全组统一使用指导书推荐的 **QEMU 4.1.1**：从源码编译，带调试信息（`--enable-debug`，分支任务调试 QEMU 源码时也用它），不执行 `make install`，通过环境变量 `QEMU` 交给 Makefile 使用。

| 成员 | 宿主系统 | QEMU（自带 OpenSBI） | 交叉工具链 |
| :--- | :--- | :--- | :--- |
| 2411264-张远 | WSL2 Ubuntu 24.04 | 4.1.1（OpenSBI v0.4） | riscv64-unknown-elf-gcc 15.1.0 |
| 2414099-李云鹏 | WSL2 Ubuntu 22.04 | 4.1.1（OpenSBI v0.4） | SiFive riscv64-unknown-elf-gcc 10.2.0 |
| 2413074-刘昀皓 | WSL2 Ubuntu 24.04 | 4.1.1（OpenSBI v0.4） | riscv64-unknown-elf-gcc 12.2.0 |

调试器均为工具链自带的 `riscv64-unknown-elf-gdb`；分支任务中调试 QEMU 进程本身用的是宿主机的 `gdb`。

## AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
| :--- | :--- | :--- | :--- |
| 2411264-张远 | Claude Code（终端 Agent） | Claude Opus 5.5 | 在 Windows 上运行，通过 WSL 编译和调试 |
| 2414099-李云鹏 | Codex（终端 Agent） | gpt-6.1-sol | |
| 2413074-刘昀皓 | Antigravity CLI / Antigravity IDE | Gemini 3.8 Flash (High) / Claude Opus 4.5 / Claude 3.7 Sonnet | |

**说明：**
- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面。
- **底层模型**：指该工具使用的大语言模型及版本。
