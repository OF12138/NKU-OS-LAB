<h1 align="center">操作系统lab1实验报告</h1>

<h5 align="center">实验名称：Lab 1: 最小可执行内核  完成日期：2026-10-07</h5 align="center">
<h5 align="center">小组成员：2411264-张远、2414099-李云鹏、2413074-刘昀皓</h5 align="center">



# 小组分工

| 成员 | 负责的练习/模块/代码/报告 |
|------|----------------|
| 2411264-张远 | 练习 2（GDB 验证启动流程）、实验整体逻辑分析、报告集成 |
| 2414099-李云鹏 | 练习 1（内核入口）、功能模块（链接脚本与内存布局、格式化输出与 SBI、构建与镜像加载）、对操作系统的理解 |
| 2413074-刘昀皓 | 实验目的、实验环境、测试与验证、拓展（现代笔记本的启动流程）、实验总结 |



# 一、实验目的

本实验在 QEMU 模拟的 RISC-V 64 位平台上，构建并调试一个最小可执行内核，从加电复位一直跟踪到内核打印出第一行信息。主要目的有：

1. **理解从加电到内核的启动流程**：弄清复位代码（`0x1000`）、M 态固件 OpenSBI（`0x80000000`）和 S 态内核（`0x80200000`）之间如何交接，包括通过寄存器传递的启动参数，以及 OpenSBI 如何用 `mret` 从 M 态切换到 S 态。
2. **理解内核如何建立运行环境并与外界交互**：理解入口汇编为什么要先建立启动栈、C 代码开始前为什么要清零 BSS，以及内核如何通过 `ecall` 调用 OpenSBI 提供的 SBI 服务，实现格式化输出。
3. **学会用 GDB 调试裸机内核**：通过 `make debug` 和 `make gdb` 把 GDB 连接到 QEMU，在没有操作系统支持的环境下设置指令级断点、单步执行、查看寄存器和内存，用调试结果验证以上过程。




# 二、实验环境

## 软件环境

三位成员各自在 WSL 中搭建了实验环境，QEMU 和 OpenSBI 的版本各不相同。

| 成员 | 宿主系统 | QEMU（自带 OpenSBI） | 交叉工具链 | 报告中对应的实验 |
| :--- | :--- | :--- | :--- | :--- |
| 2411264-张远 | WSL2 Ubuntu 24.04 | 8.2.2（OpenSBI v1.3） | riscv64-unknown-elf-gcc 15.1.0 | 练习 2、Makefile 的修改 |
| 2414099-李云鹏 | WSL2 Ubuntu 22.04 | 6.2.0（OpenSBI v0.9） | SiFive riscv64-unknown-elf-gcc 10.2.0 | 练习 1、功能模块 |
| 2413074-刘昀皓 | WSL2 Ubuntu 22.04 | 7.0.0（OpenSBI v1.0） | riscv64-unknown-elf-gcc 11.4.0 | 测试与验证 |

调试器均为工具链自带的 `riscv64-unknown-elf-gdb`。`make qemu` 在以上三个版本的 QEMU 上都能正常启动内核。

## AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
| :--- | :--- | :--- | :--- |
| 2411264-张远 | Claude Code（terminal & VS Code） | Claude Opus 5.5 & Sonnet 5.5 | 在 Windows 上运行，通过 WSL 编译和调试 |
| 2414099-李云鹏 | Codex（终端 Agent） | （待确认） | |
| 2413074-刘昀皓 | VS Code（WSL）+ 网页对话 | Claude 3.5 Sonnet / DeepSeek-R1 | |




# 三、整体分析

## 3.1 逻辑主线

Lab1 的主题是**最小可执行内核**：让一段我们自己编写的代码在一台“什么都没有”的 RISC-V 机器上拿到 CPU，并且能向外界输出信息。所谓“什么都没有”，是指这时既没有操作系统替我们装载程序，也没有 C 标准库和现成的栈。整个实验要回答的问题是：**在这样的条件下，内核怎样被放到正确的位置、从正确的指令开始执行、获得能运行 C 代码的环境，并且与外界交互？**

我们认为本章可以概括成一句话：**控制权的接力**。加电后，控制权依次经过 QEMU 生成的复位代码、OpenSBI 固件、内核入口 `entry.S` 和 C 函数 `kern_init`，最后通过 SBI 服务把字符送到串口。每一棒都要替下一棒准备好条件：复位代码交出固件的入口和启动参数，OpenSBI 设置好特权级和入口地址，`entry.S` 提供可用的栈，`kern_init` 清零未初始化的全局数据。

## 3.2 功能的逐步分析

Lab1 不要求编写新代码，我们更多进行分析。下表列出了启动每一步对应的代码和报告中的分析位置：

| 顺序 | 功能 | 主要代码 | 报告中的分析 |
|------|------|----------|--------------|
| 1 | 确定内核的形态和地址 | `Makefile`、`tools/kernel.ld` | 功能模块「链接脚本与内核内存布局」「构建与镜像加载」 |
| 2 | 装载与固件交接：把控制权交给内核 | QEMU 复位代码、OpenSBI | 练习 2 |
| 3 | 入口建栈：为 C 代码准备栈 | `kern/init/entry.S` | 练习 1 |
| 4 | C 运行环境初始化：清零 BSS | `kern/init/init.c` | 练习 1、功能模块「链接脚本与内核内存布局」 |
| 5 | 格式化输出：通过 SBI 向终端打印 | `kern/libs/stdio.c`、`libs/printfmt.c`、`libs/sbi.c` | 功能模块「格式化输出与 SBI 服务」、练习 2 第六节 |

1. **首先是构建与链接。** 内核运行之前，必须先确定它“长什么样、放在哪”：链接脚本规定 `.text` 从 `0x80200000` 开始，各段按代码、只读数据、数据、BSS 的顺序排列，`entry.o` 排在链接输入的最前面，保证 `kern_entry` 正好位于起始地址。后面所有步骤都以这些约定为前提，所以它排在第一位。

2. **接着是装载与固件交接。** QEMU 在 CPU 开始执行之前就按 ELF 把内核放到了 `0x80200000`；CPU 从 `0x1000` 的复位代码开始执行，跳到 `0x80000000` 的 OpenSBI。OpenSBI 在 M 态完成平台初始化，配置好异常委托和 PMP 内存保护，最后用 `mret` 降到 S 态，跳到内核入口。只有到了这一步，内核才第一次真正拿到 CPU。

3. **然后是入口建栈。** 内核拿到 CPU 时，`sp` 仍然指向 OpenSBI 自己的栈，而那块内存已经被 PMP 设置为 S 态不可访问。C 函数需要用栈保存返回地址和局部变量，所以 `entry.S` 的第一件事就是 `la sp, bootstacktop`，换成内核在 `.data` 段中预留的 8 KB 启动栈，然后用 `tail kern_init` 跳进 C 代码。这一步只能用汇编完成，因为在栈准备好之前，C 代码根本无法正确运行。

4. **再是 C 运行环境的初始化。** 进入 `kern_init` 后，第一件事是 `memset(edata, 0, end - edata)`，把 BSS 区清零，满足 C 语言“未初始化的全局变量为 0”的约定。`edata` 和 `end` 由链接脚本提供，这一步又一次用到了第 1 步的约定。（在我们的构建中 BSS 恰好为空，清零长度为 0，但这一步对一般的内核仍然是必需的。）

5. **最后是格式化输出。** `cprintf` 把格式化工作交给 `vprintfmt`，每生成一个字符就经 `cputch → cons_putc → sbi_console_putchar` 送出。最后一层用 `ecall` 陷入 M 态，由 OpenSBI 的串口驱动把字符写到 UART。输出放在最后，是因为它依赖前面所有步骤：需要可用的栈、已初始化的全局变量，还需要 OpenSBI 已经注册好 SBI 服务。

6. 打印完信息后，`kern_init` 进入 `while(1)` 死循环。此时内核还没有中断处理、内存管理和进程调度，能做的事情也就到此为止。

   


# 四、实验内容与实现

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

![](./images/T1-entry-step.png)

<p align="center">图 1　启动栈设置和尾跳转的 GDB 单步调试（QEMU 6.2.0 / OpenSBI v0.9）</p>

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


## 练习2：使用 GDB 验证启动流程

**负责人：** openfar

> **题目**：使用 GDB 跟踪 QEMU 模拟的 RISC-V 从加电开始，直到执行内核第一条指令（跳转到 0x80200000）的整个过程。RISC-V 硬件加电后最初执行的几条指令位于什么地址？它们主要完成了哪些功能？

**结论**：加电后 CPU 处于 M 态（机器模式），PC 被复位到 **0x1000**。这里是 QEMU 在 MROM 中生成的复位代码，一共 6 条指令，作用是为下一阶段准备三个参数（`a0` = hart 编号，`a1` = 设备树地址，`a2` = 启动信息结构 `fw_dynamic_info` 的地址），然后跳转到 **0x80000000** 处的 OpenSBI 固件。OpenSBI 在 M 态完成平台初始化，最后通过一条 `mret` 指令降到 S 态（监管者模式），并跳转到 **0x80200000** 执行内核的 `kern_entry`。下面各节的每个结论都来自实际的 GDB 输出。



### 一、背景：启动链上的三个角色

RISC-V 有三个常用的特权级：**M 态**（Machine）权限最高，可以访问一切硬件；**S 态**（Supervisor）运行操作系统内核；**U 态**（User）运行用户程序。CPU 加电时处于 M 态，内核运行在 S 态，所以在两者之间需要一个运行在 M 态的固件来交接，这个固件就是 **OpenSBI**。它在启动时完成硬件初始化，然后把 CPU 降到 S 态交给内核；内核运行期间，它常驻在 M 态，为内核提供输出字符、设置定时器等与平台相关的服务。内核通过 `ecall` 指令请求这些服务，这套 S 态与 M 态之间的调用约定称为 **SBI**（Supervisor Binary Interface）。

整个启动过程可以概括为下图：左边是这几个参与者在物理内存中的位置，右边是控制权在它们之间的传递顺序。

![启动流程与物理内存布局](./images/T2-0-boot-overview.png)

<p align="center">图 2　lab1 的启动流程与物理内存布局</p>

调试环境为 WSL Ubuntu，QEMU 8.2.2（自带 OpenSBI v1.3），GDB 使用 `riscv64-unknown-elf-gdb`。



### 二、阶段一：复位与 MROM（0x1000）

GDB 连上之后，先确认 CPU 的初始状态，再反汇编 PC 处的指令，并查看紧跟在指令后面的数据区：

![GDB 连接后停在 0x1000](./images/T2-1-reset-0x1000.png)

<p align="center">图 3　复位后的第一条指令位于 0x1000，CPU 处于 M 态</p>

`priv` 为 3，表示 CPU 处于 M 态；PC 是 0x1000，这里只有 6 条指令。逐条单步并观察寄存器，各条指令的作用如下：

| 地址 | 指令 | 作用 | 执行后 |
|------|------|------|--------|
| 0x1000 | `auipc t0,0x0` | 取当前 PC 作为基址，方便后面按相对地址取数据 | t0 = 0x1000 |
| 0x1004 | `addi a2,t0,40` | a2 = `fw_dynamic_info` 的地址 | a2 = 0x1028 |
| 0x1008 | `csrr a0,mhartid` | a0 = 当前 hart 的编号 | a0 = 0 |
| 0x100c | `ld a1,32(t0)` | 从 0x1020 读出设备树地址 | a1 = 0x87e00000 |
| 0x1010 | `ld t0,24(t0)` | 从 0x1018 读出固件入口 | t0 = 0x80000000 |
| 0x1014 | `jr t0` | 跳转到 OpenSBI | pc = 0x80000000，仍处于 M 态 |

图中 `x/2gx 0x1018` 和 `x/6gx 0x1028` 读出的是 QEMU 填在指令后面的数据。`fw_dynamic_info` 的 6 个字段依次是 magic（`0x4942534f`，即 ASCII 的 "OSBI"）、版本号 2、**下一阶段入口 next_addr = 0x80200000**、**下一阶段特权级 next_mode = 1（S 态）**、选项 0 和启动 hart 编号 0。OpenSBI 之所以知道内核在哪里、应该以什么特权级运行内核，靠的就是这张由 QEMU 填写、经 a2 传过去的表。

图中最后一条命令 `x/2i 0x80200000` 显示，**此时内核的第一条指令已经在 0x80200000 了**，而 CPU 一条指令都还没有执行。这个现象在第五节会进一步讨论。

> **版本差异**：在课程推荐的 QEMU 4.1 中（见其源码 `hw/riscv/virt.c`），复位代码只有 5 条指令，不设置 a2；配套的 OpenSBI 是 fw_jump 型，下一阶段地址在编译时就固定为 0x80200000，不需要 QEMU 传参。新版 QEMU 改用 fw_dynamic 型固件，入口地址改由 QEMU 动态传入，这也是原框架 Makefile 在新版 QEMU 上无法启动内核的原因：原框架用 `-device loader` 把镜像复制进内存，不会向 QEMU 登记入口地址，OpenSBI 拿到的 next_addr 为 0。我们因此把 Makefile 中 `qemu` 和 `debug` 两个目标改为 `-kernel bin/kernel`，由 QEMU 按 ELF 装载内核并传入入口地址，新旧版本的 QEMU 都能正常启动。



### 三、阶段二：OpenSBI 的初始化（0x80000000）

`jr t0` 之后，PC 来到 0x80000000。OpenSBI 入口处的指令与它的源码 `firmware/fw_base.S` 中的 `_start` 一一对应：

```
=> 0x80000000:	add	s0,a0,zero        ← 把 MROM 传来的 a0/a1/a2 保存到 s0/s1/s2
   0x80000004:	add	s1,a1,zero
   0x80000008:	add	s2,a2,zero
   0x8000000c:	jal	0x80000580        ← fw_boot_hart：从 fw_dynamic_info 中读出启动 hart
   ...
   0x80000034:	amoadd.w a6,a7,(a6)    ← 启动抽签：多核时只有第一个完成原子加的 hart 负责初始化
   0x80000038:	bnez	a6,0x800000da     ← 其余 hart 转去等待
```

此后 OpenSBI 还要完成一系列初始化，最后打印 banner 并跳转到内核。但 QEMU 自带的 OpenSBI 固件没有符号表，所以我们先从源码编译一份带符号的 OpenSBI，按函数名跟踪完整的初始化流程。

#### 3.1 用带符号的 OpenSBI 跟踪初始化流程

QEMU 8.2.2 自带的是 OpenSBI v1.3（banner 第一行）。我们从官方仓库取出 v1.3 的源码，按 QEMU 使用的配置（`PLATFORM=generic`，fw_dynamic 型）编译。

下图在 `sbi_hart_init`（探测 CPU 特性）和 `sbi_hart_switch_mode`（交接给内核）两处断下：

![用带符号的 OpenSBI 跟踪初始化](./images/T2-5-opensbi-symbols.png)

<p align="center">图 4　带符号的 OpenSBI：sbi_hart_init 的调用栈，以及交接函数 sbi_hart_switch_mode 的参数</p>

`sbi_hart_switch_mode` 的参数就是交接信息：`arg0 = 0`（hart 编号）、`arg1 = 2279604224`（即 0x87e00000，设备树地址）、`next_addr = 0x80200000`、`next_mode = 1`（S 态），与 MROM 通过 `fw_dynamic_info` 传入的内容一致。

在各个关键函数上设断点并依次运行，得到 OpenSBI 从入口到交接的完整流程：

| 阶段 | 函数 | 作用 |
|------|----------------|------|
| asm | `_start`（firmware/fw_base.S） | 把 MROM 传来的 a0/a1/a2 保存到 s0/s1/s2 |
| | `fw_boot_hart`（firmware/fw_dynamic.S） | 从 `fw_dynamic_info` 中读出由哪个 hart 负责启动 |
| | 启动抽签、重定位检查、清零 .bss | 多核时选出唯一的冷启动 hart，并准备好 OpenSBI 自身的内存 |
| | `fw_save_info`（firmware/fw_dynamic.S） | 保存 next_addr、next_mode 等交接信息 |
| | `fw_platform_init`（platform/generic/platform.c） | 解析设备树（参数 arg1 = 0x87e00000），得到 hart 数量、内存范围、串口等平台信息 |
| | 设置每个 hart 的 scratch 区和栈，`mtvec = _trap_handler` | 为 C 代码准备运行环境，并设置 M 态的陷阱入口 |
| | `_start_warm → sbi_init`（lib/sbi/sbi_init.c） | 进入 C 代码，冷启动 hart 执行 `init_coldboot` |
| C | `sbi_scratch_init` | 初始化每个 hart 的私有数据区 |
| | `sbi_heap_init` | 初始化 OpenSBI 内部的小堆（banner 中的 Heap） |
| | `sbi_domain_init` | 建立 root 域，划定各内存区域及其访问权限 |
| | `sbi_hsm_init` | 初始化 hart 状态管理（HSM），之后内核可以通过 SBI 启停其他 hart |
| | `sbi_hart_init` | 探测 CPU 特性：PMP 表项数、性能计数器数量和若干可选扩展 |
| | `sbi_console_init` | 初始化 uart8250 串口，从此可以打印；其间还会探测 semihosting |
| | `sbi_pmu_init` / `sbi_irqchip_init` / `sbi_ipi_init` / `sbi_tlb_init` / `sbi_timer_init` | 依次初始化性能计数器、中断控制器、核间中断、远程 TLB 刷新和定时器 |
| | `sbi_domain_finalize` | 确定域的最终配置，包括下一阶段的入口地址和特权级 |
| | `sbi_hart_pmp_configure` | 写 PMP 寄存器，禁止 S/U 态访问 OpenSBI 自己的内存 |
| | `sbi_ecall_init` | 注册各类 SBI 服务（控制台、定时器、核间中断等），供内核用 `ecall` 调用 |
| | `sbi_boot_print_*` | 打印 banner，其中 MIDELEG/MEDELEG 两行由 `sbi_hart_delegation_dump` 打印 |
| 交接 | `sbi_hsm_hart_start_finish → sbi_hart_switch_mode` | 设置 `mstatus.MPP` 和 `mepc`，执行 `mret` 进入 S 态 |

需要说明的是，这份固件与 QEMU 自带的固件**逻辑相同，但内存布局不同**：前者大小为 190 KB，读写区从 0x80020000 开始；后者为 322 KB，读写区从 0x80040000 开始。因此函数地址、寄存器分配等细节不同。本节其余部分都以 QEMU 自带的固件为准。

#### 3.2 七次 mret

不论使用哪份固件，OpenSBI 把控制权交给内核时，都必须用 `mret` 完成从 M 态到 S 态的切换。

对于没有符号的自带固件，我们先用 `objdump` 反汇编，找出其中全部 5 条 `mret` 指令，在这些地址上都设置断点，并让 GDB 在每次命中时自动打印 `mepc`（mret 将要跳往的地址）和 `mstatus.MPP`（mret 将要切换到的特权级），然后继续运行：

```
mret#1 @0x8000c67a : mepc=0x80009576 MPP=3 mcause=0x2 mtval=0x3c002873
mret#2 @0x8000c67a : mepc=0x8000aa2e MPP=3 mcause=0x2 mtval=0xb1302873
mret#3 @0x8000c67a : mepc=0x8000a412 MPP=3 mcause=0x2 mtval=0xda002573
mret#4 @0x8000c67a : mepc=0x8000a456 MPP=3 mcause=0x2 mtval=0xfb002573
mret#5 @0x8000c67a : mepc=0x8000a4aa MPP=3 mcause=0x2 mtval=0x30c02673
mret#6 @0x8000d910 : mepc=0x8000d928 MPP=3 mcause=0x3 mtval=0
mret#7 @0x8000aec8 : mepc=0x80200000 MPP=1 mcause=0x3 mtval=0
  ^ 这次 mret 跳往内核。medeleg=0xf0b509 mideleg=0x1666 a0=0 a1=0x87e00000
```

`mret` 一共执行了 7 次，只有最后一次是交给内核的。

- **第 1～5 次**（mcause = 2，非法指令）：发生在 `sbi_hart_init` 中。OpenSBI 在**探测 CPU 实现了哪些 CSR**：故意读取可能不存在的 CSR（PMP 地址寄存器、性能计数器和几个可选扩展的寄存器），触发非法指令异常就说明该 CSR 不存在。

- **第 6 次**（mcause = 3，断点）：断点前后的代码是 `slli zero,zero,0x1f; ebreak; srai zero,zero,0x7`，这是 RISC-V **semihosting**（让被调试程序借用宿主机输入输出的机制）的固定指令序列。`semihosting_enabled()` 先临时替换 `mtvec`，再执行一次 `ebreak`：如果调试器或模拟器接管了这个 ebreak，就说明 semihosting 可用。QEMU 没有开启 semihosting，所以产生了断点异常，处理代码把 mepc 加 4 后返回；OpenSBI 因此改用 uart8250 串口作为控制台，也不会调用 `semihosting_init`。
- **第 7 次**（MPP = 1）：在 `sbi_hart_switch_mode` 中，**从 M 态进入 S 态，把控制权交给内核**。



### 四、阶段三：mret 交接与进入内核（0x80200000）

我们单独在第 7 次 mret 的地址 0x8000aec8 处下断点，观察交接的完整过程：

![在交接的 mret 处断下，执行后进入 S 态的 kern_entry](./images/T2-2-mret-to-kernel.png)

<p align="center">图 5　OpenSBI 通过 mret 把控制权交给内核（左栏为此时已打印的 OpenSBI banner）</p>

右栏自上而下可以看到交接的全过程：

1. **交接前的准备**：`x/7i` 显示 mret 前面的几条指令，`csrw mepc,s3` 把 mepc 设为下一阶段的入口，`mv a0,s4` / `mv a1,s5` 准备传给内核的两个参数。它前面还有一条 `csrw mstatus`，用来把 MPP 设为 S 态。这段逻辑与 OpenSBI 源码中的 `sbi_hart_switch_mode()` 一致。此时 `mepc = 0x80200000`，`mstatus.MPP = 1`，CPU 仍处于 M 态。
2. **执行 mret**：`si` 之后 PC 跳到 `kern_entry`，`priv` 变为 1，即 **S 态**。这就是**跳转到 0x80200000**的确切时刻和方式。
3. **内核看到的初始状态**：`a0 = 0` 是 hart 编号，`a1 = 0x87e00000` 指向设备树。值得注意的是 **`sp = 0x80046eb0`，仍然是 OpenSBI 自己的栈**。左栏的 banner 显示，`0x80040000-0x8005ffff`（Domain0 Region01）对 S/U 态没有任何权限（`S/U: ()`），访问会被 PMP 拦截。所以内核如果直接用这个 sp 压栈，就会触发访问异常。这从另一个角度说明了练习 1 中 `la sp, bootstacktop` 为什么必须是内核的第一件事。
4. **切换到内核栈**：再单步两条指令（`la` 伪指令展开成 `auipc` + `mv` 两条），`sp` 变为 `0x80203000`，即 `bootstacktop`。
5. 之后继续执行  `entry.S` 和 `init.c`



### 五、关于指导书提示的勘误

指导书的提示称“SBI 固件进行主初始化，其核心任务之一是将内核加载到 0x80200000，可以使用 `watch *0x80200000` 观察内核加载瞬间”。我们按提示做了实验：

![watch 实验：watchpoint 从未触发](./images/T2-3-watch.png)

<p align="center">图 6　在 0x1000 处设置 watchpoint，运行后只命中了内核入口的断点</p>

GDB 停在 0x1000 时，0x80200000 处已经是 `kern_entry` 的指令；设置硬件 watchpoint 后继续运行，直接命中了内核入口的断点，watchpoint 从未被触发。

原因在于，**内核不是由 OpenSBI 加载的，而是 QEMU 在虚拟机复位、CPU 开始执行之前就直接写进了模拟内存**（`-kernel` 和 `-device loader` 两种参数都是这样）。OpenSBI 只负责跳转，不负责加载，所以 watch point 不被触发。同理，提示中说 0x1000 处执行的是“OpenSBI 的汇编代码”也不准确：0x1000 处是 QEMU 生成的 MROM 复位代码，OpenSBI 从 0x80000000 才开始执行。

那么在**真实硬件**上，内核是由谁加载的？以 SiFive HiFive Unmatched、StarFive VisionFive 2 等常见 RISC-V 开发板为例，典型的启动链是：

```
片上 ROM（ZSBL） → U-Boot SPL → OpenSBI（M 态） → U-Boot（S 态） → Linux 内核（S 态）
```

片上 ROM 从 SPI Flash 或 SD 卡读入 U-Boot SPL；SPL 初始化 DRAM 后，把 OpenSBI 和 U-Boot 一并读入内存；OpenSBI 完成 M 态初始化后跳转到 U-Boot；U-Boot 具备存储、文件系统和网络驱动，由它从磁盘或网络读入内核镜像和设备树，再跳转到内核。可见，在真机上 OpenSBI 同样**只负责初始化和跳转，不负责从存储设备读取内核**：它本身没有磁盘和文件系统驱动，“加载”是由它前后的引导程序（SPL、U-Boot）完成的。QEMU 实验中把这些引导程序都省掉了，由 QEMU 自己完成“读入内存”这一步。



### 六、延伸：内核如何使用 OpenSBI 的服务

内核启动后，OpenSBI 并没有退场。`kern_init` 调用 `cprintf` 输出字符串，最终会走到 `sbi_console_putchar` 中的 `ecall`。我们在这条 `ecall` 上下断点：

![内核通过 ecall 陷入 OpenSBI 请求输出字符](./images/T2-4-ecall.png)

<p align="center">图 7　内核第一次调用 SBI：S 态 ecall 陷入 M 态，处理完成后返回</p>

- 断下时 CPU 处于 S 态，`a7 = 1` 是 SBI 调用号 `SBI_CONSOLE_PUTCHAR`，`a0 = 0x28` 是要输出的字符 `(`，也就是 `(THU.CST) os is loading ...` 的第一个字符。
- 执行 `ecall` 后，PC 跳到 `mtvec` 指向的 0x80000428（OpenSBI 的陷阱入口），特权级变为 M 态，`mcause = 9` 表示“来自 S 态的 ecall”，`mepc` 记录了 ecall 的地址，以便返回。
- OpenSBI 处理完请求后用 `mret` 返回，内核从 `ecall` 的下一条指令（0x80200470）继续执行，特权级恢复为 S 态。此时左栏 QEMU 的输出中出现了一个 `(`，正是这次调用输出的字符。

这说明 `cprintf` 能在终端上显示文字，是因为内核把“向串口写字符”这件事委托给了 M 态的固件，内核自己并不直接操作串口

借助带符号的固件，还可以看到 OpenSBI 内部是怎样处理这次调用的。我们换上带符号的固件，先运行到内核入口，再在串口驱动的输出函数 `uart8250_putc` 上下断点：

![带符号的 OpenSBI 中，ecall 的完整处理链](./images/T2-6-ecall-chain.png)

<p align="center">图 8　内核的 ecall 在 OpenSBI 内部的处理链，最终由 uart8250_putc 写出字符 '('</p>

调用栈自下而上依次是：

```
_trap_handler          (fw_base.S)        保存内核的寄存器现场
 → sbi_trap_handler    (sbi_trap.c)       按 mcause = 9 判定为来自 S 态的 ecall
 → sbi_ecall_handler   (sbi_ecall.c)      按 a7 = 1 找到对应的 SBI 扩展
 → sbi_ecall_legacy_handler (sbi_ecall_legacy.c)  CONSOLE_PUTCHAR 属于 SBI v0.1 的旧式调用
 → sbi_putc            (sbi_console.c)    控制台抽象层
 → uart8250_putc('(')  (uart8250.c)       等待发送寄存器空闲，把字符写入 UART
```

可见从内核的一条 `ecall` 到字符真正出现在终端上，中间经过了 OpenSBI 的陷阱入口、SBI 调用分发和串口驱动三层。内核只需要知道“调用号 1 表示输出一个字符”，串口是什么型号、寄存器在哪个地址，这些平台细节都由 OpenSBI 屏蔽了。这正是 SBI 作为**内核与固件之间的接口**的意义。




## 核心模块理解

**负责人：** 2414099－李云鹏

### 功能模块：链接脚本与内核内存布局

链接脚本`kernel.ld` 把各目标文件的代码和数据组织成内核 ELF，确定入口及各段的地址。

```ld
OUTPUT_ARCH(riscv)
ENTRY(kern_entry)

BASE_ADDRESS = 0x80200000;
```

`ENTRY(kern_entry)` 指定 ELF 的入口字段，`. = BASE_ADDRESS` 则让接下来的 `.text` 从 `0x80200000` 开始。二者不是同一件事：入口字段告诉装载器从哪里执行，位置计数器决定链接布局。

为什么 `kern_entry` 恰好在最前面？`entry.S` 使用的是普通 `.text`，而 `make print-kobjs` 输出的第一个文件是 `obj/kern/init/entry.o`，之后才是 `init.o`、`stdio.o` 等。链接器收集这些输入节时，先放入 `entry.o` 的入口代码，所以 `kern_entry` 的地址就是 `.text` 的起点。仅有 `ENTRY` 并不会把这个函数移到最前面。

脚本的安排顺序是 `.text → .rodata → 页对齐 → .data → .sdata → .bss` 。用 SiFive GCC 10.2.0 构建后，`readelf -S` 和 `nm` 的结果如下：

| 区域 | 地址范围（左闭右开） | 本次内容 |
|---|---|---|
| `.text` | `[0x80200000,0x802004c8)` | 入口和函数机器码 |
| `.rodata` | `[0x802004c8,0x80200738)` | 字符串和格式化所需的只读数据 |
| 对齐空隙 | `[0x80200738,0x80201000)` | 后续数据按4 KiB对齐 |
| `.data` | `[0x80201000,0x80203000)` | 8 KiB启动栈 |
| `.sdata` | `[0x80203000,0x80203008)` | `SBI_CONSOLE_PUTCHAR` |
| BSS清零区间 | `[0x80203008,0x80203008)` | 空区间，`edata=end` |

下面的 `nm` 输出中可以直接核对入口、栈和数据边界。

![lyp本机内核符号表](./images/T3-symbols.png)

<p align="center">图 9　kern_entry、启动栈和数据边界的符号地址</p>

使用 `readelf -W -l bin/kernel` 查看内核 ELF 文件的程序头表，可以发现其中包含两个 `LOAD` 段。第一个段的起始物理地址为 `0x80200000`，包含 `.text` 和 `.rodata` 节，具有可读、可执行属性；第二个段的起始物理地址为 `0x80201000`，包含 `.data` 和 `.sdata` 节，具有可读、可写属性。通过 `readelf -W -S bin/kernel` 查看节头表，可以进一步验证各节的具体地址及其布局。此外，两个装载段的虚拟地址与物理地址相同。由于内核入口处 `satp=0`，说明当前仍处于未启用分页的 Bare 模式，因此这些地址直接对应物理内存，ELF 中的段权限也尚未通过页表落实为内存访问权限。

![lyp本机ELF程序头](./images/T3-elf-layout.png)

<p align="center">图 10　ELF 入口、两个 LOAD 段及节表</p>

### 功能模块：格式化输出与 SBI 服务

`kern_init` 调用 `cprintf("%s\n\n", message)` 后，加载提示便出现在终端上。沿着代码往下看，这次输出经过以下路径：

```text
cprintf → vcprintf → vprintfmt
  → cputch → cons_putc → sbi_console_putchar → sbi_call
    → ecall → OpenSBI控制台服务 → UART
```

主要函数为：

```c
int cprintf(const char *fmt, ...);
int vcprintf(const char *fmt, va_list ap);
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);
static void cputch(int c, int *cnt);
void cons_putc(int c);
void sbi_console_putchar(unsigned char ch);
uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0, uint64_t arg1, uint64_t arg2);
```

`cprintf` 首先通过 `va_start` 获取可变参数，再将格式字符串和参数交给 `vcprintf`。`vcprintf` 初始化字符计数器，并调用 `vprintfmt` 进行格式化处理。`vprintfmt` 负责解析 `%s` 等格式说明符，每生成一个字符，就调用回调函数 `cputch`。`cputch` 通过 `cons_putc` 将字符交给 SBI 输出，同时将字符计数加一。

采用回调函数的好处是将**格式化处理与实际输出分离**。`vprintfmt` 只负责生成字符，具体输出到哪里由回调函数决定。例如，`cprintf` 将字符输出到控制台，而 `vsnprintf` 则复用相同的格式化逻辑，将字符写入内存缓冲区。`cprintf` 最终返回的是已处理的字符数，而非设备实际成功输出的字符数。

`sbi_console_putchar` 选择旧式 SBI 的字符输出调用号1。`sbi_call` 用内联汇编把调用号放到 `a7`，字符放到 `a0`，其余参数放到 `a1/a2`，最后执行 `ecall`。

执行 `ecall` 前，格式化和字符处理都在 S 态完成。陷入后，处理器记录异常原因和返回地址，进入 OpenSBI 的 M 态陷阱入口。固件保存现场、分发字符输出请求，再由控制台服务调用 UART 驱动。处理结束后恢复现场，返回到 `ecall` 的下一条内核指令，继续输出后续字符。

为观察这次调用，本机在 `0x80200492` 的 `ecall` 处断下，再单步进入固件，最后在下一条内核指令 `0x80200496` 处断下。

![lyp本机SBI陷入与返回](./images/T3-ecall-step.png)

<p align="center">图 11　VSCode 终端中字符输出时 S→M→S 的 GDB 单步调试（QEMU 6.2.0 / OpenSBI v0.9）</p>

图中 `a7=1`、`a0=0x28`，表示请求输出字符 `(`。单步后 `priv` 从1变成3，`mcause=9`，`mepc` 记录 `0x80200492`，PC 到达 `mtvec` 指向的 `0x80000520`。返回内核时 PC 为 `0x80200496`，`priv` 又变成1。左栏此时出现了一个 `(`，与 `a0` 的字符值一致。这组寄存器变化对应了一次完整的固件调用。

### 功能模块：构建与镜像加载

执行 `make` 后，构建流程为：

```text
.c/.S → obj/.../*.o → bin/kernel → bin/ucore.img
```

`function.mk` 收集源文件并生成编译规则，Makefile把 kernel/libs 两组目标文件组成 `KOBJS`。`.S` 先由GCC预处理，因此能引用头文件中的栈大小宏。链接时，`ld -T tools/kernel.ld` 合并各节、确定地址和完成重定位，生成 ELF；`--gc-sections` 删除未使用的节。最后，`objcopy --strip-all -O binary` 生成裸镜像。

| 文件 | 内容 | 当前用途 |
|---|---|---|
| `bin/kernel` | ELF头、入口、程序头、装载内容和符号／调试信息 | QEMU按ELF装载，GDB读取符号 |
| `bin/ucore.img` | 内核内容字节及地址间隙的填充，无ELF元数据 | 原框架按指定地址加载的裸镜像 |

 `wc -c bin/ucore.img` 得到12296字节，即 `0x3008`，对应从 `0x80200000` 到 `0x80203008` 的跨度。ELF中还有头和调试信息，因此不能用它的文件大小直接表示内核占用的内存。

`make debug` 多加 `-s -S`，前者开启默认1234端口的GDB服务，后者让CPU启动时暂停；`make gdb` 读取 `bin/kernel` 的符号后连接QEMU。调试自编译固件时，两端传入相同的 `OPENSBI` 路径。


## 拓展：现代笔记本与 RISC-V 启动流程对比

本节以现代 x86/ARM 笔记本体系为对照标杆，深度剖析现代计算机从加电到操作系统接管的完整生命周期，并与练习 2 第五节总结的 RISC-V 真实硬件启动链（`ROM -> U-Boot SPL -> OpenSBI -> U-Boot -> Kernel`）进行横向对比分析。

### 现代 x86 笔记本的启动流程（UEFI）
现代 x86 体系（Intel/AMD 平台）已彻底淘汰传统 Legacy BIOS，严格遵循 UEFI 规范标准。其引导时序可划分为五个离散的逻辑阶段：

1. **SEC (Security) 阶段**：
   - 物理加电复位后，CPU 执行主板 SPI Flash ROM 中的只读初始化代码。
   - 此时物理内存（DRAM）尚未完成标定与时序训练，CPU 将内部 L1/L2 缓存配置为 **CAR (Cache-As-RAM)** 临时内存堆栈。
   - 确立系统的可信根（Root of Trust），校验后续阶段固件模块的公钥证书与签名。
2. **PEI (Pre-EFI Initialization) 阶段**：
   - 调度执行 PEI 核心模块（PEIM），完成芯片组、电源管理、系统时钟等基础硬件的标定。
   - 驱动内存控制器完成 DRAM 物理内存的通道扫描与时序训练，彻底激活物理内存。
   - 将已探测的硬件资源状态抽象封装为 **HOB (Hand-Off Block)** 数据结构列表，传递给下一阶段。
3. **DXE (Driver Execution Environment) 阶段**：
   - 在全量可用的物理内存中构建软硬件调度总线。
   - 并行调度加载数十至上百个 DXE 驱动（PCIe 总线、NVMe 固态存储、USB 控制器、图形 GOP 模块等），构建起 UEFI 运行时服务（Runtime Services）与引导服务（Boot Services）。
4. **BDS (Boot Device Selection) 阶段**：
   - 读取主板 NVRAM 中保存的启动项优先级策略。
   - 挂载 EFI 系统分区（ESP, FAT32 格式），检索引导文件。
5. **OS Loader 与内核接管**：
   - 加载操作系统的 EFI 引导加载器（如 Windows 的 `bootmgfw.efi` 或 Linux 的 `grubx64.efi`），校验 Secure Boot 签名。
   - 引导程序将内核镜像与 initramfs 装载入内存后，调用 UEFI 核心服务 `ExitBootServices()`。调用之后，UEFI 引导阶段临时占用的内存被彻底回收释放，硬件控制权完全移交操作系统内核（Ring 0）。


### 三种架构启动阶段的对照

将现代笔记本（x86 UEFI 与 ARM TF-A）的启动拓扑与练习 2 中总结的 RISC-V 真实硬件启动链进行对照：

| 引导阶段职责 | RISC-V 真机启动链 | 现代 x86 UEFI 笔记本 | 现代 ARM64 笔记本 (TF-A) |
| :--- | :--- | :--- | :--- |
| **阶段 1：物理根固件 (No-DRAM)** | **MaskROM** (芯片内部固化) | **SEC 阶段** (Flash ROM / CAR 模式) | **BL1 (MaskROM, EL3)** |
| **阶段 2：硬件标定与内存初始化** | **U-Boot SPL** (片内 SRAM 运行) | **PEI 阶段** (Memory Training) | **BL2 (S-EL1 / EL3)** |
| **阶段 3：底层特权级运行时监视器** | **OpenSBI** (常驻 M-Mode) | **SMM (System Management Mode)** | **BL31 (TF-A Monitor, EL3)** |
| **阶段 4：富外设驱动与系统引导** | **U-Boot (Full)** (运行于 S-Mode) | **DXE + BDS 阶段** | **BL33 (EDK2 / U-Boot, EL2)** |
| **阶段 5：操作系统内核接管** | **Kernel (uCore/Linux)** (S-Mode) | **OS Kernel (Linux/NT)** (Ring 0) | **OS Kernel (Linux/XNU)** (EL1) |
| **运行期服务请求通道** | **SBI 调用** (`ecall` 陷入 M 态) | **SMI 中断 / UEFI Runtime** | **SMC 调用** (Secure Monitor Call) |


### 共同的设计思路
通过跨架构对比，可以提炼出计算系统底层固件设计的普适性工业哲学：
1. **最小依赖原则（渐进式引导）**：无论是 x86 的 CAR 技术、ARM 的片内 SRAM 还是 RISC-V 的 SPL，初期都严格受限于物理硬件未激活状态，必须以“最小依赖”逐级点亮 DRAM 内存，再承载高层复杂驱动。
2. **特权级降级收敛与安全解耦**：最高特权级（M-Mode / EL3 / Ring -2 SMM）仅常驻精炼的硬件抽象与安全监控服务（如 OpenSBI、BL31）；富设备驱动与复杂加载逻辑下放至降权环境运行，最终将全部硬件资源无损交付给 OS 内核。




# 五、测试与验证

## make qemu 运行验证

**验证人**：2413074-刘昀皓

**环境**：WSL2 Ubuntu 22.04，QEMU 7.0.0（自带 OpenSBI v1.0）。在 `code/` 目录下执行 `make clean && make qemu`。

QEMU 启动后，CPU 先执行复位代码，再跳到 OpenSBI。OpenSBI 在 M 态完成平台初始化、配置 PMP 内存保护之后，按 QEMU 传入的启动信息（`Next Address = 0x80200000`，`Next Mode = S-mode`）执行 `mret`，切换到 S 态并跳到内核入口。内核执行 `kern_init`，通过 SBI 的字符输出服务在终端打印出加载信息，随后进入死循环，所以 QEMU 会一直运行，需要按 `Ctrl+A` 再按 `X` 退出。

![make qemu 运行截图](./images/T6-qemu-run.png)

<p align="center">图 12　make qemu 的运行结果（QEMU 7.0.0 / OpenSBI v1.0）</p>

截图中可以看到 OpenSBI 打印的平台信息，其中 `Domain0 Next Address` 为 `0x0000000080200000`、`Next Mode` 为 `S-mode`，最后一行是内核输出的 `(THU.CST) os is loading ...`，说明内核已经在 S 态正常运行。




# 六、实验总结与收获

## 对操作系统的理解

**负责人：** 2414099－李云鹏

### 1. 实验知识点与OS原理的对应

这次内核只打印了一条加载提示，之后就在循环里停住了。但从复位到这条输出，需要固件、入口汇编、链接布局和SBI接口一起配合。对照原理知识时，我们主要关注这些已经在代码和调试中出现的机制。

| 实验中的知识点 | 对应原理及二者的关系 |
|---|---|
| 复位代码 → OpenSBI → 内核入口 | 对应系统引导。原理中的阶段交接在这里表现为PC和特权级的变化。本配置由QEMU提前装载ELF，OpenSBI完成固件初始化与交接，实验没有实现从磁盘读取内核的引导程序。 |
| 内核在S态，通过 `ecall` 请求M态固件输出 | 对应特权级和受控调用。CPU记录异常原因和返回位置，固件处理后返回。它与用户系统调用的机制有关，但调用者和服务者不同：这里是S→M，还没有U态用户程序。 |
| 设置启动栈，按ABI进入C函数 | 对应执行上下文与栈。保存返回地址、局部数据等操作都需要可访问的栈；但当前只有启动栈，没有进程独立内核栈和上下文切换。 |
| 链接脚本排列代码与数据，启动时清零BSS | 对应程序布局和运行时初始化。链接器确定各部分地址，启动代码满足静态变量零初始化要求。本次BSS为空；页对齐和ELF段标志也没有自动建立页表保护。 |
| 编译、链接、生成镜像，再由QEMU装载 | 对应程序装入。ELF的程序头和入口为装载提供信息，裸镜像则需要外部指定地址。这次装入的是内核，还没有用户进程的加载和 `exec`。 |
| 格式化回调 → 控制台接口 → SBI → UART | 对应分层设计和设备抽象。格式化函数负责生成字符，固件负责访问串口。当前通路逐字符输出，没有内核缓冲队列或异步I/O管理。 |

其中最容易混淆的是页对齐与分页。代码里出现 `PGSIZE=4096`，栈也按页对齐，但入口处 `satp=0`，内核没有建立页表。这里的“页”先用于描述布局粒度，还没有承担地址转换和进程隔离的作用。

异常处理也要区分固件和内核。字符输出已经通过 `ecall` 触发了异常，OpenSBI负责处理；尚未实现的是内核自己的时钟中断、页故障等处理逻辑

### 2. 本实验没有覆盖的重要知识点

- **内存分配和虚拟内存。** 启动栈是静态预留的，没有空闲页管理、动态分配、页表、缺页处理和换页机制。
- **进程、线程与调度。** 没有PCB、运行队列、进程创建退出及上下文切换。`kern_init` 末尾的循环持续占用CPU，不等于休眠或调度空闲任务。
- **用户态与系统调用。** 没有加载用户程序，也没有系统调用分发、用户参数检查。现有SBI接口服务的是内核对固件的请求。
- **内核中断与完整设备管理。** `kbd_intr`、`serial_intr`、`cons_init` 都是空函数。虽然可以经固件输出字符，内核仍没有实现设备中断、DMA、请求队列及完成通知。
- **同步和通信。** 当前启动执行流没有使用锁、信号量或条件变量，也没有进程间通信。本次单hart运行不能验证多核并发。
- **文件系统与持久化。** 没有文件、目录、块分配和磁盘读写管理。

这些内容需要在后续实验中逐步补齐。Lab1能验证的是内核已经获得控制权、具有可用的启动栈，并能通过固件输出字符；它距离能运行用户程序的操作系统还有不少工作。


## 实验收获

### 启动流程与系统架构
通过 Lab 1 的全流程实践，本小组系统性地打通了 RISC-V 体系架构下操作系统启动的微观硬件行为与宏观软件分层逻辑：
1. **固件与操作系统内核的交接边界**：从加电复位地址 `0x1000` 到 OpenSBI 的 `0x80000000`，再到操作系统入口 `0x80200000`，团队通过 GDB 指令级跟踪彻底厘清了 CPU 特权级（M 态至 S 态）由 `mret` 触发的降权跃迁过程，以及核心参数（`a0` Hart ID、`a1` DTB 地址）的传递约定。
2. **硬件模拟与真实物理系统的映射**：明确了 QEMU `-kernel` 机制直接注入 ELF 镜像的快捷特性，同时通过拓展题横向推导，对齐了真实硬件环境（MaskROM -> U-Boot SPL -> OpenSBI -> U-Boot -> Kernel）与现代 x86 UEFI / ARM TF-A 的引导链路，提炼出固件“最小依赖渐进引导”与“特权级降级收敛”的通用设计模式。
3. **特权服务代理机制**：加深了对 SBI 规范的认知。内核在 S 态通过 `ecall` 请求 M 态 OpenSBI 代为执行串口字符打印等底层敏感操作，实现了平台相关性与操作系统核心逻辑的优雅解耦。

### 分工协作
本实验全面推行基于 `.handoff` 任务切片与模块化合并的团队协同流：
1. **接口与文件锁契约**：团队严格恪守“各任务仅操作自身名下文件”的原则，报告采用独立 section 分拆撰写，图片统一在 `report/images/` 归档并使用 `../images/` 相对路径引用，规避了多人同时编辑同一大文件时的 Git 合并冲突。
2. **多版本兼容性考量**：三位成员分别使用 QEMU 6.2、7.0 和 8.2，面对不同版本的环境时，团队及时通过参数重构与实测印证，确保了构建链与调试链的健壮性。

## AI 协作开发的经验

在借助 AI 工具（Claude Code、Codex、Claude 和 DeepSeek 的网页对话等）进行工程推进与文档整理的过程中，团队达成了高度一致的人机协同共识：
- **人类主导逻辑审查，防范底层时序幻觉**：大语言模型在处理底层体系结构细节（如 CSR 寄存器自动更新时序、栈顶符号地址绑定）时存在潜在的语义泛化偏差。团队坚持以官方硬件手册（*RISC-V Privileged Architecture Manual*）与真实 GDB 寄存器倾倒数据为唯一检验标准，避免盲目采纳未经求证的代码与论述。
- **结构化 Prompt 驱动**：采用指导书推行的标准四段式结构，将可信上下文（`[RELY]`）与验收标准（`[GUARANTEE]`）严格限定在最小必要范围内，显著提升了 Agent 产出物的精度与工程契合度。
