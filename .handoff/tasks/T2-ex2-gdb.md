# T2 练习2：使用 GDB 验证启动流程
- 负责人：openfar    状态：完成    依赖：无
- 可改文件：report/sections/T2-*.md, report/images/T2-*

## 要求
- 指导书原题：用 GDB 跟踪 QEMU 从加电到执行内核第一条指令（0x80200000）的全过程，回答：加电后最初执行的几条指令位于什么地址，主要完成了哪些功能。报告中记录调试过程、观察结果和答案，并附上截图。

## 当前进度 / 下一步
- 已完成：report/sections/T2-ex2-gdb.md（正文，5 张图穿插在文中）和 T2-ex2-gdb.prompt.md。
- 图片：T2-0-boot-overview.png（启动流程与内存布局示意图）、T2-1-reset-0x1000、T2-2-mret-to-kernel、T2-3-watch、T2-4-ecall（真实终端截图，QEMU 8.2.2）。
- 写作时参考了 reference/ 中前辈的报告（zaz lab1 在 QEMU 4.1.1 上同样发现 watch 不会触发）。

## 关键决策与结论
- 启动链：MROM 0x1000（M 态，由 QEMU 生成）→ OpenSBI 0x80000000（M 态）→ mret → kern_entry 0x80200000（S 态）。
- OpenSBI v1.3 的 ELF 没有符号表，所以改为在它的 5 条 mret（0x80000532 / 0x8000aec8 / 0x8000c640 / 0x8000c67a / 0x8000d910）上下断点。一共命中 7 次，只有第 7 次（@0x8000aec8）的 mepc = 0x80200000、MPP = 1，这一次是交接给内核。前 5 次是探测 CSR 时产生的非法指令异常，第 6 次是 semihosting 探测时执行 ebreak 产生的断点异常。
- 进入内核时 sp = 0x80046eb0，仍在 OpenSBI 的内存区域内，而这块区域被 PMP 禁止 S 态访问。T1（lyp）可以引用这一点，说明 la sp 的必要性。
- 指导书勘误：真机上同样不是 OpenSBI 加载内核（ROM → U-Boot SPL → OpenSBI → U-Boot → 内核，加载由 SPL/U-Boot 完成）。QEMU 中内核由 QEMU 在复位前写入内存，OpenSBI 只负责跳转，所以 watch *0x80200000 永远不会触发（-kernel 和 -device loader 两种加载方式都实测过）；另外 0x1000 处是 MROM，不是 OpenSBI。
- 以上地址都取自 QEMU 8.2.2 / OpenSBI v1.3 环境，换成其他版本，OpenSBI 内部的地址会不同。

## 验证结果
- 所有 GDB 输出都来自 QEMU 8.2.2 / OpenSBI v1.3 的实际运行，截图是真实的终端画面（tmux 左栏 make debug，右栏 make gdb）。
- 复现截图 2 的命令：`b *0x8000aec8` → `c` → `x/7i 0x8000aeb8` → `info registers mepc priv` → `p ($mstatus >> 11) & 3` → `si` → `info registers pc priv sp a0 a1` → `si` → `si` → `info registers pc sp`。0x8000aec8 只适用于 OpenSBI v1.3，换成其他版本需要重新用 objdump 找 mret 的地址。

## 迭代素材
- 第一版计划按指导书的提示用 watch 观察“加载瞬间”。GDB 一连上就发现 0x80200000 已经有指令，watch 从未触发。由此确认内核由 QEMU 在复位前写入，于是调整报告重点，加入勘误。
- 想在 OpenSBI 中按函数名下断点，但它的 ELF 没有符号（nm: no symbols），于是改为用 objdump 反汇编，在全部 mret 上下断点。
- 第一次在 mret 断点处停下时看到的 MPP = 3，并不是交接给内核的那次。于是改成用 GDB 的 commands 自动打印每次 mret 的 mepc、MPP、mcause、mtval，再继续运行，最终区分出 7 次 mret 各自的成因。
- 连接 GDB 时报错 `Remote replied unexpectedly to 'vMustReplyEmpty': timeout`，QEMU 日志显示 `-s: Failed to find an available port`：1234 端口被上一次运行残留的进程占用。结束残留的 qemu 后重新运行即可。

- 用 conhost 截图时，窗口抢到焦点，混进了用户的键盘输入；后来改用 Windows Terminal 窗口 + PrintWindow 截图，并在截图前提醒用户不要打字。

## 留言
