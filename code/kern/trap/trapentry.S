#include <riscv.h>

# 我们把中断入口点设置为这段汇编代码。这段汇编代码把寄存器的数据挪来挪去，进行上下文切换。
# 在RISC-V架构中，Trap是一个总称，指代任何导致控制权从当前执行流转移到陷阱处理程序的事件。Trap分为两类：
# 中断（Interrupt）：异步事件，由外部设备触发，与当前执行的指令无关。例如：定时器中断、磁盘I/O完成中断。
# 异常（Exception）：同步事件，由当前执行的指令触发。例如：访问无效内存地址、执行非法指令、缺页异常，以及执行ecall和ebreak指令。
# 由于中断处理需要进行较高权限的操作，中断处理程序一般处于内核态，或者说，处于"比被打断的程序更高的特权级"。

# ![image](../../../.vscode/image-comment/Screenshot-2026-10-29-094633.png)


    .macro SAVE_ALL  #定义汇编宏 ， 意思是：“把这段文本命名为 SAVE_ALL，后面遇到这个名字时，展开这些指令

    csrw sscratch, sp #保存原先的栈顶指针到sscratch

    addi sp, sp, -36 * REGBYTES

#REGBYTES是riscv.h定义的常量，表示一个寄存器占据几个字节
    #让栈顶指针向低地址空间延伸 36个寄存器的空间，可以放下一个trapFrame结构体。
    #除了32个通用寄存器，我们还要保存4个和中断有关的CSR

#依次保存32个通用寄存器。但栈顶指针需要特殊处理。
    #因为我们想在trapFrame里保存分配36个REGBYTES之前的sp
    #也就是保存之前写到sscratch里的sp的值

    # save x registers
    STORE x0, 0*REGBYTES(sp)
    STORE x1, 1*REGBYTES(sp)
    STORE x3, 3*REGBYTES(sp)
    STORE x4, 4*REGBYTES(sp)
    STORE x5, 5*REGBYTES(sp)
    STORE x6, 6*REGBYTES(sp)
    STORE x7, 7*REGBYTES(sp)
    STORE x8, 8*REGBYTES(sp)
    STORE x9, 9*REGBYTES(sp)
    STORE x10, 10*REGBYTES(sp)
    STORE x11, 11*REGBYTES(sp)
    STORE x12, 12*REGBYTES(sp)
    STORE x13, 13*REGBYTES(sp)
    STORE x14, 14*REGBYTES(sp)
    STORE x15, 15*REGBYTES(sp)
    STORE x16, 16*REGBYTES(sp)
    STORE x17, 17*REGBYTES(sp)
    STORE x18, 18*REGBYTES(sp)
    STORE x19, 19*REGBYTES(sp)
    STORE x20, 20*REGBYTES(sp)
    STORE x21, 21*REGBYTES(sp)
    STORE x22, 22*REGBYTES(sp)
    STORE x23, 23*REGBYTES(sp)
    STORE x24, 24*REGBYTES(sp)
    STORE x25, 25*REGBYTES(sp)
    STORE x26, 26*REGBYTES(sp)
    STORE x27, 27*REGBYTES(sp)
    STORE x28, 28*REGBYTES(sp)
    STORE x29, 29*REGBYTES(sp)
    STORE x30, 30*REGBYTES(sp)
    STORE x31, 31*REGBYTES(sp)

    # get sr, epc, badvaddr, cause
    # Set sscratch register to 0, so that if a recursive exception
    # occurs, the exception vector knows it came from the kernel
     # RISCV不能直接从CSR写到内存, 需要csrr把CSR读取到通用寄存器，再从通用寄存器STORE到内存
    csrrw s0, sscratch, x0 //write the old sp in the sscratch to s0 and then clear sscratch to zero. 
    csrr s1, sstatus #存 S 态相关的 CPU 状态
    csrr s2, sepc
    csrr s3, sbadaddr #与异常有关的地址或信息 （旧名称，当前规范名称为 stval；例如缺页时可记录出错的虚拟地址）
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp)
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm #汇编宏定义结束

    .macro RESTORE_ALL

    #lab3 function1 2411264

    # 先用 s1、s2 作中转写回 sstatus 和 sepc（sret 依据它们返回），s1、s2 本身随后再恢复
    # badvaddr、cause 只是给 trap() 看的现场记录，sret 不读它们，不需要写回
    LOAD s1, 32*REGBYTES(sp)
    LOAD s2, 33*REGBYTES(sp)
    csrw sstatus, s1
    csrw sepc, s2

    #根据中断或者异常的类型重新设置sepc，确保程序能够从正确的地址继续执行。对于系统调用，这通常是 ecall指令的下一条指令地址（即sepc + 4）；对于中断，这是被中断打断的指令地址（即sepc）；对于进程切换，这是新进程的起始地址。然后，将sstatus.SPP设置为 0，表示要返回到 U 模式。

    # 按 SAVE_ALL 的布局恢复通用寄存器，x0 恒为 0 不用恢复
    LOAD x1, 1*REGBYTES(sp)
    LOAD x3, 3*REGBYTES(sp)
    LOAD x4, 4*REGBYTES(sp)
    LOAD x5, 5*REGBYTES(sp)
    LOAD x6, 6*REGBYTES(sp)
    LOAD x7, 7*REGBYTES(sp)
    LOAD x8, 8*REGBYTES(sp)
    LOAD x9, 9*REGBYTES(sp)
    LOAD x10, 10*REGBYTES(sp)
    LOAD x11, 11*REGBYTES(sp)
    LOAD x12, 12*REGBYTES(sp)
    LOAD x13, 13*REGBYTES(sp)
    LOAD x14, 14*REGBYTES(sp)
    LOAD x15, 15*REGBYTES(sp)
    LOAD x16, 16*REGBYTES(sp)
    LOAD x17, 17*REGBYTES(sp)
    LOAD x18, 18*REGBYTES(sp)
    LOAD x19, 19*REGBYTES(sp)
    LOAD x20, 20*REGBYTES(sp)
    LOAD x21, 21*REGBYTES(sp)
    LOAD x22, 22*REGBYTES(sp)
    LOAD x23, 23*REGBYTES(sp)
    LOAD x24, 24*REGBYTES(sp)
    LOAD x25, 25*REGBYTES(sp)
    LOAD x26, 26*REGBYTES(sp)
    LOAD x27, 27*REGBYTES(sp)
    LOAD x28, 28*REGBYTES(sp)
    LOAD x29, 29*REGBYTES(sp)
    LOAD x30, 30*REGBYTES(sp)
    LOAD x31, 31*REGBYTES(sp)

    # sp 最后恢复：2 号槽位存的是 SAVE_ALL 经 sscratch 转存的 Trap 前的 sp，
    # 一旦恢复，sp 就不再指向 Trap frame，之后不能再从帧里取数据
    LOAD x2, 2*REGBYTES(sp)
    .endm





    .globl __alltraps
    .align(2)
__alltraps:
    SAVE_ALL

    #lab3 function1 2411264
    # 此时 sp 指向 Trap frame 的起始地址；按调用约定第一个参数放在 a0
    move  a0, sp  #第一个参数 = 当前 Trap frame 的地址
    jal trap #就是调用 trap.c 中的 trap()
    # sp should be the same as before "jal trap"

    .globl __trapret
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret #这是一条特权指令，用于从S模式返回


# about 函数传参（RISCV)
# RISC-V 普通整数/指针参数使用 a0～a7
# ![image](../../../.vscode/image-comment/Screenshot-2026-10-29-113738.png)
