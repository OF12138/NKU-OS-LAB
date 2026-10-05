## 练习2：使用 GDB 验证启动流程

**负责人：** openfar

> **题目**：使用 GDB 跟踪 QEMU 模拟的 RISC-V 从加电开始，直到执行内核第一条指令（跳转到 0x80200000）的整个过程。RISC-V 硬件加电后最初执行的几条指令位于什么地址？它们主要完成了哪些功能？

**结论**：加电后 CPU 处于 M 态（机器模式），PC 被复位到 **0x1000**。这里是 QEMU 在 MROM 中生成的复位代码，一共 6 条指令，作用是为下一阶段准备三个参数（`a0` = hart 编号，`a1` = 设备树地址，`a2` = 启动信息结构 `fw_dynamic_info` 的地址），然后跳转到 **0x80000000** 处的 OpenSBI 固件。OpenSBI 在 M 态完成平台初始化，最后通过一条 `mret` 指令降到 S 态（监管者模式），并跳转到 **0x80200000** 执行内核的 `kern_entry`。下面各节的每个结论都来自实际的 GDB 输出。

### 一、背景：启动链上的三个角色

RISC-V 有三个常用的特权级：**M 态**（Machine）权限最高，可以访问一切硬件；**S 态**（Supervisor）运行操作系统内核；**U 态**（User）运行用户程序。CPU 加电时处于 M 态，内核运行在 S 态，所以在两者之间需要一个运行在 M 态的固件来“交接”，这个固件就是 **OpenSBI**。它在启动时完成硬件初始化，然后把 CPU 降到 S 态交给内核；内核运行期间，它常驻在 M 态，为内核提供输出字符、设置定时器等与平台相关的服务。内核通过 `ecall` 指令请求这些服务，这套 S 态与 M 态之间的调用约定称为 **SBI**（Supervisor Binary Interface）。

整个启动过程可以概括为下图：左边是这几个参与者在物理内存中的位置，右边是控制权在它们之间的传递顺序。

![启动流程与物理内存布局](./images/T2-0-boot-overview.png)

<center>图 2-1　lab1 的启动流程与物理内存布局</center>

调试环境为 WSL Ubuntu，QEMU 8.2.2（自带 OpenSBI v1.3），GDB 使用工具链自带的 `riscv64-unknown-elf-gdb`。调试方法是在一个终端运行 `make debug`：QEMU 带 `-s -S` 参数启动，CPU 停在第一条指令之前，并在 1234 端口等待 GDB 连接。然后在另一个终端运行 `make gdb` 连接上去。为了同时看到 QEMU 的输出和 GDB 的输出，我们用 tmux 把终端分成左右两栏，下文截图的左栏是 `make debug`，右栏是 `make gdb`。

### 二、阶段一：复位与 MROM（0x1000）

GDB 连上之后，先确认 CPU 的初始状态，再反汇编 PC 处的指令，并查看紧跟在指令后面的数据区：

![GDB 连接后停在 0x1000](./images/T2-1-reset-0x1000.png)

<center>图 2-2　复位后的第一条指令位于 0x1000，CPU 处于 M 态</center>

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

> **版本差异**：在课程推荐的 QEMU 4.1 中（见其源码 `hw/riscv/virt.c`），复位代码只有 5 条指令，不设置 a2；配套的 OpenSBI 是 fw_jump 型，下一阶段地址在编译时就固定为 0x80200000，不需要 QEMU 传参。新版 QEMU 改用 fw_dynamic 型固件，入口地址改由 QEMU 动态传入，这也是原框架 Makefile 在新版 QEMU 上无法启动内核的原因（见「对实验框架的修改」一节）。

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

此后 OpenSBI 依次检查重定位、清零自己的 .bss、设置陷阱入口 `mtvec`、解析设备树、初始化串口和定时器、配置异常委托与 PMP 内存保护，最后打印我们在 `make qemu` 中看到的 banner。

QEMU 自带的 OpenSBI 固件没有符号表（对其 ELF 运行 `nm` 的结果是 `no symbols`），无法按函数名下断点，逐条单步又要走上万条指令。我们换了一种思路：OpenSBI 把控制权交给内核时，必然要通过 `mret` 指令完成从 M 态到 S 态的切换。所以先用 `objdump` 反汇编固件，找出其中全部 5 条 `mret` 指令，在它们上面都设置断点，并让 GDB 在每次命中时自动打印 `mepc`（mret 将要跳往的地址）和 `mstatus.MPP`（mret 将要切换到的特权级），然后继续运行：

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

`mret` 一共被执行了 7 次，只有最后一次是交给内核的，前 6 次都是 OpenSBI 初始化时的“试探”：

- **第 1～5 次**（mcause = 2，非法指令）：OpenSBI 在**探测 CPU 实现了哪些 CSR**。它故意读一个可能不存在的寄存器：如果触发了非法指令异常，就说明这个寄存器不存在，异常处理程序记下结果后用 mret 返回（从 M 态回到 M 态）。把 mtval 中记录的指令编码解码后，被探测的 CSR 依次是 `pmpaddr16`（0x3c0，用来确定 PMP 表项数，结果为 16）、`mhpmcounter19`（0xb13，用来确定性能计数器数量，结果为 16）、`scountovf`（0xda0）、`mtopi`（0xfb0）和 `mstateen0`（0x30c），后三个分别对应几项可选扩展。两个探测结果与 OpenSBI banner 中的 `PMP Count : 16` 和 `MHPM Count : 16` 一致。
- **第 6 次**（mcause = 3，断点）：断点前后的代码是 `slli zero,zero,0x1f; ebreak; srai zero,zero,0x7`，这是 RISC-V **semihosting**（让被调试程序借用宿主机输入输出的机制）的固定指令序列。OpenSBI 先临时替换 `mtvec`，再执行一次 `ebreak` 来探测 semihosting 是否可用。QEMU 没有开启 semihosting，于是产生断点异常，处理程序把 mepc 加 4 后返回。
- **第 7 次**（MPP = 1）：这一次**从 M 态进入 S 态，把控制权交给内核**。

### 四、阶段三：mret 交接与进入内核（0x80200000）

我们单独在第 7 次 mret 的地址 0x8000aec8 处下断点，观察交接的完整过程：

![在交接的 mret 处断下，执行后进入 S 态的 kern_entry](./images/T2-2-mret-to-kernel.png)

<center>图 2-3　OpenSBI 通过 mret 把控制权交给内核（左栏为此时已打印的 OpenSBI banner）</center>

右栏自上而下可以看到交接的全过程：

1. **交接前的准备**：`x/7i` 显示 mret 前面的几条指令，`csrw mepc,s3` 把 mepc 设为下一阶段的入口，`mv a0,s4` / `mv a1,s5` 准备传给内核的两个参数。它前面还有一条 `csrw mstatus`（不在截图范围内），用来把 MPP 设为 S 态。这段逻辑与 OpenSBI 源码中的 `sbi_hart_switch_mode()` 一致。此时 `mepc = 0x80200000`，`mstatus.MPP = 1`，CPU 仍处于 M 态。
2. **执行 mret**：`si` 之后 PC 跳到 `kern_entry`，`priv` 变为 1，即 **S 态**。这就是“跳转到 0x80200000”的确切时刻和方式。
3. **内核看到的初始状态**：`a0 = 0` 是 hart 编号，`a1 = 0x87e00000` 指向设备树，这两个参数要到后续实验才会用到。值得注意的是 **`sp = 0x80046eb0`，仍然是 OpenSBI 自己的栈**。左栏的 banner 显示，`0x80040000-0x8005ffff`（Domain0 Region01）对 S/U 态没有任何权限（`S/U: ()`），访问会被 PMP 拦截。所以内核如果直接用这个 sp 压栈，就会触发访问异常。这从另一个角度说明了练习 1 中 `la sp, bootstacktop` 为什么必须是内核的第一件事。
4. **切换到内核栈**：再单步两条指令（`la` 伪指令展开成 `auipc` + `mv` 两条），`sp` 变为 `0x80203000`，即 `bootstacktop`。GDB 把这个地址显示为 `<SBI_CONSOLE_PUTCHAR>`，是因为栈顶与紧随其后的全局变量 `SBI_CONSOLE_PUTCHAR` 地址相同。栈向低地址增长，第一次压栈写入的是 0x80202ff8，不会覆盖这个变量。

此外，交接前 OpenSBI 设置的**异常委托**也值得一看（见左栏 banner 最后两行）：

- `MEDELEG = 0xf0b509`：指令地址未对齐、断点、U 态 `ecall`、各类页错误等异常直接交给 S 态的内核处理；**S 态的 `ecall`（第 9 位）没有被委托**，所以内核执行 `ecall` 时会陷入 M 态由 OpenSBI 处理，SBI 调用就是这样实现的（第六节会验证）。
- `MIDELEG = 0x1666`：S 态的软件中断、时钟中断和外部中断（第 1、5、9 位）委托给内核，其余几位与虚拟化扩展有关。

### 五、关于指导书提示的勘误

指导书的提示称“SBI 固件进行主初始化，其核心任务之一是将内核加载到 0x80200000，可以使用 `watch *0x80200000` 观察内核加载瞬间”。我们按提示做了实验：

![watch 实验：watchpoint 从未触发](./images/T2-3-watch.png)

<center>图 2-4　在 0x1000 处设置 watchpoint，运行后只命中了内核入口的断点</center>

GDB 停在 0x1000 时，0x80200000 处已经是 `kern_entry` 的指令；设置硬件 watchpoint 后继续运行，直接命中了内核入口的断点，watchpoint 从未被触发。我们也用框架原本的 `-device loader` 方式重复了这个实验，结果相同。

原因在于，**内核不是由 OpenSBI 加载的，而是 QEMU 在虚拟机复位、CPU 开始执行之前就直接写进了模拟内存**（`-kernel` 和 `-device loader` 两种参数都是这样）。OpenSBI 只负责“跳转”，不负责“加载”，所以 watch 永远等不到那次写入。同理，提示中说 0x1000 处执行的是“OpenSBI 的汇编代码”也不准确：0x1000 处是 QEMU 生成的 MROM 复位代码，OpenSBI 从 0x80000000 才开始执行。

那么在**真实硬件**上，内核是由谁加载的？以 SiFive HiFive Unmatched、StarFive VisionFive 2 等常见 RISC-V 开发板为例，典型的启动链是：

```
片上 ROM（ZSBL） → U-Boot SPL → OpenSBI（M 态） → U-Boot（S 态） → Linux 内核（S 态）
```

片上 ROM 从 SPI Flash 或 SD 卡读入 U-Boot SPL；SPL 初始化 DRAM 后，把 OpenSBI 和 U-Boot 一并读入内存；OpenSBI 完成 M 态初始化后跳转到 U-Boot；U-Boot 具备存储、文件系统和网络驱动，由它从磁盘或网络读入内核镜像和设备树，再跳转到内核。可见，在真机上 OpenSBI 同样**只负责初始化和跳转，不负责从存储设备读取内核**：它本身没有磁盘和文件系统驱动，“加载”是由它前后的引导程序（SPL、U-Boot）完成的。OpenSBI 还有一种 fw_payload 形式，把内核或 U-Boot 直接打包进固件镜像，但这时 payload 是随固件一起被前一级引导程序读入内存的，OpenSBI 仍然只做跳转。QEMU 实验中把这些引导程序都省掉了，由 QEMU 自己完成“读入内存”这一步。

### 六、延伸：内核如何使用 OpenSBI 的服务

内核启动后，OpenSBI 并没有退场。`kern_init` 调用 `cprintf` 输出字符串，最终会走到 `sbi_console_putchar` 中的 `ecall`。我们在这条 `ecall` 上下断点：

![内核通过 ecall 陷入 OpenSBI 请求输出字符](./images/T2-4-ecall.png)

<center>图 2-5　内核第一次调用 SBI：S 态 ecall 陷入 M 态，处理完成后返回</center>

- 断下时 CPU 处于 S 态，`a7 = 1` 是 SBI 调用号 `SBI_CONSOLE_PUTCHAR`，`a0 = 0x28` 是要输出的字符 `(`，也就是 `(THU.CST) os is loading ...` 的第一个字符。
- 执行 `ecall` 后，PC 跳到 `mtvec` 指向的 0x80000428（OpenSBI 的陷阱入口），特权级变为 M 态，`mcause = 9` 表示“来自 S 态的 ecall”，`mepc` 记录了 ecall 的地址，以便返回。
- OpenSBI 处理完请求后用 `mret` 返回，内核从 `ecall` 的下一条指令（0x80200470）继续执行，特权级恢复为 S 态。此时左栏 QEMU 的输出中出现了一个 `(`，正是这次调用输出的字符。

这说明 `cprintf` 能在终端上显示文字，是因为内核把“向串口写字符”这件事委托给了 M 态的固件，内核自己并不直接操作串口（OpenSBI banner 中的 `Platform Console Device : uart8250`）。在后续实验中，时钟中断的设置（`sbi_set_timer`）也会走同样的路径。
