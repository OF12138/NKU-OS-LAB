@AGENTS.md

> 以上是全组共用的规范（AGENTS.md 是唯一来源，修改规范请改 AGENTS.md）。以下是 openfar 本机 Claude Code 的附加须知，其他成员可以忽略。
> 本节由 Claude 自行维护：遇到值得记住的本机环境、工具用法和用户偏好时直接更新，用户会定期审阅修改。



# 本机 Claude 须知（openfar）

## 环境

* Claude Code 运行在 Windows 上，编译和调试通过 `wsl -e bash -c '...'` 在 WSL Ubuntu 中进行。WSL 的 bash 是非交互式的，不会读取 `.bashrc`，需要手动设置 PATH：`export PATH=$HOME/Environment/riscv/riscv-elf-toolchains/bin:$PATH`。
* 本机 QEMU：`/usr/bin/qemu-system-riscv64` 8.2.2，自带 OpenSBI v1.3（fw_dynamic）。OpenSBI 的 ELF 位于 `/usr/share/qemu/opensbi-riscv64-generic-fw_dynamic.elf`，没有符号表。
* 从 Git Bash 调用 wsl 时，要加 `MSYS_NO_PATHCONV=1`，否则 `/mnt/...` 路径会被改写。
* GDB 连接失败、报 `vMustReplyEmpty: timeout` 时，通常是 1234 端口被残留的 qemu 占用，先执行 `pkill -f qemu-system-riscv64`。
* 引用仓库代码做实验或截图时，要用 `git archive HEAD code` 导出干净的副本：用户本地可能有未提交的学习注释，会改变源码行号。
* **WSL 中不要直接在 `~/` 下新建临时目录。** 需要在 WSL 里编译或调试时，统一用 `.claude/tools/sync-wsl.sh [分支]` 把已提交的 code/ 同步到 `~/Code/NKU-OS-LAB/<分支>/code`。

## 带符号的 OpenSBI

* QEMU 自带的固件没有符号表。WSL 中的 `~/Code/opensbi` 是 OpenSBI **v1.3** 的源码（与 QEMU 8.2.2 自带的版本相同），已编译好，带符号的固件位于 `~/Code/opensbi/build/platform/generic/firmware/fw_dynamic.elf`。
* 重新编译：`make PLATFORM=generic CROSS_COMPILE=riscv64-unknown-elf-`。本机 GCC 15 默认采用 C23，`bool` 会和 OpenSBI 的 typedef 冲突，所以已在该仓库 Makefile 的 CFLAGS 中加了 `-std=gnu11`（只改了这一处，没有改源码）。
* 使用：`make debug OPENSBI=<上面的路径>`，配合 `make gdb OPENSBI=<同一路径>`（lab1 的 code/Makefile 已支持）。之后可以按函数名下断点，例如 `sbi_hart_init`、`sbi_hart_switch_mode`、`sbi_ecall_handler`、`uart8250_putc`。截图时设置 `MAKEARGS="OPENSBI=..."` 再调用 tmux-gdb.sh。
* 注意：自己编译的固件与 QEMU 自带的固件逻辑相同，但内存布局不同（190 KB、读写区从 0x80020000 开始，自带的是 322 KB、从 0x80040000 开始），所以两者的地址不能混用。
* 后续 lab 如果要在 Makefile 中加入 `OPENSBI` 变量，可以照搬 lab1 的写法（`-bios $(OPENSBI)`，以及 `GDB_OPENSBI` 变量）。

## 截图（终端）

* 用户偏好 Windows Terminal 的现代显示效果（浅色主题），不要用 conhost 的黑底窗口。
* 流程：先用 `.claude/tools/tmux-gdb.sh <WSL 中的 code 目录> "<gdb 命令>" ...` 在 WSL 中建好 tmux 分栏（左栏 make debug，右栏 make gdb），再在 PowerShell 中运行 `.claude/tools/capture-wt.ps1 -Out <png> -Cols 170 -Lines <行数>`。脚本会打开一个标题为 `lab1-gdb` 的 Windows Terminal 窗口，attach 到 tmux 会话，用 `PrintWindow` 截取后关闭这个窗口。
* 截图窗口会弹出几秒，期间用户在键盘上打字会被输入进去，所以截图前要提醒用户。截完后检查画面里有没有多余字符。

## 画图

* 风格：传统工科风格，黑白、不用彩色，矩形用尖角（直角），不用圆角。中文用宋体（SimSun），西文用 Times New Roman，地址用 Consolas。
* 用 matplotlib 画图时使用 conda 环境 FCOS（`D:\Applications\Anaconda\envs\FCOS\python.exe`）。base 环境的 numpy/matplotlib 会崩溃（0xc06d007e），其他环境的 matplotlib 可能也有问题。
* 示例脚本：`.claude/tools/diagram-example.py`（lab1 启动流程图）。SimSun 中没有 `⋮` 这类特殊符号，需要改用图形元素绘制。

## 用户偏好

* 用户本地有未提交的修改（学习注释、报告润色等）时，检查没有大问题就**直接提交，不用询问**。代码改动要先确认编译通过、`make qemu` 正常；只加注释时可以比较 `.text` 段

## 审核成员提交（openfar 的默认职责）

* 每次 session 拉取到其他成员的新提交时，都要主动审核（内容是否正确、是否符合模板、截图是否真实）。
* **错误少、便于修改**：直接修改该成员的 section，然后在其任务文件的「留言」区逐条说明改了什么。
* **需要重大改动**（例如结论错误、缺少模板要求的部分、需要重做实验或截图）：先写一份飞书提醒草稿给用户看，**用户确认后**再用 `lark-cli im +messages-send --as user` 发送。消息开头标明“openfar 的 Claude 助手代发”。lyp 的 open_id 是 `ou_d99902ac7dfa159ac2d575daae77b1f7`，nagilix 的是 `ou_85669e7b438f06a27b7a9de723634ffa`。
* 状态没有「待审」，成员做完即为「完成」；审核不改变状态，除非需要返工。

## 集成工具

* `python .claude/tools/merge-report.py --init [完成日期]`：首次按模板顺序把 report/sections/ 合并成 report.md 和 prompt.md。图片路径改成 `./images/`，图号按出现顺序统一编号，指向 prompt 文件的相对链接改为纯文本，缺失的 section 用带隐藏标记的“【待补】”占位。report.md 已存在时拒绝执行；**不要加 `--force`**，lab1 曾因此覆盖了用户正在修改的内容。新 lab 要按该 lab 的 section 名调整脚本中的 `init()`。
* `python .claude/tools/merge-report.py --fill`：只把已提交的 section 填进“【待补】”占位处，其余文字不动，然后重新编号全文图号；prompt.md 只追加尚未收录的提示词。
* `python .claude/tools/merge-report.py --replace <section 文件> "<起始标题>" "<结束标题>" [...]`：成员在合并后又修改了 section 时，用它把 report.md 中对应的那一段换成新版（可以一次给多组，每组 3 个参数），其余部分不动；同时统一格式（`../images/`→`./images/`、`<img ...>`→`![]()`、`<center>`→`<p align="center">`），并重新编号全文图号。运行前先确认 report.md 没有用户未提交的修改。
* `python .claude/tools/merge-report.py --prompts`：用各 `*.prompt.md` 的当前内容刷新 prompt.md 中已有的段落。
* `python .claude/tools/check-rely.py`：逐行检查各提示词 [RELY] 中的代码是否仍在 code/ 中（比较时忽略注释），标注“修改前”的引文会跳过。

## 报告写法

* 动笔前先参考 `reference/` 中前辈的同类报告，防止偏题或掉坑；只能参考，不能抄袭。
* 图片插在相关文字中间，不要集中放在末尾；每张图下面写图题 `<p align="center">图 x-y　……</p>`。**不要用 `<center>`**：GitHub 会删掉这个标签，导致不居中；`align="center"` 在 GitHub 和 Typora 上都有效。
* **工作区里有用户未提交的文件时，禁止 `git stash`、`git pull --autostash`、`git checkout -- <文件>` 等任何会临时改写这些文件的操作。** lab1 时 autostash 把 report.md 换回旧版本的那一两秒里，Typora 读到旧内容并存了回去，覆盖了用户的修改（后来从 autostash 快照中恢复）。正确做法：只 `git add` 自己的文件并提交，`git fetch` 确认远端没有新提交后直接 `git push`；远端有新提交时，在独立的 worktree 里 rebase 后再推，或者等用户提交完再拉取。
* **report.md 由用户亲自修改。** 用户改它期间，不运行任何会写 report.md 的脚本；之后补内容只用 `merge-report.py --fill`，运行前先 `git status` 确认 report.md 没有未提交的修改，有就先停下来问用户。
