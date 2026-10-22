#include <console.h>
#include <defs.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <dtb.h>
#include <slub.h>
#include <buddy_pmm.h>

int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);
void print_kerninfo(void);

/* *
 * print_kerninfo - print the information about kernel, including the location
 * of kernel entry, the start addresses of data and text segements, the start
 * address of free memory and how many memory that kernel has used.
 * */
void print_kerninfo(void) {
    extern char etext[], edata[], end[];
    cprintf("Special kernel symbols:\n");
    cprintf("  entry  0x%016lx (virtual)\n", (uintptr_t)kern_init);
    cprintf("  etext  0x%016lx (virtual)\n", etext);
    cprintf("  edata  0x%016lx (virtual)\n", edata);
    cprintf("  end    0x%016lx (virtual)\n", end);
    cprintf("Kernel executable memory footprint: %dKB\n",
            (end - (char*)kern_init + 1023) / 1024);
}

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

    // grade_backtrace();
    pmm_init();  // init physical memory management
    slub_init(); // 在页分配器之上建立 SLUB 对象分配器（Challenge）
    slub_check(); // lab2 2414099：验证对象分配、释放和页面回收
    // lab2 2411264：Buddy 独立测试。按运行时的管理器判断，不用 PMM_buddy 宏，免 clean 切换时 init.o 不会重编
    if (pmm_manager == &buddy_pmm_manager) {
        buddy_test();
    }

    /* do nothing */
    while (1)
        ;
}

