# Lab 3 提示词汇总

各任务按指导书的四段式模板整理的最终版提示词。


---

<!-- prompt:T1-trapentry.prompt.md -->
## 功能模块一：Trap 上下文恢复与 Trap 入口（RESTORE_ALL、__alltraps）

[PROMPT]

**任务**：在 `code/kern/trap/trapentry.S` 中补全宏 `RESTORE_ALL` 和入口 `__alltraps` 的缺失部分，使内核形成“Trap 进入 → SAVE_ALL 保存上下文 → 调用 C 函数 `trap()` → RESTORE_ALL 恢复上下文 → `sret` 返回原执行流”的完整闭环。`RESTORE_ALL` 根据当前 Trap frame 恢复被打断程序的处理器状态；`__alltraps` 把 SAVE_ALL 建好的 Trap frame 按 RISC-V 调用约定传给 `trap(struct trapframe *tf)`。

**操作要求**：你必须在项目中直接修改实际文件，而不是只展示代码片段。只在文件中两处 `#lab3 function1 YOUR CODE` 注释的下方写代码，并把这两处注释中的 `YOUR CODE` 替换为学号 `2411264`，保留 `lab3 function1`。不要修改 SAVE_ALL、`__trapret` 标号和其后的 `sret`、`struct trapframe`、`trap()` 的签名以及其他任何文件。关键步骤写简短的中文注释。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中提供的信息，以工程中的真实代码为准。

[RELY]

```asm
# code/kern/trap/trapentry.S（框架原样，省略部分重复行）
#include <riscv.h>

    .macro SAVE_ALL

    csrw sscratch, sp

    addi sp, sp, -36 * REGBYTES
    # save x registers
    STORE x0, 0*REGBYTES(sp)
    STORE x1, 1*REGBYTES(sp)
    STORE x3, 3*REGBYTES(sp)
    STORE x4, 4*REGBYTES(sp)
    ...                          # x5 ~ x30 同样依次存到 n*REGBYTES(sp)
    STORE x31, 31*REGBYTES(sp)

    # get sr, epc, badvaddr, cause
    # Set sscratch register to 0, so that if a recursive exception
    # occurs, the exception vector knows it came from the kernel
    csrrw s0, sscratch, x0
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, sbadaddr
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp)
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm

    .macro RESTORE_ALL

    #lab3 function1 YOUR CODE

    .endm

    .globl __alltraps
    .align(2)
__alltraps:
    SAVE_ALL

    #lab3 function1 YOUR CODE
    # sp should be the same as before "jal trap"

    .globl __trapret
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret
```

```c
// code/libs/riscv.h（RV64 部分）
#if __riscv_xlen == 64
# define STORE    sd
# define LOAD     ld
# define LOG_REGBYTES 3
#endif
#define REGBYTES (1 << LOG_REGBYTES)
```

```c
// code/kern/trap/trap.h
struct pushregs {
    uintptr_t zero;  // Hard-wired zero
    uintptr_t ra;    // Return address
    uintptr_t sp;    // Stack pointer
    uintptr_t gp;    // Global pointer
    uintptr_t tp;    // Thread pointer
    uintptr_t t0;    // Temporary
    ...              // t1, t2, s0, s1, a0 ~ a7, s2 ~ s11, t3 ~ t6，即 x6 ~ x31 依次排列
    uintptr_t t6;    // Temporary
};

struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};

void trap(struct trapframe *tf);
```

```c
// code/kern/trap/trap.c
void idt_init(void) {
    extern void __alltraps(void);
    /* Set sup0 scratch register to 0, indicating to exception vector
       that we are presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
}
```

- RISC-V 调用约定：第一个参数通过 `a0` 传递；`sp` 是被调用者保存寄存器，`trap()` 返回时 `sp` 与调用前相同；`jal` 会改写 `ra`。
- 处理器进入 S 态 Trap 时，硬件把被打断指令的地址写入 `sepc`，把 `sstatus.SIE` 保存到 `SPIE` 后清零，并把原特权级记入 `SPP`。`sret` 跳转到 `sepc`，并按 `SPP`、`SPIE` 恢复特权级和中断使能，不读取 `stval`（即 `sbadaddr`）和 `scause`。
- `trap()` 可能修改 Trap frame 中的字段，例如异常处理会推进 `tf->epc`。

[GUARANTEE]

```asm
    .macro RESTORE_ALL     # 输入：sp 指向 Trap frame；输出：恢复后的处理器执行状态
    ...
    .endm

__alltraps:                # SAVE_ALL 之后：把 Trap frame 作为 struct trapframe * 传给 trap()
    SAVE_ALL
    ...
__trapret:                 # trap() 返回后顺序执行到这里
    RESTORE_ALL
    sret
```

[SPECIFICATION]

## RESTORE_ALL

**Pre-Condition**:
- `sp` 指向 SAVE_ALL 建立的 Trap frame（共 36 × `REGBYTES` 字节），帧中第 n 个通用寄存器槽位存 xn，32～35 号槽位依次是 status、epc、badvaddr、cause。
- `trap()` 已返回，帧内容可能已被修改（例如 `epc`）。

**Post-Condition**:
- `sstatus`、`sepc` 等于帧中的 status、epc，随后的 `sret` 依据它们返回。
- x1、x3～x31 等于帧中对应槽位的值；x0 不需要恢复。
- `sp` 等于 Trap 发生前的值（帧中 2 号槽位）。

  **Case 1**（x2/sp 的特殊处理）:
  - SAVE_ALL 没有直接保存 x2：它先把原 sp 暂存到 `sscratch`，`sp` 下移开辟 Trap frame 后，再通过 `csrrw s0, sscratch, x0` 取出原 sp 存进 2 号槽位，同时把 `sscratch` 清零。所以 2 号槽位是 Trap 前的 sp，而当前 `sp` 是帧的基址。
  - 所有数据都要通过 `sp` 从帧中取，sp 必须最后恢复；提前恢复 sp 会让后续的加载全部读错地址。

  **Case 2**（CSR 的恢复顺序）:
  - 写 CSR 需要借用通用寄存器作中转。先用中转寄存器取出 status、epc 写回 `sstatus`、`sepc`，再恢复这些中转寄存器本身，不能让它们最后保留 CSR 的值。

  **Case 3**（不需要写回的字段）:
  - badvaddr、cause 只是提供给 `trap()` 的现场记录，`sret` 不使用它们，不需要写回 CSR。

**Requirements**:
- 恢复的布局与 SAVE_ALL 保存时完全一致，使用 `LOAD` 和 `n*REGBYTES(sp)` 的写法，不自行假设其他布局。

## __alltraps

**Pre-Condition**:
- `stvec` 指向 `__alltraps`（由 `idt_init()` 设置），Trap 发生后处理器跳到这里，SAVE_ALL 执行完时 `sp` 指向 Trap frame。

**Post-Condition**:
- 调用 `trap(tf)` 时 `a0` 等于 Trap frame 的地址。
- `trap()` 返回后顺序进入 `__trapret`，此时 `sp` 仍指向同一个 Trap frame。

**Requirements**:
- 不能在调用 `trap()` 前改动 `sp`，也不能在 `trap()` 返回后插入跳转，`__trapret` 紧跟在调用之后。
- 运行 `make grade` 时，Test 1“trap entry, register restore, and breakpoint EPC”（ebreak 前后 t0～t6、s0～s11、a1～a7 全部保持不变）、Test 2（非法指令返回）、Test 3（持续的时钟中断）都依赖本模块。


---

<!-- prompt:T2-trap-handler.prompt.md -->
# T2 功能模块二：中断与异常处理最终提示词

[PROMPT]
任务：实现 Lab3 `code/kern/trap/trap.c` 的中断与异常处理模块。
操作要求：直接修改真实文件，仅在 `//lab3 function2 YOUR CODE` 区域补充必要头文件、常量及两个处理函数，保留 `lab3 function2` 标记并把 `YOUR CODE` 替换为 `2414099`。不改变已有函数、函数签名、初始化流程、汇编入口或评测文件。
输出要求：提供实现、实际编译与运行结果；说明断点/非法指令的返回地址更新和时钟自动关机边界。未运行的验证明确标为未验证。

[RELY]
以下是导入框架提交 `8cc0679` 中已有的接口与定义。宏仅节选本功能所需内容；省略无关代码，不把待实现的函数当作现成依赖。

`kern/trap/trap.h`：
```c
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};

void print_trapframe(struct trapframe *tf);
```
其中 `struct pushregs` 是框架中按寄存器编号排列、含全部 32 个通用寄存器槽位的结构，不改变其定义。

`libs/riscv.h`：
```c
#define IRQ_U_SOFT   0
#define IRQ_S_SOFT   1
#define IRQ_H_SOFT   2
#define IRQ_M_SOFT   3
#define IRQ_U_TIMER  4
#define IRQ_S_TIMER  5
#define IRQ_H_TIMER  6
#define IRQ_M_TIMER  7
#define IRQ_U_EXT    8
#define IRQ_S_EXT    9
#define IRQ_H_EXT    10
#define IRQ_M_EXT    11
#define IRQ_COP      12
#define IRQ_HOST     13
#define CAUSE_ILLEGAL_INSTRUCTION 0x2
#define CAUSE_BREAKPOINT 0x3
#define SIP_SSIP MIP_SSIP
```
还可调用框架的 `clear_csr(reg, bit)` 宏清除 CSR 中相应位。

`kern/driver/clock.h`：
```c
extern volatile size_t ticks;

void clock_init(void);
void clock_set_next_event(void);
```

`libs/sbi.h`：
```c
void sbi_shutdown(void);
```

`kern/trap/trap.c`：
```c
#define TICK_NUM 100

static void print_ticks() {
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}
```
已有 `trap_dispatch()` 依据 `(intptr_t)tf->cause < 0` 区分中断和异常；异常返回路径会把 `tf->epc` 写回 `sepc`，再执行 `sret`。不要直接写 `sepc` 来代替修改帧。

[GUARANTEE]
必须实现接口：
```c
void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);
```
必须交付：两个接口的实现；与实际结果一致的验证记录；在统一环境中以 GDB 观察断点及非法指令的原始 EPC、编码、更新后的 EPC 和 `sret` 后的 PC，并保存真实取证图片。

[SPECIFICATION]
1. `interrupt_handler(struct trapframe *tf)`
   - Pre-Condition：`tf` 指向有效 Trap 帧；实际执行环境为 RV64 S 态。真实中断的 `cause` 最高位置位，测试也可能直接传入不带该位的中断编号。
   - Post-Condition：清除中断标志位后按编号分派，不改变 `tf->epc`。管理态时钟中断先调用 `clock_set_next_event()`，再令 `ticks` 加一；每达 `TICK_NUM` 整数倍调用 `print_ticks()`，累计输出 `SHUTDOWN_AFTER=10` 次后调用 `sbi_shutdown()`。不清零或取模覆盖 `ticks`。
   - Case 1：管理态软件中断清除 `sip.SSIP`，输出 `Supervisor software interrupt`，防止 pending 位一直保留造成重复触发。
   - Case 2：其余框架已定义的中断编号输出对应类型的提示；这里只作分类诊断，不宣称已实现完整设备驱动。
   - Case 3：未知编号先输出 `Unknown interrupt: cause <编号>`，再调用 `print_trapframe(tf)`。直接传入编号 15 时也必须正确输出 15。
   - Requirements：关机次数必须大于 2，让等待 `ticks >= 200` 的评测主流程能在中断返回后继续执行；不额外定义 `DEBUG_GRADE`，避免第一次打印即 panic。

2. `exception_handler(struct trapframe *tf)`
   - Pre-Condition：`tf` 指向有效异常帧；对断点及非法指令，本实验保证故障指令地址可按半字读取，待跳过的指令为 16 位或 32 位。不将此假设推广到任意用户地址、取指故障或长指令扩展。
   - Post-Condition：非法指令输出 `Illegal instruction at` 和原始 EPC，断点输出 `Breakpoint at` 和原始 EPC；随后读取 EPC 处的 `uint16_t` 首半字，低两位为 `11` 时推进 4 字节，否则推进 2 字节。保留帧中其他字段。
   - Case 1：`c.ebreak` 编码 `0x9002`，返回地址增加 2。
   - Case 2：32 位 `ebreak` 编码 `0x00100073`，返回地址增加 4；不能把断点一律当作压缩指令。
   - Case 3：`.word 0x0000007b` 触发非法指令，返回地址增加 4。
   - Case 4：其他异常调用 `print_trapframe(tf)`，不擅自修改 EPC。诊断输出不等同于已恢复未知异常。
   - Requirements：从实际指令编码判断长度，不按异常类型硬编码，不依赖 `badvaddr` 一定包含指令编码；先读半字，避免在仅 2 字节对齐地址进行 32 位读取。

3. 验证要求
   - 保留原评测与初始化锚点；在隔离副本中验证，明确测试对象为提交版基线叠加当前 `trap.c`。有多个 WSL 发行版时，先用 `wsl --list --verbose` 核对，并选择安装 QEMU 4.1.1 的发行版；本机为 `Ubuntu-22.04-OS`，工具链在 `/opt/riscv/bin`。
   - 编译采用原选项；日常运行应周期打印 `100 ticks` 并在第十次打印后关机。统一 QEMU 4.1.1 下的原始 `make grade` 目标为 100/100。
   - GDB 使用硬件断点观察真实异常，不修改 EPC 来制造成功返回；核对 `sret` 后 PC 等于原始 EPC 加实际长度。没有运行证据时，不生成或引用伪造截图。
   - 补测软件中断时区分“直接调用处理函数”和“CPU 递交真实中断”：真实触发需要局部 `sie.SSIE`、全局 `sstatus.SIE` 及挂起 `sip.SSIP` 的条件均满足；核对处理后 SSIP 清零，且实际返回 PC 等于未推进的原始 EPC。调试期的触发与临时指令恢复仅作用于专用来宾实例，不修改仓库或评测源码。


---

<!-- prompt:T3-knowledge.prompt.md -->
# T3 知识点与 OS 原理对应：最终提示词

[PROMPT]
任务：为 Lab3 报告撰写“对操作系统的理解”，保存到 `report/sections/T3-knowledge.md`，负责人为 2414099-李云鹏。
操作要求：结合本仓库真实代码进行分析，不修改代码或最终汇总报告，不替代其他成员负责的整体逻辑分析。参考 `reference/OS_lab2_report.pdf` 的原理与代码对应写法，但不复用其表述。
输出要求：按报告模板的两个问题组织，使用简体中文；每个重要知识点说明含义、与代码的关系及抽象原理和本实验实现的差异。准确区分“实现了什么”和“验证了什么”。

[RELY]
以下为当前代码中可核对的最小上下文，省略与分析无关的行和注释。

`code/kern/trap/trap.h`：
```c
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};
```

`code/kern/trap/trap.c`：
```c
bool trap_in_kernel(struct trapframe *tf) {
    return (tf->status & SSTATUS_SPP) != 0;
}
```

`code/kern/driver/clock.c`：
```c
volatile size_t ticks;
static uint64_t timebase = 100000;
void clock_set_next_event(void) { sbi_set_timer(get_cycles() + timebase); }
```

`code/kern/trap/trapentry.S` 中返回路径的关键指令：
```asm
    LOAD s1, 32*REGBYTES(sp)
    LOAD s2, 33*REGBYTES(sp)
    csrw sstatus, s1
    csrw sepc, s2
```

```asm
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret
```

补充事实：`SAVE_ALL` 分配 `36 * REGBYTES` 的帧；当前为 RV64，`REGBYTES=8`。`trap_dispatch()` 按 cause 最高位区分中断与异常；T2 处理时钟、软件中断、断点和非法指令。`kern_init()` 初始化后进入无限循环，尚无进程调度或用户程序运行流程。

[GUARANTEE]
必须回答：
1. 本实验中哪些知识点重要？它们在 OS 原理中的含义是什么？与本实验的实现有什么联系和差异？
2. OS 原理中哪些重要机制在本实验没有对应实现？明确缺失的组件，不能只列名词。
必须交付：可独立合并的中文报告 section；不虚构运行结果或截图。

[SPECIFICATION]
1. 原理对应分析
   - Pre-Condition：以当前实验代码及已明确的任务范围为依据，不假定已有进程、用户态和完整设备驱动。
   - Post-Condition：至少覆盖中断与异常语义、软硬件上下文保存、特权级与 SBI、时钟机制、EPC 与指令长度、中断挂起和应答。
   - Case 1：区分 Trap 保存现场和切换到另一进程；保存寄存器不代表已实现调度。
   - Case 2：区分中断继续执行、断点/非法指令按实验策略跳过，以及缺页修复后可能重试；不声称所有异常都增加 EPC。
   - Case 3：明确当前只处理可读内核代码中的 16/32 位故障指令，未知异常诊断不等于成功恢复。
   - Case 4：解释 `volatile` 不提供原子性；分类打印外部中断不等于完成中断控制器和设备应答。
2. 未覆盖机制
   - Pre-Condition：把“尚未实现”与“此前实验已有基础”区分开。
   - Post-Condition：说明进程线程与调度、系统调用及用户栈切换、按需分页、同步互斥与多核、完整 I/O、文件系统、故障隔离的作用及本实验缺项。
   - Requirements：不过度扩展成其他实验的实现教程，不改写 openfar 负责的整体流程分析，不将静态分析或编译通过表述为端到端运行成功。


---

<!-- prompt:T3-logic.prompt.md -->
## 实验整体逻辑分析

[PROMPT]

**任务**：为 lab3 实验报告撰写“实验整体逻辑分析”一章，写入 `report/sections/T3-logic.md`，包含“逻辑主线”“功能的逐步实现”“各功能的核心函数”三节，并画一张 Trap 处理全流程图（`report/images/T3-trap-flow.png`）。

**操作要求**：先通读本 lab 已经完成的代码（`trapentry.S`、`trap.c`、`clock.c`、`intr.c`、`sync.h`、`init.c`）和各任务的 section（功能模块一、功能模块二、思考题），以真实代码为准，不复述指导书原文。先用一句话概括主线，再用表格列出实现顺序，逐条说明“为什么排在这里”。流程图用 matplotlib 画成黑白、直角矩形的图片，不用字符画。

**输出要求**：Markdown，语言平实；章节标题由集成脚本编号，文件内从二级标题开始。每个结论都要能在代码或验证记录中找到依据；其他章节的细节只做概括，并写明在报告中的位置。

[RELY]

- `code/kern/init/init.c` 中 `kern_init` 的顺序：`idt_init(); pmm_init(); idt_init(); clock_init(); intr_enable(); while (1);`。
- `code/kern/trap/trap.c`：`idt_init` 执行 `write_csr(sscratch, 0); write_csr(stvec, &__alltraps);`；`trap_dispatch` 以 `(intptr_t)tf->cause < 0` 区分中断与异常。
- `code/kern/trap/trapentry.S`：`SAVE_ALL` 经 `sscratch` 中转保存原 `sp`，在栈上开辟 `36 * REGBYTES` 的 Trap frame；`__alltraps` 以 `.align(2)` 对齐，结尾落入 `__trapret`，执行 `RESTORE_ALL` 后 `sret`。
- `code/kern/trap/trap.h` 中的 `struct pushregs` 与 `struct trapframe`（`gpr`、`status`、`epc`、`badvaddr`、`cause`）。
- `code/kern/driver/clock.c`：`clock_init` 执行 `set_csr(sie, MIP_STIP)` 并调用 `clock_set_next_event()`，后者调用 `sbi_set_timer(get_cycles() + timebase)`；`intr.c` 的 `intr_enable` 置位 `sstatus.SIE`。
- `code/kern/sync/sync.h` 的 `local_intr_save`/`local_intr_restore`，以及 `pmm.c` 中 `alloc_pages`/`free_pages` 对它们的使用。
- `tools/inject.py`：测试分别插在第二个 `idt_init();`、`clock_init();`、`intr_enable();` 之后。
- T1、T2 任务文件中的验证结论（反例实验得分、GDB 观察到的 `sepc`/`pc`）。

[GUARANTEE]

需要回答的问题：

1. 本章围绕什么主题展开，要解决什么问题？用一句话概括主线。
2. 硬件、`trapentry.S`、`trap.c` 在一次 Trap 中各自负责什么？它们之间靠什么衔接？
3. 各功能按什么顺序实现，为什么是这个顺序？每一步依赖前面哪一步的结果？
4. 每个功能的核心函数是什么，哪些由框架给出、哪些由我们实现？

需要交付的产出：Trap 处理全流程图（初始化、硬件响应、`SAVE_ALL`、分派、两个处理函数、`RESTORE_ALL`、`sret` 及返回原执行流的回路，并标注每一步由谁完成）、实现顺序表、核心函数表。

[SPECIFICATION]

**Pre-Condition**：T0～T2 的代码已经完成并通过 `make grade`，各 section 已有初稿。

**Post-Condition**：

- 逻辑主线以“进入—处理—返回”的闭环组织，点明 Trap frame 是汇编与 C 之间的契约这条副线。
- 实现顺序覆盖补齐 lab2 代码、`idt_init`、现场保存与恢复、汇编调用 C、异常处理、中断处理和思考题，每一步说明排序的理由。
- 描述与代码一致，例如：硬件不保存通用寄存器；`sp` 最后恢复；中断返回时不修改 `epc`；时钟定时器每次都要重新设定；关机阈值要大于评测等待的 200 ticks。

**Requirements**：不重复各功能模块章节中的实现细节和迭代过程，只做汇总。


---

<!-- prompt:T4-questions.prompt.md -->
```markdown
[PROMPT]
任务：针对操作系统 Lab 3 指导书中的两道思考题，进行系统、严谨且深入的原理剖析与报告撰写，输出符合 report-template.md 格式的 Markdown 文档。
操作要求：直接基于框架源码与 RISC-V 官方特权级规范，从硬件规范、编译器文法与操作系统内核并发机制三个维度展开深度解答。
输出要求：按层次清晰给出背景、问题现象、原理解析与场景反例，作为报告中的「练习：指导书思考题」小节。

[RELY]
// 1. RISC-V Privileged Architecture Specification (v1.9 vs v1.10) 关于 stvec 的定义演进：
// v1.9: stvec 为 SXLEN 位寄存器，保存 4 字节对齐的 trap vector base address，最低 2 位硬件固定为 0。
// v1.10+: stvec 拆分为 BASE[XLEN-1:2] 与 MODE[1:0]：
//   MODE=0 (Direct): 所有异常与中断统一跳转到 BASE
//   MODE=1 (Vectored): 同步异常跳转到 BASE，异步中断跳转到 BASE + 4 * cause
//   MODE>=2: Reserved

// 2. kern/sync/sync.h 中的中断保护宏与辅助内联函数：
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);

// 3. kern/mm/pmm.c 中内存分配/释放对中断保护宏的使用：
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

[GUARANTEE]
必须系统、全面地回答指导书正文提出的两道核心思考题，不得遗漏任何子问题：
1. 思考题 1：stvec 最低两位的模式编码是 1.10 版才加入的，这个改动如何保证与旧版本兼容？
2. 思考题 2：sync.h 中 local_intr_save 宏用 do { } while (0) 包起来有什么作用？结合 pmm.c 的 alloc_pages / free_pages 说明为什么要先关中断再操作。

[SPECIFICATION]
针对思考题 1（stvec 向后兼容性）：
- Case 1（编码与数值兼容）：说明旧规范对基地址 4 字节对齐的要求，推导旧软件写入的地址最低两位必然为 00，如何天然匹配 1.10 中 MODE=0（Direct 模式）的编码。
- Case 2（地址计算一致性）：分析 1.10 在 Direct 模式下的地址计算公式（BASE << 2），证明其计算出的目标 PC 与旧软件预期地址严格相等。
- Case 3（控制流与架构回退）：分析单入口统一分发控制流模型的承接关系，以及硬件层面将最低两位硬连为 0 时的实现兼容性。

针对思考题 2（宏封装与关中断保护）：
- Case 1（do-while 语法保护）：给出不使用 do-while(0) 时在 if-else 单分支场景下的具体反例与预处理器展开结果，指出末尾多余分号导致编译器报 'else without a previous if' 错误的原因；说明局部作用域隔离与零运行时开销。
- Case 2（临界区与非原子操作）：结合 pmm_manager 的双向空闲链表 free_list、nr_free 计数及 Page 描述符的增删改、拆分与合并过程，说明多步操作非原子性导致的中间不一致状态。
- Case 3（中断重入危害）：详细推演在单核环境下，若不关中断导致异步中断打断 alloc_pages/free_pages，当中断服务程序或调度换入的新线程再次调用内存分配时，产生的双向链表破损、死循环、内核 Page Fault、Double Allocation 等致命故障。
- Case 4（嵌套安全性）：分析 __intr_save() 保存原始 SIE 标志位再关中断、__intr_restore() 按标志恢复的设计对支持临界区嵌套调用的重要意义。
```
