/*
C语言编写的内核入口点。主要包含kern_init()函数，从kern/init/entry.S跳转过来完成其他初始化工作。
在 kern/init/init.c 中，kern_init 函数肩负着内核初启的核心任务：

初始化内核环境：例如，清理 .bss 段，为未初始化的全局变量赋零值。
向用户提供可视化反馈：这是操作系统与开发者/用户的第一次交互，通过输出信息告诉我们：“os is loading ...”。
*/

#include <stdio.h>
#include <string.h>
#include <sbi.h>
int kern_init(void) __attribute__((noreturn)); // to tell the compiler that this func wont return . 

int kern_init(void) 
{
    extern char edata[], end[]; //这里声明的两个符号由链接器定义，分别指向.data段结束和.bss段结束
    memset(edata, 0, end - edata); // 清除.bss段：由于内核没有标准库，memset需要我们自己实现

    const char *message = "(THU.CST) os is loading ...\n";


/*
异常：同步的，由当前指令引起（非法指令、ecall、ebreak、缺页）。
限提升的关键指令。

当在 U 模式（用户态）执行 ecall，会触发异常，从而陷入到 S 模式（内核态）。这是系统调用的底层机制。

当在 S 模式（内核态）执行 ecall，会触发异常，从而陷入到 M 模式（机器态）。这正是我们调用 OpenSBI 服务的方式。

通过 ecall 调用 SBI 服务，需要遵循一个明确的调用约定。这个过程类似于在一个预定义的表格中查找一个功能号，然后按照固定的规则传递参数（后面会学习到这就是系统调用的调用格式），最后执行一个特殊指令来触发它。

指定服务编号：将想要调用的 SBI 功能编号（例如，SBI_CONSOLE_PUTCHAR = 1）放入指定的寄存器（通常是 a7 或 x17）。
传递参数：根据 RISC-V 的函数调用约定（Calling Convention），将参数放入寄存器 a0, a1, a2（即 x10, x11, x12）。
执行调用：执行 ecall 指令。CPU 会 trap 到 M 模式，由 OpenSBI 固件处理请求。
获取返回值：处理完成后，OpenSBI 会将返回值放入 a0（x10）寄存器，然后返回。

*/

    cprintf("%s\n\n", message);
   while (1)
        ;
}

