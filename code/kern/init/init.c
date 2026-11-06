#include <clock.h>
#include <console.h>
#include <defs.h>
#include <intr.h>
#include <kdebug.h>
#include <kmonitor.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <trap.h>
#include <dtb.h>

int kern_init(void) __attribute__((noreturn));

void grade_backtrace(void);

int kern_init(void) 
{
    extern char edata[], end[];
    memset(edata, 0, end - edata); // clear .bss 
    dtb_init(); //在kern_entry部分的初始化结束，我们正式进入到kern_init之后，会执行dtb_init函数来读取设备树结构中储存的相关信息
    cons_init();  // init the console
    const char *message = "(THU.CST) os is loading ...\0";
    //cprintf("%s\n\n", message);
    cputs(message);
    print_kerninfo();

    /*
    内核初始化函数kern_init()的执行流：
    (从kern/init/entry.S进入) 
    -> 输出一些信息说明正在初始化 
    -> 设置中断向量表(stvec)跳转到的地方为kern/trap/trapentry.S里的一个标记 
    ->在kern/driver/clock.c设置第一个时钟事件，使能时钟中断
    ->设置全局的S mode中断使能位-
    - > 现在开始不断地触发时钟中断
    */

    // grade_backtrace();
    idt_init();  // init interrupt descriptor table

    pmm_init();  // init physical memory management

    idt_init();  // ... 测试需要。

    clock_init();   // init clock interrupt
    intr_enable();  // enable irq interrupt

    /* do nothing */
    while (1)
        ;
}

/*
 * 下面四个函数用于构造多层函数调用，供栈回溯（backtrace）练习使用，
 * 不负责初始化硬件。调用顺序为：
 * grade_backtrace -> grade_backtrace0 -> grade_backtrace1 -> grade_backtrace2
 *                -> mon_backtrace -> print_stackframe。
 * 栈回溯的目标是沿调用关系查看“当前函数是由谁调用的”。
 * 注意：上面的 grade_backtrace(); 已被注释，所以当前启动不会走这条路径；
 * 当前 print_stackframe() 仍是 panic("Not Implemented!")，还不能真正打印调用栈。
 *
 * 语法说明：
 * 1. 开头的 void 是返回类型，表示函数不返回值；没有 return 也可执行到末尾返回。
 * 2. __attribute__((noinline)) 是 GCC 扩展，要求不要把函数体内联到调用处，
 *    方便观察独立的函数调用；它本身不保证关闭尾调用优化或保留帧指针。
 * 3. void 与函数名分成两行只是排版，仍然是一条完整的函数定义。
 * 4. int arg0 等是形参；调用时把实参的值复制给对应形参，arg0 不是特殊关键字。
 */
void __attribute__((noinline))
grade_backtrace2(int arg0, int arg1, int arg2, int arg3) {
    // 最深的一层：调用调试监视器提供的回溯命令。
    // 三个实参依次对应 argc、argv、tf；NULL 表示空指针。
    // 当前 mon_backtrace 不使用这些参数，只调用 print_stackframe()。
    // 本函数的四个形参没有在函数体内使用，主要用于构造回溯练习中的调用现场。
    mon_backtrace(0, NULL, NULL);
}

void __attribute__((noinline)) grade_backtrace1(int arg0, int arg1) {
    // &arg0 / &arg1：取当前函数形参对象的地址，并非读取它们所保存的整数值。
    // (uintptr_t)：显式转换为能表示指针数值的无符号整数类型（本项目为 uint64_t）。
    // 被调用函数的第二、四个形参却是 int，因此 RV64 下还会发生窄化转换，
    // 不能把收到的 int 当作完整的 64 位地址使用；当前回溯练习没有使用这些参数。
    grade_backtrace2(arg0, (uintptr_t)&arg0, arg1, (uintptr_t)&arg1);
}

void __attribute__((noinline)) grade_backtrace0(int arg0, int arg1, int arg2) {
    // 增加一层调用：把本层的第一、第三个参数传给下一层，第二个参数不使用。
    grade_backtrace1(arg0, arg2);
}

// 入口：括号里的 void 表示“不接收参数”，与开头表示“不返回值”的 void 含义不同。
// kern_init 不带 () 时表示函数地址；(uintptr_t) 把它转换为整数供练习传参，
// 这里并不会调用 kern_init。该整数随后也会转换为 int，且下一层不使用它。
// 0xffff0000 是十六进制整数常量；整个单行写法与多行函数定义的语法相同。
void grade_backtrace(void) { grade_backtrace0(0, (uintptr_t)kern_init, 0xffff0000); }

