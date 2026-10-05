## 练习2：使用 GDB 验证启动流程

**负责人：** openfar

> **题目**：使用 GDB 跟踪 QEMU 模拟的 RISC-V 从加电开始，直到执行内核第一条指令（跳转到 0x80200000）的整个过程。RISC-V 硬件加电后最初执行的几条指令位于什么地址？它们主要完成了哪些功能？

**结论**：加电后 CPU 处于 M 态（机器模式），PC 被复位到 **0x1000**。这里是 QEMU 在 MROM 中生成的复位代码，一共 6 条指令，作用是为下一阶段准备三个参数（`a0` = hart 编号，`a1` = 设备树地址，`a2` = 启动信息结构 `fw_dynamic_info` 的地址），然后跳转到 **0x80000000** 处的 OpenSBI 固件。OpenSBI 在 M 态完成平台初始化，最后通过一条 `mret` 指令降到 S 态（监管者模式），并跳转到 **0x80200000** 执行内核的 `kern_entry`。下面各节的每个结论都来自实际的 GDB 输出。

### 一、背景：启动链上的三个角色

RISC-V 有三个常用的特权级：**M 态**（Machine）权限最高，可以访问一切硬件；**S 态**（Supervisor）运行操作系统内核；**U 态**（User）运行用户程序。CPU 加电时处于 M 态，内核运行在 S 态，所以在两者之间需要一个运行在 M 态的固件来“交接”，这个固件就是 **OpenSBI**。它在启动时完成硬件初始化，然后把 CPU 降到 S 态交给内核；内核运行期间，它常驻在 M 态，为内核提供输出字符、设置定时器等与平台相关的服务。内核通过 `ecall` 指令请求这些服务，这套 S 态与 M 态之间的调用约定称为 **SBI**（Supervisor Binary Interface）。

整个启动过程可以概括为下图：左边是这几个参与者在物理内存中的位置，右边是控制权在它们之间的传递顺序。

![启动流程与物理内存布局](../images/T2-0-boot-overview.png)

<center>图 2-1　lab1 的启动流程与物理内存布局</center>

调试环境为 WSL Ubuntu，QEMU 8.2.2（自带 OpenSBI v1.3），GDB 使用 `riscv64-unknown-elf-gdb`。

### 二、阶段一：复位与 MROM（0x1000）

GDB 连上之后，先确认 CPU 的初始状态，再反汇编 PC 处的指令，并查看紧跟在指令后面的数据区：

![GDB 连接后停在 0x1000](../images/T2-1-reset-0x1000.png)

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

此后 OpenSBI 还要完成一系列初始化，最后打印 banner 并跳转到内核。但 QEMU 自带的 OpenSBI 固件没有符号表（对它运行 `nm`，结果是 `no symbols`），GDB 里只能看到地址，看不出每段代码属于哪个函数。为了弄清 OpenSBI 具体做了什么，我们分两步观察：先从源码编译一份带符号的 OpenSBI，按函数名跟踪完整的初始化流程；再回到 QEMU 自带的固件，用 `mret` 断点验证其中的关键行为。

#### 3.1 用带符号的 OpenSBI 跟踪初始化流程

QEMU 8.2.2 自带的是 OpenSBI v1.3（banner 第一行）。我们从官方仓库取出 v1.3 的源码，按 QEMU 使用的配置（`PLATFORM=generic`，fw_dynamic 型）编译。编译时只额外加了 `-std=gnu11`，没有改动任何源码：我们的 GCC 15 默认采用 C23 标准，`bool` 成了关键字，与 OpenSBI 自己的 `typedef` 冲突。编译得到的 `fw_dynamic.elf` 带完整的调试信息。我们在 Makefile 中增加了可选变量 `OPENSBI`，用它替换 QEMU 默认的固件，并让 GDB 同时加载 OpenSBI 的符号：

```bash
make debug OPENSBI=~/Code/opensbi/build/platform/generic/firmware/fw_dynamic.elf
make gdb   OPENSBI=~/Code/opensbi/build/platform/generic/firmware/fw_dynamic.elf
```

换上这份固件后，内核照常启动，输出与原来相同。现在可以直接按函数名下断点，并用 `bt` 查看调用关系。下图在 `sbi_hart_init`（探测 CPU 特性）和 `sbi_hart_switch_mode`（交接给内核）两处断下：

![用带符号的 OpenSBI 跟踪初始化](../images/T2-5-opensbi-symbols.png)

<center>图 2-3　带符号的 OpenSBI：sbi_hart_init 的调用栈，以及交接函数 sbi_hart_switch_mode 的参数</center>

`sbi_hart_switch_mode` 的参数就是交接信息：`arg0 = 0`（hart 编号）、`arg1 = 2279604224`（即 0x87e00000，设备树地址）、`next_addr = 0x80200000`、`next_mode = 1`（S 态），与 MROM 通过 `fw_dynamic_info` 传入的内容一致。图中这个函数的调用栈经过了 `init_warm_startup`，但源码中是 `init_coldboot` 在最后调用 `sbi_hsm_hart_start_finish`。我们推测这是编译器把两个函数结尾的相同代码合并后，调试信息出现的偏差。

在各个关键函数上设断点并依次运行，得到 OpenSBI 从入口到交接的完整流程：

| 阶段 | 函数（源文件） | 作用 |
|------|----------------|------|
| 汇编 | `_start`（firmware/fw_base.S） | 把 MROM 传来的 a0/a1/a2 保存到 s0/s1/s2 |
| | `fw_boot_hart`（firmware/fw_dynamic.S） | 从 `fw_dynamic_info` 中读出由哪个 hart 负责启动 |
| | 启动抽签、重定位检查、清零 .bss | 多核时选出唯一的冷启动 hart，并准备好 OpenSBI 自身的内存 |
| | `fw_save_info`（firmware/fw_dynamic.S） | 保存 next_addr、next_mode 等交接信息 |
| | `fw_platform_init`（platform/generic/platform.c） | 解析设备树（参数 arg1 = 0x87e00000），得到 hart 数量、内存范围、串口等平台信息 |
| | 设置每个 hart 的 scratch 区和栈，`mtvec = _trap_handler` | 为 C 代码准备运行环境，并设置 M 态的陷阱入口 |
| | `_start_warm → sbi_init`（lib/sbi/sbi_init.c） | 进入 C 代码，冷启动 hart 执行 `init_coldboot` |
| C 语言 | `sbi_scratch_init` | 初始化每个 hart 的私有数据区 |
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

需要说明的是，自己编译的固件与 QEMU 自带的固件**逻辑相同，但内存布局不同**：前者大小为 190 KB，读写区从 0x80020000 开始；后者为 322 KB，读写区从 0x80040000 开始。因此函数地址、寄存器分配等细节不同。本节其余部分和第四节中出现的地址，都以 QEMU 自带的固件为准。

#### 3.2 回到 QEMU 自带的固件：七次 mret

不论使用哪份固件，OpenSBI 把控制权交给内核时，都必须用 `mret` 完成从 M 态到 S 态的切换。对于没有符号的自带固件，我们先用 `objdump` 反汇编，找出其中全部 5 条 `mret` 指令，在这些地址上都设置断点，并让 GDB 在每次命中时自动打印 `mepc`（mret 将要跳往的地址）和 `mstatus.MPP`（mret 将要切换到的特权级），然后继续运行：

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

`mret` 一共执行了 7 次，只有最后一次是交给内核的。结合带符号固件中的断点（在 `__sbi_expected_trap` 和 `semihosting_enabled` 上下断点，查看陷阱发生的位置和调用栈），可以把每一次都对应到源码：

- **第 1～5 次**（mcause = 2，非法指令）：发生在 `sbi_hart_init` 中，即 `lib/sbi/sbi_hart.c` 第 622、632、669、682、697 行（被内联进来的 `hart_detect_features`）。OpenSBI 在**探测 CPU 实现了哪些 CSR**：它调用 `csr_read_allowed()`，先把 `mtvec` 临时换成 `__sbi_expected_trap`，再去读一个可能不存在的 CSR。如果读取触发了非法指令异常，CPU 就会跳到 `__sbi_expected_trap`，它只把异常原因记下来、把 `mepc` 加 4，然后用 `mret` 回到原处，OpenSBI 由此得知这个 CSR 不存在。这类探测走的是这条“预期内”的专用路径，**不会进入正式的陷阱处理函数 `sbi_trap_handler`**（我们在 `sbi_trap_handler` 上设的断点在内核启动前一次也没有命中）。把 mtval 中记录的指令编码解码后，5 次探测的对象依次是：

  | 源码行 | 被探测的 CSR | 目的 |
  |--------|-------------|------|
  | 622 | `pmpaddr16`（0x3c0） | 从 pmpaddr0 起逐个读，读到第一个不存在的就得出 PMP 表项数，结果为 16 |
  | 632 | `mhpmcounter19`（0xb13） | 同样的方法确定性能计数器数量，结果为 16 |
  | 669 | `scountovf`（0xda0） | 检测 Sscofpmf 扩展（计数器溢出中断） |
  | 682 | `mtopi`（0xfb0） | 检测 AIA 扩展（高级中断架构） |
  | 697 | `mstateen0`（0x30c） | 检测 Smstateen 扩展 |

  前两个探测结果与 banner 中的 `PMP Count : 16`、`MHPM Count : 16` 一致。

- **第 6 次**（mcause = 3，断点）：调用链是 `sbi_console_init → generic_console_init → semihosting_enabled()`（`platform/generic/platform.c` 第 279 行）。断点前后的代码是 `slli zero,zero,0x1f; ebreak; srai zero,zero,0x7`，这是 RISC-V **semihosting**（让被调试程序借用宿主机输入输出的机制）的固定指令序列。`semihosting_enabled()` 先临时替换 `mtvec`，再执行一次 `ebreak`：如果调试器或模拟器接管了这个 ebreak，就说明 semihosting 可用。QEMU 没有开启 semihosting，所以产生了断点异常，处理代码把 mepc 加 4 后返回；OpenSBI 因此改用 uart8250 串口作为控制台，也不会调用 `semihosting_init`。
- **第 7 次**（MPP = 1）：在 `sbi_hart_switch_mode` 中，**从 M 态进入 S 态，把控制权交给内核**。

### 四、阶段三：mret 交接与进入内核（0x80200000）

我们单独在第 7 次 mret 的地址 0x8000aec8 处下断点，观察交接的完整过程：

![在交接的 mret 处断下，执行后进入 S 态的 kern_entry](../images/T2-2-mret-to-kernel.png)

<center>图 2-4　OpenSBI 通过 mret 把控制权交给内核（左栏为此时已打印的 OpenSBI banner）</center>

右栏自上而下可以看到交接的全过程：

1. **交接前的准备**：`x/7i` 显示 mret 前面的几条指令，`csrw mepc,s3` 把 mepc 设为下一阶段的入口，`mv a0,s4` / `mv a1,s5` 准备传给内核的两个参数。它前面还有一条 `csrw mstatus`，用来把 MPP 设为 S 态。这段逻辑与 OpenSBI 源码中的 `sbi_hart_switch_mode()` 一致。此时 `mepc = 0x80200000`，`mstatus.MPP = 1`，CPU 仍处于 M 态。
2. **执行 mret**：`si` 之后 PC 跳到 `kern_entry`，`priv` 变为 1，即 **S 态**。这就是**跳转到 0x80200000**的确切时刻和方式。
3. **内核看到的初始状态**：`a0 = 0` 是 hart 编号，`a1 = 0x87e00000` 指向设备树。值得注意的是 **`sp = 0x80046eb0`，仍然是 OpenSBI 自己的栈**。左栏的 banner 显示，`0x80040000-0x8005ffff`（Domain0 Region01）对 S/U 态没有任何权限（`S/U: ()`），访问会被 PMP 拦截。所以内核如果直接用这个 sp 压栈，就会触发访问异常。这从另一个角度说明了练习 1 中 `la sp, bootstacktop` 为什么必须是内核的第一件事。
4. **切换到内核栈**：再单步两条指令（`la` 伪指令展开成 `auipc` + `mv` 两条），`sp` 变为 `0x80203000`，即 `bootstacktop`。
5. 之后继续执行  `entry.S` 和 `init.c`

### 五、关于指导书提示的勘误

指导书的提示称“SBI 固件进行主初始化，其核心任务之一是将内核加载到 0x80200000，可以使用 `watch *0x80200000` 观察内核加载瞬间”。我们按提示做了实验：

![watch 实验：watchpoint 从未触发](../images/T2-3-watch.png)

<center>图 2-5　在 0x1000 处设置 watchpoint，运行后只命中了内核入口的断点</center>

GDB 停在 0x1000 时，0x80200000 处已经是 `kern_entry` 的指令；设置硬件 watchpoint 后继续运行，直接命中了内核入口的断点，watchpoint 从未被触发。

原因在于，**内核不是由 OpenSBI 加载的，而是 QEMU 在虚拟机复位、CPU 开始执行之前就直接写进了模拟内存**（`-kernel` 和 `-device loader` 两种参数都是这样）。OpenSBI 只负责跳转，不负责加载，所以 watch point 不被触发。同理，提示中说 0x1000 处执行的是“OpenSBI 的汇编代码”也不准确：0x1000 处是 QEMU 生成的 MROM 复位代码，OpenSBI 从 0x80000000 才开始执行。

那么在**真实硬件**上，内核是由谁加载的？以 SiFive HiFive Unmatched、StarFive VisionFive 2 等常见 RISC-V 开发板为例，典型的启动链是：

```
片上 ROM（ZSBL） → U-Boot SPL → OpenSBI（M 态） → U-Boot（S 态） → Linux 内核（S 态）
```

片上 ROM 从 SPI Flash 或 SD 卡读入 U-Boot SPL；SPL 初始化 DRAM 后，把 OpenSBI 和 U-Boot 一并读入内存；OpenSBI 完成 M 态初始化后跳转到 U-Boot；U-Boot 具备存储、文件系统和网络驱动，由它从磁盘或网络读入内核镜像和设备树，再跳转到内核。可见，在真机上 OpenSBI 同样**只负责初始化和跳转，不负责从存储设备读取内核**：它本身没有磁盘和文件系统驱动，“加载”是由它前后的引导程序（SPL、U-Boot）完成的。QEMU 实验中把这些引导程序都省掉了，由 QEMU 自己完成“读入内存”这一步。

### 六、延伸：内核如何使用 OpenSBI 的服务

内核启动后，OpenSBI 并没有退场。`kern_init` 调用 `cprintf` 输出字符串，最终会走到 `sbi_console_putchar` 中的 `ecall`。我们在这条 `ecall` 上下断点：

![内核通过 ecall 陷入 OpenSBI 请求输出字符](../images/T2-4-ecall.png)

<center>图 2-6　内核第一次调用 SBI：S 态 ecall 陷入 M 态，处理完成后返回</center>

- 断下时 CPU 处于 S 态，`a7 = 1` 是 SBI 调用号 `SBI_CONSOLE_PUTCHAR`，`a0 = 0x28` 是要输出的字符 `(`，也就是 `(THU.CST) os is loading ...` 的第一个字符。
- 执行 `ecall` 后，PC 跳到 `mtvec` 指向的 0x80000428（OpenSBI 的陷阱入口），特权级变为 M 态，`mcause = 9` 表示“来自 S 态的 ecall”，`mepc` 记录了 ecall 的地址，以便返回。
- OpenSBI 处理完请求后用 `mret` 返回，内核从 `ecall` 的下一条指令（0x80200470）继续执行，特权级恢复为 S 态。此时左栏 QEMU 的输出中出现了一个 `(`，正是这次调用输出的字符。

这说明 `cprintf` 能在终端上显示文字，是因为内核把“向串口写字符”这件事委托给了 M 态的固件，内核自己并不直接操作串口

借助带符号的固件，还可以看到 OpenSBI 内部是怎样处理这次调用的。我们换上带符号的固件，先运行到内核入口，再在串口驱动的输出函数 `uart8250_putc` 上下断点：

![带符号的 OpenSBI 中，ecall 的完整处理链](../images/T2-6-ecall-chain.png)

<center>图 2-7　内核的 ecall 在 OpenSBI 内部的处理链，最终由 uart8250_putc 写出字符 '('</center>

调用栈自下而上依次是：

```
_trap_handler          (fw_base.S)        保存内核的寄存器现场
 → sbi_trap_handler    (sbi_trap.c)       按 mcause = 9 判定为来自 S 态的 ecall
 → sbi_ecall_handler   (sbi_ecall.c)      按 a7 = 1 找到对应的 SBI 扩展
 → sbi_ecall_legacy_handler (sbi_ecall_legacy.c)  CONSOLE_PUTCHAR 属于 SBI v0.1 的旧式调用
 → sbi_putc            (sbi_console.c)    控制台抽象层
 → uart8250_putc('(')  (uart8250.c)       等待发送寄存器空闲，把字符写入 UART
```

可见从内核的一条 `ecall` 到字符真正出现在终端上，中间经过了 OpenSBI 的陷阱入口、SBI 调用分发和串口驱动三层。内核只需要知道“调用号 1 表示输出一个字符”，串口是什么型号、寄存器在哪个地址，这些平台细节都由 OpenSBI 屏蔽了。这正是 SBI 作为“内核与固件之间的接口”的意义。
