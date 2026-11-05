#ifndef __KERN_TRAP_TRAP_H__
#define __KERN_TRAP_TRAP_H__

#include <defs.h>

struct pushregs  //gpr
{
    uintptr_t zero;  // Hard-wired zero
    uintptr_t ra;    // Return address
    uintptr_t sp;    // Stack pointer
    uintptr_t gp;    // Global pointer
    uintptr_t tp;    // Thread pointer
    uintptr_t t0;    // Temporary
    uintptr_t t1;    // Temporary
    uintptr_t t2;    // Temporary
    uintptr_t s0;    // Saved register/frame pointer
    uintptr_t s1;    // Saved register
    uintptr_t a0;    // Function argument/return value
    uintptr_t a1;    // Function argument/return value
    uintptr_t a2;    // Function argument
    uintptr_t a3;    // Function argument
    uintptr_t a4;    // Function argument
    uintptr_t a5;    // Function argument
    uintptr_t a6;    // Function argument
    uintptr_t a7;    // Function argument
    uintptr_t s2;    // Saved register
    uintptr_t s3;    // Saved register
    uintptr_t s4;    // Saved register
    uintptr_t s5;    // Saved register
    uintptr_t s6;    // Saved register
    uintptr_t s7;    // Saved register
    uintptr_t s8;    // Saved register
    uintptr_t s9;    // Saved register
    uintptr_t s10;   // Saved register
    uintptr_t s11;   // Saved register
    uintptr_t t3;    // Temporary
    uintptr_t t4;    // Temporary
    uintptr_t t5;    // Temporary
    uintptr_t t6;    // Temporary
};

struct trapframe 
{
    /*
    在64位RISCV架构里我们定义uintptr_t为64位无符号整数
    一个trapFrame结构体占据36个uintptr_t的空间
    里面依次排列通用寄存器x0到x31,然后依次排列4个和中断相关的CSR, 我们希望中断处理程序能够利用这几个CSR的数值。
    */
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc; //保存了被中断指令的虚拟地址
    uintptr_t badvaddr;
    uintptr_t cause; //记录了一个编码，精确指出了中断或异常的具体原因
};

void trap(struct trapframe *tf);
void idt_init(void);
void print_trapframe(struct trapframe *tf);
void print_regs(struct pushregs* gpr);
bool trap_in_kernel(struct trapframe *tf);

#endif /* !__KERN_TRAP_TRAP_H__ */


/*
About Registers. :
在 RISC-V 架构中，寄存器主要可以分为三类：
1. 通用寄存器（GPR）：存储数据、地址、函数参数等。
2. 程序计数器（PC）：记录当前执行的指令地址。
3. 控制与状态寄存器（CSR）：管理 CPU 的运行状态、中断、异常和特权级。
此外，RISC-V 还可以通过扩展支持浮点寄存器、向量寄存器


一、什么是 CSR 寄存器？
CSR = Control and Status Register（控制与状态寄存器）。

CSR 不是某一个具体的寄存器，而是一类特殊寄存器。

它们主要用于：
- 控制 CPU：设置中断开关、异常处理入口等。
- 记录 CPU 状态：保存异常原因、异常发生位置等。
- 管理操作系统：管理特权级、虚拟内存等。
与普通寄存器不同，CSR 通常通过专门的 CSR 指令访问，并且部分 CSR 只能在特定的特权级访问。

// ![image](../../../.vscode/image-comment/Screenshot-2026-10-28-222717.png)

二、RISC-V 的 32 个通用寄存器
标准 RV32I/RV64I 具有 32 个整数通用寄存器，编号为 x0 到 x31。
- RV32I：每个寄存器 32 位。
- RV64I：每个寄存器 64 位。
每个寄存器都有编号和 ABI 名称，例如 x1 也叫 ra


三组特别重要的寄存器
1. 参数寄存器 a0–a7

负责函数参数传递，a0 和 a1 还负责返回值传递。
2. 保存寄存器 s0–s11

按照标准调用约定，如果被调用函数修改了这些寄存器，就必须在返回前恢复原值。

相比之下，t0–t6 属于临时寄存器，被调用函数无需保留它们的原值
3. 栈指针 sp 与返回地址 ra

// ![image](../../../.vscode/image-comment/Screenshot-2026-10-28-222941.png)

*/
