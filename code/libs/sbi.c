// libs/sbi.c
#include <sbi.h>
#include <defs.h>

// SBI 功能编号清单 i.e. sbi_type
uint64_t SBI_SET_TIMER = 0;
uint64_t SBI_CONSOLE_PUTCHAR = 1; 
uint64_t SBI_CONSOLE_GETCHAR = 2;
uint64_t SBI_CLEAR_IPI = 3;
uint64_t SBI_SEND_IPI = 4;
uint64_t SBI_REMOTE_FENCE_I = 5;
uint64_t SBI_REMOTE_SFENCE_VMA = 6;
uint64_t SBI_REMOTE_SFENCE_VMA_ASID = 7;
uint64_t SBI_SHUTDOWN = 8;

// sbi_call - 通用的 SBI 调用函数
// @sbi_type: SBI 功能编号
// @arg0, arg1, arg2: 传递给 SBI 服务的参数
// 返回值：SBI 服务返回的结果
uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0, uint64_t arg1, uint64_t arg2) 
{
    uint64_t ret_val;
    __asm__ volatile (
        // 1. 将功能编号和参数放入指定的寄存器
        "mv x17, %[sbi_type]\n"
        "mv x10, %[arg0]\n"
        "mv x11, %[arg1]\n"
        "mv x12, %[arg2]\n"
        // 2. 执行 ecall 指令，发起调用
        "ecall\n"
        // 3. 将返回值（在 x10/a0 中）移动到 C 变量 ret_val 中
        "mv %[ret_val], x10"
        : [ret_val] "=r" (ret_val)  // 输出操作数：将汇编的结果输出到C变量ret_val
        : [sbi_type] "r" (sbi_type), [arg0] "r" (arg0), [arg1] "r" (arg1), [arg2] "r" (arg2)  // 输入操作数：将C变量sbi_type, arg0, arg1, arg2的值作为输入传给汇编
        : "memory"  // 告知编译器：内联汇编可能会读取或写入内存，防止编译器优化时出错
    );
    return ret_val;
}

// 基于通用的 sbi_call，封装出专用的字符输出函数
void sbi_console_putchar(unsigned char ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
} //相关的月饼包装： 如下
/*
console.c只是简单地封装一下
stdio.c里面实现了一些函数，注意我们已经实现了ucore版本的puts函数: cputs()
我们还在libs/printfmt.c实现了一些复杂的格式化输入输出函数。最后得到的cprintf()函数仍在kern/libs/stdio.c定义，功能和C标准库的printf()基本相同。
printfmt.c还依赖一个头文件riscv.h,这个头文件主要定义了若干和riscv架构相关的宏，尤其是将一些内联汇编的代码封装成宏，使得我们更方便地使用内联汇编来读写寄存器。当然这里我们还没有用到它的强大功能。

*/

void sbi_set_timer(unsigned long long stime_value) {
    sbi_call(SBI_SET_TIMER, stime_value, 0, 0);
}
