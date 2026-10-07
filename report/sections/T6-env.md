## 软件环境

三位成员各自在 WSL 中搭建了实验环境，QEMU 版本各不相同，自带的 OpenSBI 版本也随之不同。报告中各处的地址和截图都注明了所用环境，不同版本的地址不能混用。

| 成员 | 宿主系统 | QEMU（自带 OpenSBI） | 交叉工具链 | 报告中对应的实验 |
| :--- | :--- | :--- | :--- | :--- |
| 2411264-张远 | WSL2 Ubuntu 24.04 | 8.2.2（OpenSBI v1.3） | riscv64-unknown-elf-gcc 15.1.0 | 练习 2、Makefile 的修改 |
| 2414099-李云鹏 | WSL2 Ubuntu 22.04 | 6.2.0（OpenSBI v0.9） | SiFive riscv64-unknown-elf-gcc 10.2.0 | 练习 1、功能模块 |
| 2413074-刘昀皓 | WSL2 Ubuntu 22.04 | 7.0.0（OpenSBI v1.0） | riscv64-unknown-elf-gcc 11.4.0 | 测试与验证 |

调试器均为工具链自带的 `riscv64-unknown-elf-gdb`。Makefile 改用 `-kernel` 加载内核之后（见练习 2 第二节的“版本差异”说明），`make qemu` 在以上三个版本的 QEMU 上都能正常启动内核。

## AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
| :--- | :--- | :--- | :--- |
| 2411264-张远 | Claude Code（终端 Agent） | Claude Opus 5.5 | 在 Windows 上运行，通过 WSL 编译和调试 |
| 2414099-李云鹏 | Codex（终端 Agent） | （待确认） | |
| 2413074-刘昀皓 | VS Code（WSL）+ 网页对话 | Claude 3.5 Sonnet / DeepSeek-R1 | |

**说明：**
- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面。
- **底层模型**：指该工具使用的大语言模型及版本。
