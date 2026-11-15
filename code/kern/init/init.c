#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <kdebug.h>
#include <picirq.h>
#include <trap.h>
#include <clock.h>
#include <intr.h>
#include <pmm.h>
#include <vmm.h>
#include <proc.h>
#include <kmonitor.h>
#include <dtb.h>

int kern_init(void) __attribute__((noreturn));

void grade_backtrace(void);

int kern_init(void)
{
    extern char edata[], end[];
    memset(edata, 0, end - edata); // clear .bss 
    dtb_init(); //在kern_entry部分的初始化结束，我们正式进入到kern_init之后，会执行dtb_init函数来读取设备树结构中储存的相关信息
    cons_init(); // init the console

    const char *message = "(THU.CST) os is loading ...";
    cprintf("%s\n\n", message);

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

    pmm_init(); // init physical memory management

    pic_init(); // init interrupt controller
    idt_init(); // init interrupt descriptor table

    vmm_init();  // init virtual memory management
    proc_init(); // init process table

    clock_init();  // init clock interrupt
    intr_enable(); // enable irq interrupt

    cpu_idle(); // run idle process
}

static void
lab1_print_cur_status(void)
{
    static int round = 0;
    round++;
}
