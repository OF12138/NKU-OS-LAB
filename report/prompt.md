# Lab 1 提示词汇总

各任务按指导书的四段式模板整理的最终版提示词。


---

<!-- prompt:T0-env-compat.prompt.md -->
## 环境兼容：修改 QEMU 启动方式

[PROMPT]

**任务**：修改 `code/Makefile` 中的 `qemu` 和 `debug` 两个目标，使 lab1 内核在较新版本的 QEMU（自带 fw_dynamic 型 OpenSBI，例如 QEMU 8.2 / OpenSBI 1.3）上能够被 OpenSBI 跳转执行，同时保持对课程推荐的 QEMU 4.1（fw_jump 型 OpenSBI）的兼容。

**操作要求**：你必须在项目中直接修改实际文件，而不是只展示代码片段。只修改这两个目标的 QEMU 启动参数，不要改动编译、链接规则和其他目标。在修改处用中文注释说明原因。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中提供的信息，以工程中的真实代码为准。

[RELY]

```makefile
# code/Makefile（修改前）
kernel = $(call totarget,kernel)
UCOREIMG	:= $(call totarget,ucore.img)

qemu: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-device loader,file=$(UCOREIMG),addr=0x80200000

debug: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-device loader,file=$(UCOREIMG),addr=0x80200000\
		-s -S
```

```ld
/* code/tools/kernel.ld */
ENTRY(kern_entry)
BASE_ADDRESS = 0x80200000;
```

- 现象：在 QEMU 8.2.2 上运行 `make qemu`，OpenSBI 输出 `Domain0 Next Address : 0x0000000000000000`，之后没有任何内核输出。
- `bin/kernel` 是链接得到的 ELF 文件，其 LOAD 段的物理地址从 0x80200000 开始；`bin/ucore.img` 是 objcopy 生成的纯二进制文件，不带地址和入口信息。

[GUARANTEE]

必须修改的目标：

```makefile
qemu:   # 启动 QEMU 运行内核
debug:  # 以 -s -S 启动 QEMU，等待 GDB 连接
```

不得新增或删除其他目标，不得修改 `make gdb` 的行为。

[SPECIFICATION]

## qemu

**Pre-Condition**:
- `bin/kernel` 和 `bin/ucore.img` 已经由默认目标构建完成。

**Post-Condition**:
- QEMU 以 virt 机型、默认 OpenSBI 固件启动，内核位于物理地址 0x80200000，OpenSBI 完成初始化后跳转到 `kern_entry`，终端输出 `(THU.CST) os is loading ...`。

  **Case 1**:
  - 使用 fw_dynamic 型 OpenSBI（新版 QEMU）时，QEMU 必须知道内核的入口地址，并通过 fw_dynamic_info 传给 OpenSBI，OpenSBI 报告的 Next Address 应为 0x80200000。

  **Case 2**:
  - 使用 fw_jump 型 OpenSBI（QEMU 4.1）时，OpenSBI 固定跳转到 0x80200000，内核必须恰好被加载在这个地址上。

## debug

**Pre-Condition**:
- 同 qemu。

**Post-Condition**:
- 与 qemu 使用相同的内核加载方式，另外加上 `-s -S`：CPU 在复位地址 0x1000 暂停，并在 1234 端口等待 GDB 连接。通过 `make gdb` 连接后执行 `b *kern_entry` 和 `c`，应停在 0x80200000。

**Requirements**:
- 同一份 Makefile 必须在两类 QEMU 上都能工作，不能依赖某个特定的 QEMU 版本号。


---

<!-- prompt:T1-ex1-entry.prompt.md -->
# T1 最终提示词：入口汇编与启动栈分析

[PROMPT]
任务：分析 code/kern/init/entry.S 的两个入口伪指令及其与 kern_init 的关系，完成练习1。
操作要求：直接撰写真实文件 report/sections/T1-ex1-entry.md，保留现有实验代码，不修改其他成员文件。
输出要求：围绕源码和单步观察回答问题，只使用 lyp 自己实测产生的图片并解释寄存器变化，不引用 T2 图片。区分伪指令、链接松弛后的机器指令与编译期栈空间预留，协作记录不写进正文。

[RELY]
以下内容分别原样摘自 entry.S、mmu.h、memlayout.h、init.c（entry.S 与 init.c 省略了注释行和行尾注释）：
```asm
kern_entry:
    la sp, bootstacktop

    tail kern_init
```
```c
#define PGSIZE          4096                    // bytes mapped by a page
#define PGSHIFT         12                      // log2(PGSIZE)
#define KSTACKPAGE          2                           // # of pages in kernel stack
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // sizeof kernel stack
int kern_init(void) __attribute__((noreturn));
```
```c
    extern char edata[], end[];
    memset(edata, 0, end - edata);
```

[GUARANTEE]
交付练习1章节：回答 la 加载什么、为什么先建立栈，tail 怎样转移控制权、是否保存返回地址；给出真实反汇编、8192 字节栈范围与对齐、.data 与 BSS 清零关系、tail/call/noreturn 的区别、验证命令及真实迭代说明。

[SPECIFICATION]
## kern_entry 与 kern_init 的分析
Pre-Condition：已阅读当前 lab1 源码、任务要求和最新指导书；从已提交代码副本构建 ELF，记录本次工具链版本。
Post-Condition：报告中每个具体地址与本次 nm/objdump/GDB 输出一致；说明 la 加载符号地址而非内存内容；跳转不会自动清理栈，noreturn 是编译器约定，实际不返回由函数循环保证。
Case 1：链接松弛改变 tail 的展开形式时，同时说明通用的 auipc/jalr 语义与最终 ELF 的指令，不写成固定展开。
Case 2：edata 与 end 相等时，说明本次清零长度为零，不声称已清除非空 BSS。
Requirements：不得编造截图或人工答辩经历；运行时需要先建立有效栈再进入需要栈的 C 函数，但不要断言所有内核入口的第一条机器指令都必须设置 sp。


---

<!-- prompt:T2-ex2-gdb.prompt.md -->
## 练习2：使用 GDB 验证启动流程

[PROMPT]

**任务**：在 lab1 工程（`code/`）中，使用 QEMU + GDB 跟踪 RISC-V 虚拟机从加电复位到执行内核第一条指令（0x80200000，`kern_entry`）的完整过程，并撰写实验报告中的「练习2」一节，写入 `report/sections/T2-ex2-gdb.md`。

**操作要求**：你必须在本机实际运行 QEMU 和 GDB 获取调试输出，报告中的每个结论都要有对应的 GDB 输出作为依据，不能凭记忆或照搬指导书。不要修改 `code/` 下的任何文件。指导书中与实测不符的说法，要在报告中指出并给出证据。

**输出要求**：直接写入报告文件，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中的信息，以实际运行结果为准。

[RELY]

```makefile
# code/Makefile
debug: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios $(OPENSBI) \
		-kernel $(kernel) \
		-s -S

gdb:
	riscv64-unknown-elf-gdb \
    -ex 'file bin/kernel' \
    $(GDB_OPENSBI) \
    -ex 'set arch riscv:rv64' \
    -ex 'target remote localhost:1234'
```

```asm
# code/kern/init/entry.S（省略注释）
kern_entry:
    la sp, bootstacktop
    tail kern_init
```

```c
// code/libs/sbi.c
uint64_t SBI_CONSOLE_PUTCHAR = 1;
void sbi_console_putchar(unsigned char ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
}
```

- 运行环境：QEMU 8.2.2，自带 OpenSBI v1.3（fw_dynamic）。固件 ELF 位于 `/usr/share/qemu/opensbi-riscv64-generic-fw_dynamic.elf`，**没有符号表**。
- OpenSBI v1.3 的源码可从 https://github.com/riscv-software-src/opensbi 获取，用 `make PLATFORM=generic CROSS_COMPILE=riscv64-unknown-elf-` 编译出带调试信息的 `build/platform/generic/firmware/fw_dynamic.elf`。QEMU 可以用 `-bios <路径>` 换上这份固件，GDB 用 `add-symbol-file <路径>` 加载它的符号。
- 指导书给出的提示：复位地址是 0x1000；“SBI 固件将内核加载到 0x80200000，可以用 `watch *0x80200000` 观察加载瞬间”；用 `b *0x80200000` 在内核入口断下。
- GDB 可以通过 `$priv` 查看当前特权级，通过 `$mepc`、`$mstatus`、`$mcause`、`$mtval`、`$medeleg`、`$mideleg` 等读取 CSR。

[GUARANTEE]

必须回答的问题和需要交付的产出：

1. 加电后最初执行的几条指令位于什么地址，每条指令完成了什么功能（附 GDB 输出）。
2. 从 0x1000 到 0x80200000 经过了哪几个阶段，每个阶段运行在什么特权级，由谁提供。
3. OpenSBI 把控制权交给内核的具体时刻和方式（哪条指令、跳转地址、特权级如何变化）。
4. 核实指导书中关于“OpenSBI 加载内核、用 watch 观察”的提示是否成立。
5. 一张启动流程与物理内存布局的示意图，以及调试截图（复位处、交接给内核处、watch 实验、内核调用 SBI），每张图插在对应的文字中间。
6. 说明真实硬件上内核由谁加载，与 QEMU 的做法有何不同。
7. 编译带符号的 OpenSBI，按函数名梳理它从入口到交接内核的完整初始化流程，以及处理内核 ecall 的调用链；为 Makefile 增加可选变量，使 `make debug` / `make gdb` 能切换到这份固件。

[SPECIFICATION]

## 阶段一：复位与 MROM

**Pre-Condition**:
- `make debug` 已启动，CPU 停在第一条指令之前；GDB 通过 `make gdb` 连接成功。

**Post-Condition**:
- 报告列出 0x1000 处的全部指令，并给出单步执行后的寄存器变化；说明 a0/a1/a2 各自的含义，以及紧随其后的数据区（固件入口、设备树地址、`fw_dynamic_info` 各字段）。

## 阶段二：OpenSBI 初始化与交接

**Pre-Condition**:
- 已到达 0x80000000。QEMU 自带的 OpenSBI 没有符号，无法按函数名下断点。

**Post-Condition**:
- 报告说明 OpenSBI 入口处几条指令的作用，按函数列出 OpenSBI 从入口到交接内核的初始化流程，并找到“降到 S 态并跳往内核”的那条指令。

  **Case 1**:
  - 使用带符号的 OpenSBI 时：在各关键函数上设断点，用 `bt` 确认调用关系，给出每个函数的作用；还要说明这份固件与 QEMU 自带固件在内存布局上的差异，避免把两者的地址混用。

  **Case 2**:
  - 使用 QEMU 自带的无符号固件时：如果 OpenSBI 中有多条可能执行的 `mret`，就要区分哪一次是交接给内核的（mepc = 0x80200000，MPP = S 态）。其余各次 mret 的成因要结合 mcause、mtval，以及带符号固件中对应的源码行给出解释。

  **Case 3**:
  - 交接时 OpenSBI 设置好的中断和异常委托（medeleg、mideleg），要说明其含义，特别是 S 态 `ecall` 为什么没有被委托。

## 阶段三：进入内核

**Post-Condition**:
- 在 `kern_entry` 处报告特权级、`satp`、`a0`、`a1`、`sp` 的取值，并解释它们与练习 1 中 `la sp, bootstacktop` 的关系。

## 指导书提示的核实

**Post-Condition**:
- 用 GDB 证明 0x80200000 处的内核代码在 CPU 执行第一条指令之前是否已经存在，以及 watchpoint 是否会被触发；如果与指导书不符，就指出谁、在什么时候把内核放进了内存。

## 延伸：内核如何调用 SBI

**Post-Condition**:
- 在 `sbi_console_putchar` 中的 `ecall` 处断下，说明 a7、a0 的含义，以及 ecall 前后特权级、mcause、mepc、mtvec 的变化。再用带符号固件给出这次调用在 OpenSBI 内部从陷阱入口到串口驱动的完整调用链。

**Requirements**:
- 所有地址、寄存器值和指令都必须来自本次实际运行的输出。
- 面向不熟悉 OpenSBI 的读者，先讲清楚 M/S/U 特权级和 SBI 的作用，再展开细节。
- 示意图采用传统工科风格：黑白、直角矩形。截图使用真实终端画面，左栏 make debug、右栏 make gdb。


---

<!-- prompt:T3-modules.prompt.md -->
# T3 最终提示词

## 1. 链接脚本与内存布局

[PROMPT]
任务：分析 code/tools/kernel.ld 与入口目标文件顺序，撰写 report/sections/T3-modules.md 的链接与内存布局模块。
操作要求：直接编辑真实报告文件，保留实验代码及其他成员章节。
输出要求：结合 nm、readelf 与 make print-kobjs，解释入口、段顺序、边界符号和装载地址；明确链接器与装载器的不同职责。

[RELY]
原样摘自 code/tools/kernel.ld（省略行尾注释）：
```ld
OUTPUT_ARCH(riscv)
ENTRY(kern_entry)

BASE_ADDRESS = 0x80200000;
```
```ld
    .text : {
        *(.text.kern_entry .text .stub .text.* .gnu.linkonce.t.*)
    }
```
```ld
    PROVIDE(edata = .);
```
```ld
    PROVIDE(end = .);
```

[GUARANTEE]
交付段布局表、etext/edata/end 的含义、ENTRY 与 BASE_ADDRESS 的区别、kern_entry 位于最前面的实际原因，以及验证命令和本次结果。

[SPECIFICATION]
## 链接布局分析
Pre-Condition：读取完整 kernel.ld、entry.S、Makefile 和 function.mk，已生成实际 ELF。
Post-Condition：说明当前 entry.S 使用普通 .text，entry.o 为链接输入的第一个目标文件；ENTRY 设置 ELF 入口，不自动重新排列代码。表中地址和边界与实际构建一致。
Case 1：PROVIDE 符号没有被引用、nm 未输出 etext 时，解释它仍表示脚本中 .text 末尾的位置，不补造符号表记录。
Requirements：不把段标志当成已经开启的页表权限；不把页对齐当成分页管理已实现；不修改链接脚本。

## 2. 格式化输出与 SBI 调用

[PROMPT]
任务：分析内核从 cprintf 到 ecall 的输出功能，撰写 T3 报告的输出模块。
操作要求：直接写入真实报告文件，不修改代码；按源码顺序说明每层职责。
输出要求：列出源码调用链，解释各层职责；只使用 lyp 自己实测产生的图片说明输出过程，不引用 T2 图片；区分源码关系与实测结果。

[RELY]
以下声明原样摘自 code/libs/stdio.h、code/kern/driver/console.h、code/libs/sbi.h：
```c
int cprintf(const char *fmt, ...);
int vcprintf(const char *fmt, va_list ap);
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);
void cons_putc(int c);
void sbi_console_putchar(unsigned char ch);
```
以下函数原样摘自 code/libs/sbi.c：
```c
void sbi_console_putchar(unsigned char ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
}
```

[GUARANTEE]
交付 cprintf → vcprintf → vprintfmt → cputch → cons_putc → sbi_console_putchar → sbi_call → ecall 的源码调用关系、参数寄存器与陷入返回解释，以及不能直接使用宿主 printf 的原因。

[SPECIFICATION]
## 输出功能分析
Pre-Condition：读取 stdio.c、printfmt.c、console.c、sbi.c，结合 T2 的固件调试记录和本次反汇编。
Post-Condition：解释变参、回调与计数，在 S 态完成格式化，在当前固件配置下通过 S 态 ecall 进入 M 态；a7=1，a0 为字符；OpenSBI 最终输出，返回后继续内核执行。
Case 1：编译优化内联 vcprintf 或 sbi_call 时，保留源码逻辑链并标明 ELF 中不一定有独立函数符号。
Case 2：讨论 ecall 时，区分 S→M 的 SBI 调用与 U→S 的用户系统调用；不写成任意 ecall 必然进入 M 态。
Requirements：只说明本框架实际支持和使用的旧式 SBI console_putchar；不声称输出经过内核缓冲队列或输出计数就是硬件成功确认。

## 3. 构建与加载流程

[PROMPT]
任务：分析 Makefile/function.mk 从源文件到 ELF 和裸镜像的流程，补全 T3 构建模块及验证记录。
操作要求：直接修改真实报告文件，保留现有代码；在独立干净副本中编译运行。
输出要求：以当前 make 规则和 readelf 结果说明产物区别，结合 T0 解释当前 QEMU 加载方式；正文集中讲代码、观察和结果，环境调用故障留在任务文件。

[RELY]
原样摘自 code/Makefile：
```make
KOBJS	= $(call read_packet,kernel libs)
```
```make
$(UCOREIMG): $(kernel)
	$(OBJCOPY) $(kernel) --strip-all -O binary $@
```
```make
LDFLAGS	+= -nostdlib --gc-sections
```

[GUARANTEE]
交付 .c/.S→.o→bin/kernel→bin/ucore.img 的流程、ELF 与纯二进制区别、make qemu/debug/gdb 的含义及实测运行结论。

[SPECIFICATION]
## 构建和装载分析
Pre-Condition：已读取完整构建规则，并在已提交源码副本执行 make、readelf、make qemu。
Post-Condition：说明 .S 先预处理、目标文件重定位后形成 ELF；objcopy 生成不含 ELF 入口/符号表的裸镜像；当前 make qemu 仍依赖生成镜像，但实际 -kernel 加载 bin/kernel。
Case 1：看到旧式 loader 相关注释时，说明这是历史方案，不当作当前执行命令。
Case 2：make qemu 因 kern_init 的无限循环被 timeout 停止时，记录输出与预期退出码，不把超时误判为启动失败或测试全面通过。
Requirements：说明 lab1 缺少 tools/grade.sh，make grade 不适用；不得声称在本机验证了未运行的 QEMU 版本。


---

<!-- prompt:T4-knowledge.prompt.md -->
# T4 最终提示词：实验与 OS 原理的知识点对照

[PROMPT]
任务：根据当前 lab1 与 T1/T2/T3 分析，完成 report/sections/T4-knowledge.md，对应报告模板「六、实验总结与收获／对操作系统的理解」。
操作要求：直接编写真实 section 文件，保留其他成员内容和实验代码，不编辑最终 report.md。
输出要求：用简洁对照表解释本实验知识点与原理的关系，未覆盖内容按主题归纳；围绕实际代码说明差异，避免重复写成审计清单或加入空泛感想。

[RELY]
原样摘自 code/kern/init/init.c（省略行尾注释）：
```c
int kern_init(void)
{
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    const char *message = "(THU.CST) os is loading ...\n";
    cprintf("%s\n\n", message);
   while (1)
        ;
}
```
原样摘自 code/kern/driver/console.c：
```c
void cons_init(void) {}
```

[GUARANTEE]
交付知识点对照表，覆盖引导、特权级与陷入、栈和 ABI、段布局和 BSS、编译链接装载、SBI 与设备抽象；列出分页/物理页管理、进程调度、用户系统调用、内核中断处理、同步、文件系统及完整设备管理等未覆盖内容。

[SPECIFICATION]
## 知识点映射
Pre-Condition：已阅读当前源码、任务要求、T1/T2/T3 及参考资料，并核对指导书与实测的差异。
Post-Condition：每项明确给出实验事实、OS 概念与二者关系；启动栈不等于进程栈切换，SBI 不等于用户系统调用，段划分不等于页表保护，忙循环不等于调度或休眠。
Case 1：说明异常机制时，承认 ecall 已触发固件陷阱处理，只说未实现内核自己的中断/异常处理，不声称实验完全没有异常。
Case 2：讨论页与对齐时，区分大小常量和实际页表/分配器；不从头文件名推断功能已经实现。
Requirements：仅依据真实代码判断范围，不抄写参考报告，不编造原理课讲授进度或人工学习体验。


---

<!-- prompt:T5-mainline.prompt.md -->
## 实验整体逻辑分析

[PROMPT]

**任务**：撰写 lab1 实验报告的「三、实验整体逻辑分析」，写入 `report/sections/T5-mainline.md`，包括「本章节的逻辑主线」和「功能的逐步实现」两部分。

**操作要求**：直接写入真实的报告文件，不修改 `code/` 下的任何文件。论述要以框架代码和已完成的练习 1、练习 2 及功能模块分析为依据；与这些 section 中的结论保持一致，不重复它们的细节，而是指明对应的位置。

**输出要求**：使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中的信息，按 `report-template.md` 第三部分的格式组织。

[RELY]

```asm
# code/kern/init/entry.S（省略注释）
kern_entry:
    la sp, bootstacktop

    tail kern_init
```

```c
// code/kern/init/init.c（省略行尾注释）
int kern_init(void)
{
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    const char *message = "(THU.CST) os is loading ...\n";
    cprintf("%s\n\n", message);
   while (1)
        ;
}
```

```ld
/* code/tools/kernel.ld（省略行尾注释） */
ENTRY(kern_entry)

BASE_ADDRESS = 0x80200000;
```

- 已完成的分析：练习 1（入口建栈与尾跳转）、练习 2（复位代码 → OpenSBI → mret 交接，以及 SBI 调用链）、功能模块（链接脚本与内存布局、格式化输出与 SBI 服务、构建与镜像加载）、对实验框架的修改（新版 QEMU 需要 `-kernel` 加载 ELF）。
- 实测结论：进入内核时 sp 仍指向被 PMP 保护的 OpenSBI 栈；本次构建 `edata = end`，BSS 为空。

[GUARANTEE]

必须交付的内容：

1. 本章的核心主题，以及它要解决的问题。
2. 一条贯穿全章的主线，说明各部分之间的关系。
3. 按顺序列出本章涉及的各项功能，说明每一步为什么排在这个位置，并给出对应的代码文件和报告位置。

[SPECIFICATION]

## 逻辑主线

**Pre-Condition**:
- 练习 1、练习 2 和功能模块的 section 已经完成并通过审核。

**Post-Condition**:
- 用“控制权的接力”串起复位代码、OpenSBI、`entry.S`、`kern_init` 和 SBI 输出，并指出每一棒为下一棒准备了什么条件。
- 指出构建时的地址约定（`0x80200000`、`kern_entry`）与运行时的跳转目标必须一致，并以 T0 中遇到的问题为例说明。

## 功能的逐步实现

**Post-Condition**:
- 按“构建与链接 → 装载与固件交接 → 入口建栈 → C 运行环境初始化 → 格式化输出”的顺序展开，每一步都说明它依赖前面哪些步骤。

  **Case 1**:
  - lab1 不需要编写新代码，要说明这里的“实现顺序”指的是框架中各层功能被构建和执行的顺序，而不是我们的编码顺序。

  **Case 2**:
  - 涉及 BSS 清零时，要如实说明本次构建中 BSS 为空，同时解释这一步对一般内核的必要性。

**Requirements**:
- 结论必须与其他 section 一致，不引入它们没有验证过的说法。
- 结尾说明当前内核停在死循环中的原因，以及后续实验将补充的内容。

---

---

---

<!-- prompt:T6-misc.prompt.md -->
# T6 任务交付提示词 (Prompt Handoff)

## 提示词 1：现代笔记本启动流程横向对比分析推演

[PROMPT]
任务：分析现代笔记本启动流程（UEFI 规范）并与 RISC-V 真实硬件启动链进行横向对比，输出报告章节至 report/sections/T6-extension.md。
操作要求：严格依据 T2 练习对 RISC-V 启动链的实测结论，提炼计算机固件引导的通用设计哲学；采用严谨的技术文档风格，禁止使用口语化表达。

[RELY]
- 团队 T2 启动链结论：
  加电复位 0x1000 (MROM) -> 0x80000000 (OpenSBI) -> mret 降权跳转 0x80200000 (S-Mode kern_entry)。
- 固件交接数据结构：
  struct fw_dynamic_info {
      unsigned long magic;
      unsigned long version;
      unsigned long next_addr;
      unsigned long next_mode;
      unsigned long options;
      unsigned long boot_hart;
  };
- 真机启动拓扑：
  片上 ROM（ZSBL） -> U-Boot SPL -> OpenSBI（M 态） -> U-Boot（S 态） -> Linux 内核（S 态）
- 工业规范：UEFI Specification v2.10, ARM Trusted Firmware Design (TF-A)。

[GUARANTEE]
必须完整产出以下内容并写入文件：
1. 现代 x86 UEFI 规范的五大阶段时序解析（SEC -> PEI -> DXE -> BDS -> OS Loader -> ExitBootServices）。
2. RISC-V 真机、x86 UEFI 与 ARM64 TF-A 的五层横向对比映射表格。
3. 计算机底层引导的两大核心设计哲学（最小依赖原则、特权级降级收敛）。

[SPECIFICATION]
- Pre-Condition: T2-ex2-gdb.md 中关于 MROM、OpenSBI 以及真机启动拓扑的分析已完成。
- Post-Condition: 生成 report/sections/T6-extension.md，符合 Markdown 标准规范，无多余外层代码块嵌套。
- Requirements: 对比维度必须覆盖特权级别、无 DRAM 阶段处理机制、运行时服务请求通道（SBI / SMI / SMC）。

---

## 提示词 2：实验报告非代码核心章节系统化构建

[PROMPT]
任务：依据 NKU-OS-LAB 规约，生成实验目的、实验环境表、QEMU 运行记录及实验总结初稿，分别写入对应的 report/sections/T6-*.md 文件中。
操作要求：实验环境表严格填报张远（2411264）、李云鹏（2414099）、刘昀皓（2413074）三位成员的软硬件与 AI 协同矩阵；运行记录引用真实截图 ../images/T6-qemu-run.png 并注明 QEMU 7.0.0 版本；实验总结采用团队视角并预留成员补充槽位。

[RELY]
- 团队规范：AGENTS.md, report-template.md, .handoff/tasks/T6-misc-sections.md
- 内核入口与字符输出的声明（原样摘自 code/kern/init/init.c、code/libs/stdio.h、code/libs/sbi.h，省略行尾注释）：
```c
int kern_init(void) __attribute__((noreturn));
int cprintf(const char *fmt, ...);
void sbi_console_putchar(unsigned char ch);
```
- 实测参数：Ubuntu 22.04 LTS (WSL2), QEMU emulator version 7.0.0, OpenSBI v1.0。
- 其他成员的环境：张远为 QEMU 8.2.2 / OpenSBI v1.3 / GCC 15.1.0，李云鹏为 QEMU 6.2.0 / OpenSBI v0.9 / GCC 10.2.0。

[GUARANTEE]
必须交付以下规范文件：
1. report/sections/T6-purpose.md：包含 4 条实验目的，分别对应启动流程与特权级切换、内核构建与内存布局、启动栈 / BSS 清零与 SBI 输出、GDB 调试；只写 lab1 实际涉及的内容（本实验没有开启分页，也没有内核的中断处理）。
2. report/sections/T6-env.md：包含三位成员各自的软件环境表（QEMU 及自带 OpenSBI 的版本、工具链）和符合 report-template.md 规范的 AI 工具表，每一项都要与成员实际使用的情况一致。
3. report/sections/T6-qemu-run.md：包含环境声明、流程分析及内嵌 ../images/T6-qemu-run.png 的截图引用。
4. report/sections/T6-summary.md：以团队视角出发的总结初稿，包含启动时序认知、异步工程协同及人机协同反思槽位。

[SPECIFICATION]
- Pre-Condition: 本地通过 make qemu 跑通内核并已将截图归档至 report/images/T6-qemu-run.png。
- Post-Condition: 所有生成的 section 文件均可在本地正确打开，图片引用路径统一为 ../images/T6-qemu-run.png。
- Requirements: 保持纯正技术文档语言，杜绝主观口语化修饰。
