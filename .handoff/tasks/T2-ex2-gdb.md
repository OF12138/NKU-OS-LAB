# T2 练习2：使用 GDB 验证启动流程
- 负责人：openfar    状态：进行中    依赖：无
- 可改文件：report/sections/T2-*.md, report/images/T2-*

## 要求
- 指导书原题：用 GDB 跟踪 QEMU 从加电到执行内核第一条指令（0x80200000）的全过程，回答：加电后最初执行的几条指令位于什么地址，主要完成了哪些功能。报告中记录调试过程、观察结果和答案，并附上截图。

## 当前进度 / 下一步
- 已完成：report/sections/T2-ex2-gdb.md（正文，全部基于实测输出）和 T2-ex2-gdb.prompt.md。
- 下一步：补 3 张截图，文件名已在正文中引用：T2-1-reset-0x1000.png、T2-2-mret-to-kernel.png、T2-3-watch.png（操作步骤见下方「复现步骤」）。截图放好后改为「完成」。

## 关键决策与结论
- 启动链：MROM 0x1000（M 态，由 QEMU 生成）→ OpenSBI 0x80000000（M 态）→ mret → kern_entry 0x80200000（S 态）。
- OpenSBI v1.3 的 ELF 没有符号表，所以改为在它的 5 条 mret（0x80000532 / 0x8000aec8 / 0x8000c640 / 0x8000c67a / 0x8000d910）上下断点。一共命中 7 次，只有第 7 次（@0x8000aec8）的 mepc = 0x80200000、MPP = 1，这一次是交接给内核。前 5 次是探测 CSR 时产生的非法指令异常，第 6 次是 semihosting 探测时执行 ebreak 产生的断点异常。
- 进入内核时 sp = 0x80046eb0，仍在 OpenSBI 的内存区域内，而这块区域被 PMP 禁止 S 态访问。T1（lyp）可以引用这一点，说明 la sp 的必要性。
- 指导书勘误：内核由 QEMU 在复位前写入内存，OpenSBI 只负责跳转，所以 watch *0x80200000 永远不会触发（-kernel 和 -device loader 两种加载方式都实测过）；另外 0x1000 处是 MROM，不是 OpenSBI。
- 以上地址都取自 QEMU 8.2.2 / OpenSBI v1.3 环境，换成其他版本，OpenSBI 内部的地址会不同。

## 验证结果
复现步骤（截图就按这个步骤来）：
1. 终端 A：`cd code && make debug`；终端 B：`cd code && make gdb`。
2. 截图 1（T2-1）：在 GDB 中依次执行 `info registers pc priv`、`x/6i $pc`、`x/6gx 0x1028`、`x/2i 0x80200000`。画面上要能看到：pc = 0x1000，priv = Machine，6 条 MROM 指令，fw_dynamic_info 中的 0x80200000 / 1，以及 0x80200000 处已经存在的 kern_entry 指令。
3. 截图 2（T2-2）：`b *0x8000aec8` → `c` → `info registers mepc priv` → `p ($mstatus>>11)&3` → `si` → `info registers pc priv sp`。画面上要能看到：mepc = 0x80200000，MPP = 1，si 之后 pc = 0x80200000 <kern_entry>，priv = Supervisor，sp = 0x80046eb0。
4. 截图 3（T2-3）：重新启动一次 make debug / make gdb，然后 `watch *(unsigned int *)0x80200000` → `b *0x80200000` → `c`。画面上要能看到：只有 Breakpoint 被命中，watchpoint 没有触发。
- 本次的完整 GDB 输出都已整理进报告正文。

## 迭代素材
- 第一版计划按指导书的提示用 watch 观察“加载瞬间”。GDB 一连上就发现 0x80200000 已经有指令，watch 从未触发。由此确认内核由 QEMU 在复位前写入，于是调整报告重点，加入勘误。
- 想在 OpenSBI 中按函数名下断点，但它的 ELF 没有符号（nm: no symbols），于是改为用 objdump 反汇编，在全部 mret 上下断点。
- 第一次在 mret 断点处停下时看到的 MPP = 3，并不是交接给内核的那次。于是改成用 GDB 的 commands 自动打印每次 mret 的 mepc、MPP、mcause、mtval，再继续运行，最终区分出 7 次 mret 各自的成因。
- 连接 GDB 时报错 `Remote replied unexpectedly to 'vMustReplyEmpty': timeout`，QEMU 日志显示 `-s: Failed to find an available port`：1234 端口被上一次运行残留的进程占用。结束残留的 qemu 后重新运行即可。

## 留言
