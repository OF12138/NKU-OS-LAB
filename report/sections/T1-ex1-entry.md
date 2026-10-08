## 练习1：理解内核启动中的程序入口操作

**负责人：** 2414099－李云鹏

> **题目**：阅读 kern/init/entry.S内容代码，结合操作系统内核启动流程，说明指令 la sp, bootstacktop 完成了什么操作，目的是什么？ tail kern_init 完成了什么操作，目的是什么？

**结论**：进入内核后，`entry.S` 先执行 `la sp, bootstacktop`，再执行 `tail kern_init`。这两句分别解决栈和控制流的问题：先给 C 函数准备栈，再把控制权交给 C 语言内核初始化函数。

### 1. `la sp, bootstacktop` 做了什么

先看启动栈的定义：

```asm
.section .data
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE
    .global bootstacktop
bootstacktop:
```

`mmu.h` 中 `PGSHIFT=12`、`PGSIZE=4096`，`memlayout.h` 中 `KSTACKPAGE=2`。因此 `.align PGSHIFT` 使栈按 4096 字节对齐，`.space KSTACKSIZE` 为内核栈分配内存空间（8192 字节）。`bootstack` 和 `bootstacktop` 则分别标记这块空间的低地址端和高地址端。

`la` 加载的是 `bootstacktop` 的地址。执行后，`sp` 指向预留区域的高地址端；RISC-V 栈向低地址增长，后面的函数通过减小 `sp` 留出栈帧。这里没有动态申请内存：栈空间已经由汇编和链接阶段安排好，入口只需让 `sp` 指向它。

之所以一定要先设置栈，是因为接下来进入 `kern_init()` 后，C 函数会用栈保存返回地址、局部变量等，后面的 `cprintf()` 也需要正常的函数调用环境。

使用命令 `riscv64-unknown-elf-nm -n bin/kernel` 查看最终内核 ELF 文件里的符号及其地址，观察到：

```text
0000000080201000 D bootstack
0000000080203000 D bootstacktop
```

即栈区是 `[0x80201000,0x80203000)`，共 8 KiB；初始 `sp=0x80203000`，也满足 ABI 的 16 字节对齐要求。

下面是本机重新运行 GDB 后的输出。在 VSCode 终端的 tmux 分屏中，左栏运行 `make debug`，右栏连接 GDB 并单步执行：`sp` 从 `0x80017ee0` 变为 `0x80203000`，执行 `tail` 后 PC 到达 `kern_init`，`ra` 仍为 `0x800078cc`。

<img src="../images/T1-entry-step.png"  style="zoom:200%;" />

<center>图 T1-1　启动栈设置和尾跳转的 GDB 单步调试（QEMU 6.2.0 / OpenSBI v0.9）</center>

### 2. 为什么一条 `la` 要单步两次

通过指令`riscv64-unknown-elf-objdump -d -M no-aliases bin/kernel`查看在最终 ELF 中，入口的反汇编为：

```text
80200000: 00003117    auipc sp,0x3
80200004: 00010113    mv    sp,sp
80200008: a009        j     8020000a <kern_init>
```

`la` 是伪指令，前两条才是它实际对应的机器指令。`auipc` 用当前 PC 加上高位偏移，得到 `0x80200000 + (3 << 12) = 0x80203000`；第二条补低位偏移。这次低位恰好为零，所以 `addi sp,sp,0` 被反汇编显示成 `mv sp,sp`。

GDB 按机器指令单步，所以检查 `la` 的执行结果时要执行两次 `si`，才能到下一行源码。

### 3. `tail kern_init` 做了什么

`tail` 跳到 `kern_init`，且不保存返回地址，直接把控制权交给内核初始化函数即可。普通 `call` 会更新 `ra`，以便函数结束后回到调用点。但这里启动入口不再需要继续执行。

值得注意的是，`tail` 只表示“不保存返回地址地跳转”，但最终使用哪种机器指令，取决于链接结果。通过指令 `riscv64-unknown-elf-objdump -dr obj/kern/init/entry.o` 检查未链接的 `entry.o` ，可以看到，`tail kern_init` 被汇编为带重定位信息的 `auipc` 和 `jalr` 两条指令。链接后，由于 `kern_init` 距离较近，链接器将其优化为一条 16 位的 `c.j`。

本机在 GDB 中执行完 `la`、再执行完 `tail`，得到以下记录：

| 时刻 | PC | sp | ra |
|---|---|---|---|
| 执行完 `la` | `0x80200008` | `0x80203000` | `0x800078cc` |
| 执行完 `tail` | `0x8020000a` | `0x80203000` | `0x800078cc` |

`PC` 已到 `kern_init`，`sp` 和 `ra` 都没有变化。`tail` 本身不会清理栈；当前入口也没有建立需要清理的栈帧。

`kern_init` 的声明带有 `__attribute__((noreturn))`，告诉编译器这个函数不会返回。实际代码在清零和打印之后执行 `while (1)`，这才保证它不会走回调用者。`noreturn` 是编译约定，不是让函数无法返回的硬件机制。

### 4. 为什么启动栈放在 `.data`

`kern_init` 开始时执行：

```c
    extern char edata[], end[];
    memset(edata, 0, end - edata);
```

链接脚本把 `edata` 放在 `.data/.sdata` 之后，把 `end` 放在 `.bss` 之后。这段代码清零 `[edata,end)`，为未显式初始化的静态变量建立零值。

调用 `memset` 时，启动栈已经在使用了。如果把栈直接移到这个会被整体清零的 BSS 区间，清零可能覆盖栈上保存的返回地址和数据。放在 `.data` 后，启动栈位于 `edata` 之前，不会被这次清零碰到；相应地，预留的零字节也会占用镜像空间。

本次 `nm` 中 `edata` 和 `end` 都是 `0x80203008`，说明当前没有存活的非空 BSS，调用长度为零。这里解释的是这段启动代码的用途，而本次构建还没有数据需要它清零。
