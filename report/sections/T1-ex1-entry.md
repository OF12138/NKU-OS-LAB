## 练习1：理解内核启动中的程序入口操作

**负责人：** 2414099－李云鹏（lyp）

进入内核后，`entry.S` 先执行 `la sp, bootstacktop`，再执行 `tail kern_init`。这两句分别解决栈和控制流的问题：先给 C 函数准备能用的栈，再跳到 C 语言入口。

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

`mmu.h` 中 `PGSHIFT=12`、`PGSIZE=4096`，`memlayout.h` 中 `KSTACKPAGE=2`。因此 `.align PGSHIFT` 使栈按 4096 字节对齐，`.space KSTACKSIZE` 预留 8192 字节。`bootstack` 和 `bootstacktop` 分别标记这块空间的低地址端和高地址端。

`la` 加载的是 `bootstacktop` 的地址。执行后，`sp` 指向预留区域的高地址端；RISC-V 栈向低地址增长，后面的函数通过减小 `sp` 留出栈帧。这里没有动态申请内存：栈空间已经由汇编和链接阶段安排好，入口只需让 `sp` 指向它。

进入 `kern_init` 后，编译器会生成分配栈帧、保存返回地址的指令，后续 `cprintf` 也会使用栈。因此入口要先建立内核自己的栈，不能沿用仍指向固件区域的 `sp`。

本机用 SiFive GCC 10.2.0 构建了同一份代码，`nm` 得到：

```text
0000000080201000 D bootstack
0000000080203000 D bootstacktop
```

所以栈区域是 `[0x80201000,0x80203000)`，共 8 KiB；初始 `sp=0x80203000`，也满足 ABI 的 16 字节对齐要求。

下面是本机重新运行 GDB 后的输出。截图取自批处理原始输出的展示页：`sp` 从 `0x80017ee0` 变为 `0x80203000`，执行 `tail` 后 PC 到达 `kern_init`，`ra` 仍为 `0x800078cc`。

![lyp本机入口单步实验](../images/T1-entry-step.jpg)

<center>图 T1-1　启动栈设置和尾跳转的 GDB 输出（lyp 本机，QEMU 6.2.0 / OpenSBI v0.9）</center>

### 2. 为什么一条 `la` 要单步两次

在最终 ELF 中，入口的反汇编为：

```text
80200000: 00003117    auipc sp,0x3
80200004: 00010113    mv    sp,sp
80200008: a009        j     8020000a <kern_init>
```

`la` 是伪指令，前两条才是它实际对应的机器指令。`auipc` 用当前 PC 加上高位偏移，得到 `0x80200000 + (3 << 12) = 0x80203000`；第二条补低位偏移。这次低位恰好为零，所以 `addi sp,sp,0` 被反汇编显示成 `mv sp,sp`。加上 `-M no-aliases` 就能看到原指令。

GDB 按机器指令单步，所以检查 `la` 的执行结果时要执行两次 `si`，才能到下一行源码。

### 3. `tail kern_init` 做了什么

`tail` 跳到 `kern_init`，且不为这次跳转保存返回地址。普通 `call` 会更新 `ra`，以便函数结束后回到调用点；这里启动入口不再需要继续执行，直接把控制权交给内核初始化函数即可。

检查未链接的 `entry.o`，`tail` 对应带重定位的 `auipc t1,...` 和 `jalr x0,0(t1)`。链接后，目标很近，链接器把这两条松弛成一条16位的 `c.j`，默认显示为上面的 `j`。因此要分清两个层次：伪指令表达的是不保存返回地址的跳转，最终使用哪几条机器指令还取决于链接结果。

本机在 GDB 中执行完 `la`、再执行完 `tail`，得到以下记录，使用的固件是 OpenSBI v0.9：

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

### 验证与分析过程

本机从提交 `7b0948d` 导出的代码副本构建，使用 WSL Ubuntu-22.04-OS、SiFive GCC 10.2.0、QEMU 6.2.0 / OpenSBI v0.9。使用的检查命令为：

```sh
make -j2
riscv64-unknown-elf-objdump -dr obj/kern/init/entry.o
riscv64-unknown-elf-objdump -d -M no-aliases bin/kernel
riscv64-unknown-elf-nm -n bin/kernel
```

GDB 在 `kern_entry` 断下后，用 `si` 和 `info registers pc sp ra` 检查栈设置及跳转。分析时，最终 ELF 里只有一条压缩跳转，于是补查目标文件的重定位，确认它来自 `tail` 的链接松弛；同时看到 `edata=end`，将清零结果写为零长度调用。

本练习的最终提示词见 [T1-ex1-entry.prompt.md](T1-ex1-entry.prompt.md)。题目见[指导书练习1](http://8.135.34.58/lab2026/_book/lab1/lab1_2_1_exercise.html)。
