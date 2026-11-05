#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>
#include <stdio.h>
#include <trap.h>

/*
 分发不同类型的中断给不同的handler, 完成上下文切换之后对中断的具体处理，
 例如外设中断要处理外设发来的信息，时钟中断要触发特定的事件。
 中断处理初始化的函数也在这里，主要是把中断向量表(stvec)设置成所有中断都要跳到trapentry.S进行处理。
*/


#define TICK_NUM 100

static void print_ticks() 
{
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* 
idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S
 */
void idt_init(void) 
{

    extern void __alltraps(void); //extern void __alltraps(void); 是在 C 中声明一个在其他地方定义的入口符号，对应 trapentry.S 里的标签 , 链接器会把 C 中对 __alltraps 的引用，与汇编中定义的地址关联起

    //约定：若中断前处于S态，sscratch为0
    //若中断前处于U态，sscratch存储内核栈地址
    //那么之后就可以通过sscratch的数值判断是内核态产生的中断还是用户态产生的中断
    //我们现在是内核态所以给sscratch置零

    /* Set sup0 scratch register to 0, indicating to exception vector
       that we are presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps); ////我们保证__alltraps的地址是四字节对齐的，将__alltraps这个符号的地址直接写到stvec寄存器
}

/* trap_in_kernel - test if trap happened in kernel */
bool trap_in_kernel(struct trapframe *tf) 
{
    return (tf->status & SSTATUS_SPP) != 0;
}

void print_trapframe(struct trapframe *tf) 
{
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs *gpr) 
{
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}

//lab3 function2 2414099
#include <sbi.h>

#define SHUTDOWN_AFTER 10

void interrupt_handler(struct trapframe *tf) {
    // 清除 scause 最高位的 Interrupt 标志，保留低位的中断编号
    uintptr_t cause = tf->cause & ~((uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1));
    static size_t printed_ticks = 0;        // 用于记录已经打印了多少次时钟信息

    switch (cause) {
        case IRQ_U_SOFT:
            cprintf("User software interrupt\n");
            break;
        case IRQ_S_SOFT:
            // 就是尝试清除 S 模式软件中断的待处理标志
            // sip：Supervisor Interrupt Pending，记录哪些中断正处于待处理状态
            // SIP_SSIP：sip 中表示 S 模式软件中断待处理的标志位
            clear_csr(sip, SIP_SSIP);
            cprintf("Supervisor software interrupt\n");
            break;
        case IRQ_H_SOFT:
            cprintf("Hypervisor software interrupt\n");
            break;
        case IRQ_M_SOFT:
            cprintf("Machine software interrupt\n");
            break;
        case IRQ_U_TIMER:
            cprintf("User timer interrupt\n");
            break;
        case IRQ_S_TIMER:
            // 设置下一次时钟中断
            clock_set_next_event();
            // 累加时钟中断次数
            ticks++;

            // 检查是否达到指定节拍数
            if (ticks % TICK_NUM == 0) {
                // 打印信息并检查是否需要关机
                print_ticks();
                if (++printed_ticks == SHUTDOWN_AFTER) {
                    sbi_shutdown();
                }
            }
            break;
        case IRQ_H_TIMER:
            cprintf("Hypervisor timer interrupt\n");
            break;
        case IRQ_M_TIMER:
            cprintf("Machine timer interrupt\n");
            break;
        case IRQ_U_EXT:
            cprintf("User external interrupt\n");
            break;
        case IRQ_S_EXT:
            cprintf("Supervisor external interrupt\n");
            break;
        case IRQ_H_EXT:
            cprintf("Hypervisor external interrupt\n");
            break;
        case IRQ_M_EXT:
            cprintf("Machine external interrupt\n");
            break;
        case IRQ_COP:
            cprintf("Coprocessor interrupt\n");
            break;
        case IRQ_HOST:
            cprintf("Host interrupt\n");
            break;
        default:
            cprintf("Unknown interrupt: cause %lu\n", (unsigned long)cause);
            print_trapframe(tf);
            break;
    }
}

void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
        // 处理非法指令异常和断点异常
        case CAUSE_ILLEGAL_INSTRUCTION:
            cprintf("Illegal instruction at 0x%016lx\n", (unsigned long)tf->epc);
            break;
        case CAUSE_BREAKPOINT:
            cprintf("Breakpoint at 0x%016lx\n", (unsigned long)tf->epc);
            break;
        default:
            print_trapframe(tf);
            return;
    }

    // 修改 tf->epc，让 CPU 跳过发生异常的那条指令
    // RISC-V 常见指令有 16 位和 32 位两种长度。 在本实验范围内，通过指令最低两位判断应该让 epc 增加 2 还是 4。
    uint16_t instruction = *(const volatile uint16_t *)tf->epc;
    if ((instruction & 0x3) == 0x3) {
        tf->epc += 4; // 4 bytes. 
    } else {
        tf->epc += 2;
    }
}


static inline void trap_dispatch(struct trapframe *tf) 
{
    //trap.c的中断处理函数trap, 实际上把中断处理,异常处理的工作分发给了interrupt_handler()，exception_handler(), 这些函数再根据中断或异常的不同类型来处理
    if ((intptr_t)tf->cause < 0) //just one trick : 把无符号数当成有符号数来看，实际上就是看最高位。
    {
        // interrupts
        interrupt_handler(tf);
    } 
    else 
    {
        // exceptions
        exception_handler(tf);
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap()
 * returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe *tf) 
{
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}

/*
// ![image](../../../.vscode/image-comment/Screenshot-2026-11-05-184130.png)

structure of scause. 
// ![image](../../../.vscode/image-comment/Screenshot-2026-10-29-103218.png)

*/