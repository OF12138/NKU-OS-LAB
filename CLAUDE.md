@AGENTS.md

> 以上是全组共用的规范（AGENTS.md 是唯一来源，修改规范请改 AGENTS.md）。以下是 openfar 本机 Claude Code 的附加须知，其他成员可以忽略。



# 本机 Claude 须知（openfar）

## 环境

* Claude Code 运行在 Windows 上，编译和调试通过 `wsl -e bash -c '...'` 在 WSL Ubuntu 中进行。WSL 的 bash 是非交互式的，不会读取 `.bashrc`，需要手动设置 PATH：`export PATH=$HOME/Environment/riscv/riscv-elf-toolchains/bin:$PATH`。
* 本机 QEMU：`/usr/bin/qemu-system-riscv64` 8.2.2，自带 OpenSBI v1.3（fw_dynamic）。OpenSBI 的 ELF 位于 `/usr/share/qemu/opensbi-riscv64-generic-fw_dynamic.elf`，没有符号表。
* 从 Git Bash 调用 wsl 时，要加 `MSYS_NO_PATHCONV=1`，否则 `/mnt/...` 路径会被改写。
* GDB 连接失败、报 `vMustReplyEmpty: timeout` 时，通常是 1234 端口被残留的 qemu 占用，先执行 `pkill -f qemu-system-riscv64`。
* 引用仓库代码做实验或截图时，要用 `git archive HEAD code` 导出干净的副本：用户本地可能有未提交的学习注释，会改变源码行号。

## 截图（终端）

* 用户偏好 Windows Terminal 的现代显示效果（浅色主题），不要用 conhost 的黑底窗口。
* 流程：先用 `.claude/tools/tmux-gdb.sh <WSL 中的 code 目录> "<gdb 命令>" ...` 在 WSL 中建好 tmux 分栏（左栏 make debug，右栏 make gdb），再在 PowerShell 中运行 `.claude/tools/capture-wt.ps1 -Out <png> -Cols 170 -Lines <行数>`。脚本会打开一个标题为 `lab1-gdb` 的 Windows Terminal 窗口，attach 到 tmux 会话，用 `PrintWindow` 截取后关闭这个窗口。
* 截图窗口会弹出几秒，期间用户在键盘上打字会被输入进去，所以截图前要提醒用户。截完后检查画面里有没有多余字符。

## 画图

* 风格：传统工科风格，黑白、不用彩色，矩形用尖角（直角），不用圆角。中文用宋体（SimSun），西文用 Times New Roman，地址用 Consolas。
* 用 matplotlib 画图时使用 conda 环境 FCOS（`D:\Applications\Anaconda\envs\FCOS\python.exe`）。base 环境的 numpy/matplotlib 会崩溃（0xc06d007e），其他环境的 matplotlib 可能也有问题。
* 示例脚本：`.claude/tools/diagram-example.py`（lab1 启动流程图）。SimSun 中没有 `⋮` 这类特殊符号，需要改用图形元素绘制。

## 报告写法

* 动笔前先参考 `reference/` 中前辈的同类报告，防止偏题或掉坑；只能参考，不能抄袭。
* 图片插在相关文字中间，不要集中放在末尾；每张图下面写图题（`<center>图 x-y　……</center>`）。
