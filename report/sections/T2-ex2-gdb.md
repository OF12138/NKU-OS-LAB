## 练习2：使用 GDB 验证启动流程

**负责人：** openfar

### 问题与结论

> 使用 GDB 跟踪 QEMU 模拟的 RISC-V 从加电开始，直到执行内核第一条指令（跳转到 0x80200000）的整个过程。RISC-V 硬件加电后最初执行的几条指令位于什么地址？它们主要完成了哪些功能？

**结论**：加电后 CPU 处于 M 态（机器模式），PC 被复位到 **0x1000**。这里是 QEMU 在 MROM 中生成的复位代码，只有 6 条指令，作用是为下一阶段准备三个参数：`a0` = 当前 hart（硬件线程）的编号，`a1` = 设备树（DTB）地址，`a2` = 传给 OpenSBI 的启动信息结构 `fw_dynamic_info` 的地址。随后跳转到 **0x80000000**，执行 OpenSBI 固件。OpenSBI 在 M 态完成平台初始化，最后通过一条 `mret` 指令切换到 S 态（监管者模式），并跳转到 **0x80200000**，开始执行内核的 `kern_entry`。

整个过程是 **MROM（M 态） → OpenSBI（M 态） → 内核（S 态）** 的三段接力。下面每个结论都来自实际的 GDB 输出。

### 背景：启动链上的三个角色

| 阶段 | 地址 | 特权级 | 由谁提供 | 职责 |
|------|------|--------|----------|------|
| MROM 复位代码 | 0x1000 | M | QEMU 启动时生成 | 准备参数，跳转到固件 |
| OpenSBI | 0x80000000 | M | QEMU 自带的固件（`-bios default`） | 初始化硬件、设置中断委托和内存保护，并在之后为内核提供 SBI 服务 |
| ucore 内核 | 0x80200000 | S | 我们编译的 `bin/kernel` | 操作系统本身 |

RISC-V 有三个常用特权级：**M 态**（Machine，最高级，可以访问一切硬件）、**S 态**（Supervisor，操作系统内核运行在这里）和 **U 态**（User，用户程序）。OpenSBI 常驻在 M 态，相当于内核下面的一层“固件服务层”。内核需要做输出字符、设置定时器这类与平台相关的事情时，执行 `ecall` 陷入 M 态，请 OpenSBI 代劳。这套 S 态与 M 态之间的调用约定就是 **SBI**（Supervisor Binary Interface）。

### 调试环境与方法

- 环境：WSL Ubuntu，QEMU 8.2.2（自带 OpenSBI v1.3），工具链为 `riscv64-unknown-elf-gdb`。
- 方法：一个终端运行 `make debug`（QEMU 加了 `-s -S` 参数，CPU 停在第一条指令前，并在 1234 端口等待 GDB 连接），另一个终端运行 `make gdb` 连接上去。
- OpenSBI 的固件 ELF（`/usr/share/qemu/opensbi-riscv64-generic-fw_dynamic.elf`）没有符号表，所以只能按地址跟踪。我们先用 objdump 把它反汇编，找出其中全部 5 条 `mret` 指令的地址，在这些地址上下断点，以此捕捉“特权级切换”这个关键时刻。

### 阶段一：复位与 MROM（0x1000）

GDB 刚连上时：

```
(gdb) info registers pc priv
pc             0x1000	0x1000
priv           0x3	prv:3 [Machine]
(gdb) x/6i $pc
=> 0x1000:	auipc	t0,0x0
   0x1004:	addi	a2,t0,40
   0x1008:	csrr	a0,mhartid
   0x100c:	ld	a1,32(t0)
   0x1010:	ld	t0,24(t0)
   0x1014:	jr	t0
```

逐条单步，结合寄存器的变化，各条指令的作用如下：

| 地址 | 指令 | 作用 | 执行后 |
|------|------|------|--------|
| 0x1000 | `auipc t0,0x0` | 取当前 PC 作为基址 | t0 = 0x1000 |
| 0x1004 | `addi a2,t0,40` | a2 = `fw_dynamic_info` 的地址 | a2 = 0x1028 |
| 0x1008 | `csrr a0,mhartid` | a0 = 当前 hart 编号 | a0 = 0 |
| 0x100c | `ld a1,32(t0)` | 从 0x1020 读出设备树地址 | a1 = 0x87e00000 |
| 0x1010 | `ld t0,24(t0)` | 从 0x1018 读出固件入口 | t0 = 0x80000000 |
| 0x1014 | `jr t0` | 跳到 OpenSBI | pc = 0x80000000，仍是 M 态 |

这 6 条指令后面跟着的是 QEMU 填好的数据：

```
(gdb) x/2gx 0x1018
0x1018:	0x0000000080000000	0x0000000087e00000      ← 固件入口、设备树地址
(gdb) x/6gx 0x1028                                     ← fw_dynamic_info
0x1028:	0x000000004942534f	0x0000000000000002      ← magic = "OSBI"，version = 2
0x1038:	0x0000000080200000	0x0000000000000001      ← next_addr = 0x80200000，next_mode = 1 (S 态)
0x1048:	0x0000000000000000	0x0000000000000000      ← options，boot_hart = 0
```

`fw_dynamic_info` 说明了 OpenSBI 是怎么知道内核在哪里、该用什么特权级运行内核的：这两项信息由 QEMU 在启动时填好，再由 MROM 通过 a2 传过去。

### 阶段二：OpenSBI 的初始化（0x80000000）

OpenSBI 入口处的指令和它的源码 `firmware/fw_base.S` 中的 `_start` 一一对应：

```
=> 0x80000000:	add	s0,a0,zero        ← 把 MROM 传来的 a0/a1/a2 存到 s0/s1/s2
   0x80000004:	add	s1,a1,zero
   0x80000008:	add	s2,a2,zero
   0x8000000c:	jal	0x80000580        ← fw_boot_hart：从 fw_dynamic_info 中读出启动 hart
   ...
   0x80000034:	amoadd.w a6,a7,(a6)    ← 启动抽签：多核时只有第一个原子加成功的 hart 负责初始化
   0x80000038:	bnez	a6,0x800000da     ← 其他 hart 转去等待
```

之后 OpenSBI 依次完成重定位检查、清零 .bss、设置陷阱入口 `mtvec`、解析设备树、初始化串口和定时器、配置中断委托与 PMP 内存保护等工作，最后打印我们在 `make qemu` 中看到的 OpenSBI banner。

由于 OpenSBI 没有符号，逐条单步要走上万条指令，并不现实。我们改为在全部 `mret` 上设置断点，每次命中时自动打印 `mepc`（mret 将跳往的地址）和 `mstatus.MPP`（mret 将切换到的特权级），然后继续运行：

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

这组数据揭示了 OpenSBI 初始化过程中一个有意思的细节：

- **第 1～5 次（mcause = 2，非法指令）**：OpenSBI 在**试探 CPU 实现了哪些 CSR**。它故意读一个可能不存在的 CSR：如果陷入了非法指令异常，就说明这个 CSR 不存在，异常处理程序记下结果后用 mret 返回（M 态返回 M 态）。把 mtval 中记录的指令编码解码后，被试探的 CSR 依次是：`pmpaddr16`（0x3c0，探测 PMP 表项数，结果为 16，与 banner 中的 `PMP Count : 16` 一致）、`mhpmcounter19`（0xb13，探测性能计数器数量，与 `MHPM Count : 16` 一致）、`scountovf`（0xda0）、`mtopi`（0xfb0）、`mstateen0`（0x30c），分别对应几项可选扩展。
- **第 6 次（mcause = 3，断点）**：断点前后的代码是 `slli zero,zero,0x1f; ebreak; srai zero,zero,0x7`，这是 RISC-V **semihosting**（让被调试程序借用宿主机 I/O 的机制）的固定指令序列。OpenSBI 临时替换了 `mtvec`，执行一次 `ebreak` 来探测 semihosting 是否可用。QEMU 没有开启 semihosting，所以产生了断点异常，处理程序把 `mepc` 加 4 之后返回。
- **第 7 次（MPP = 1）**：这是**从 M 态进入 S 态、把控制权交给内核**的那一次 mret。它前面的代码依次是 `csrw mstatus`（把 MPP 设为 S 态）、`csrw mepc,s3`（mepc = 0x80200000）、`mv a0,s4` / `mv a1,s5`（传入 hartid 和设备树地址），与 OpenSBI 源码中 `sbi_hart_switch_mode()` 的逻辑一致。

交给内核之前，OpenSBI 设置好的**异常委托**也值得一看：

- `medeleg = 0xf0b509`：指令地址未对齐、断点、U 态 `ecall`、各类页错误等异常直接交给 S 态（内核）处理。**S 态的 `ecall`（第 9 位）没有被委托**，所以内核执行 `ecall` 时会陷入 M 态，由 OpenSBI 处理，SBI 调用就是这样实现的。
- `mideleg = 0x1666`：S 态的软件中断、时钟中断和外部中断（第 1、5、9 位）委托给内核，其余几位与虚拟化扩展有关。

### 阶段三：进入内核（0x80200000）

执行完第 7 次 mret 后，停在内核入口：

```
Breakpoint 6, kern_entry () at kern/init/entry.S:7
7	    la sp, bootstacktop
(gdb) info registers pc priv a0 a1 sp satp
pc             0x80200000	0x80200000 <kern_entry>
priv           0x1	prv:1 [Supervisor]
a0             0x0	0
a1             0x87e00000	2279604224
sp             0x80046eb0	0x80046eb0
satp           0x0	0
(gdb) x/wx $a1
0x87e00000:	0xedfe0dd0
```

- 特权级已经变成 **S 态**；`satp = 0` 说明分页还没有开启，内核目前直接使用物理地址。
- `a0 = 0` 是 hartid，`a1` 指向设备树（读出的 `0xedfe0dd0` 是设备树魔数 `0xd00dfeed` 按小端序显示的结果）。这两个参数要到后续实验才会用到。
- **`sp = 0x80046eb0` 仍然是 OpenSBI 自己的栈**。OpenSBI 的 banner 显示，`0x80040000-0x8005ffff` 这个区域对 S/U 态没有任何权限（`S/U: ()`），会被 PMP 拦截。所以内核如果直接用这个 sp 压栈，就会触发访问异常。这正好从另一个角度说明了练习 1 的结论：`kern_entry` 的第一件事必须是 `la sp, bootstacktop`，换成内核自己的栈。单步执行这两条指令后，`sp = 0x80203000`，也就是 `bootstacktop`；栈底 `bootstack = 0x80201000`，两者正好相差 8 KB（`KSTACKSIZE`）。

### 对指导书提示的勘误

指导书的提示称“SBI 固件进行主初始化，其核心任务之一是将内核加载到 0x80200000，可以使用 `watch *0x80200000` 观察内核加载瞬间”。实测结果与此不符：

```
(gdb) info registers pc
pc             0x1000	0x1000
(gdb) x/2i 0x80200000
   0x80200000 <kern_entry>:	auipc	sp,0x3        ← CPU 还没有执行任何指令，内核已经在内存里了
   0x80200004 <kern_entry+4>:	mv	sp,sp
(gdb) watch *(unsigned int *)0x80200000
Hardware watchpoint 1: *(unsigned int *)0x80200000
(gdb) b *0x80200000
(gdb) c
Breakpoint 2, kern_entry () at kern/init/entry.S:7    ← watch 从未触发，直接命中内核入口断点
```

内核是由 **QEMU 在虚拟机复位、CPU 开始执行之前**直接写入模拟内存的（`-kernel` 或 `-device loader` 参数）。OpenSBI 只负责“跳转”，不负责“加载”，因此 watch 永远等不到写入。我们对框架原本的 `-device loader` 方式也做了同样的实验，结果相同。另外，提示中说 0x1000 处执行的是“OpenSBI 的汇编代码”，同样不准确：0x1000 处是 QEMU 生成的 MROM 复位代码，OpenSBI 从 0x80000000 才开始执行。

在真实硬件上，“加载内核”通常由片上 ROM 或 U-Boot 这类引导程序完成，从存储设备读入内存；QEMU 把这一步省掉了。

### 延伸：内核如何使用 OpenSBI 的服务

内核启动后，OpenSBI 并没有退场。在 `sbi_console_putchar` 中的 `ecall` 处下断点：

```
(gdb) info registers pc priv a7 a0
pc             0x8020046c	<sbi_console_putchar+18>
priv           0x1	prv:1 [Supervisor]
a7             0x1	1          ← SBI 调用号 1 = CONSOLE_PUTCHAR
a0             0x28	40         ← 要输出的字符 '('，也就是 "(THU.CST)" 的第一个字符
(gdb) si
(gdb) info registers pc priv mcause mepc mtvec
pc             0x80000428
priv           0x3	prv:3 [Machine]   ← 陷入 M 态
mcause         0x9	9                 ← 9 = 来自 S 态的 ecall
mepc           0x8020046c             ← 记录返回位置
mtvec          0x80000428             ← OpenSBI 的陷阱入口
```

OpenSBI 处理完请求、把字符写到 uart8250 串口之后，再用 mret 回到 S 态，内核从 `ecall` 的下一条指令（0x80200470）继续执行。这就是 `cprintf` 最终能在终端上显示文字的原因：内核自己并不直接操作串口，而是委托 M 态的固件来完成。

### 调试截图

![复位后停在 0x1000：MROM 指令、M 态、内核已在 0x80200000](./images/T2-1-reset-0x1000.png)

![最后一次 mret：mepc=0x80200000、MPP=1，执行后进入 S 态的 kern_entry](./images/T2-2-mret-to-kernel.png)

![watch 实验：watchpoint 从未触发，直接命中内核入口断点](./images/T2-3-watch.png)
