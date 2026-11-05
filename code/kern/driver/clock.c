#include <clock.h>
#include <defs.h>
#include <sbi.h>
#include <stdio.h>
#include <riscv.h>

/*
通过OpenSBI的接口, 可以读取当前时间(rdtime), 设置时钟事件(sbi_set_timer)，是时钟中断必需的硬件支持。
// ![image](../../../.vscode/image-comment/Screenshot-2026-10-28-212548.png)
例如输出字符的 pipeline: 
内核 → cons_putc → SBI 调用 → OpenSBI → QEMU 模拟的串口 → 终端

我们的“时钟中断”实际上就是”每隔若干个时钟周期执行一次的程序“。
一般而言，可以设置时钟中断间隔设置为CPU频率的1%，也就是每秒钟触发100次时钟中断，避免开销过大

我们需要“每隔若干时间就发生一次时钟中断”，
但是OpenSBI提供的接口一次只能设置一个时钟中断事件。
我们采用的方式是：一开始只设置一个时钟中断，之后每次发生时钟中断的时候，设置下一次的时钟中断。
*/

volatile size_t ticks; ////volatile告诉编译器这个变量可能在其他地方被瞎改一通，所以编译器不要对这个变量瞎优化
//这里的 ticks 是软件统计的时钟中断次数，

static inline uint64_t get_cycles(void)  //return the current time. 
{
#if __riscv_xlen == 64 //对64位和32位架构，读取time的方法是不同的
    uint64_t n;
    __asm__ __volatile__("rdtime %0" : "=r"(n)); //use rdtime to read the csr “time"
    return n;
#else
    uint32_t lo, hi, tmp;
    __asm__ __volatile__(
        "1:\n"
        "rdtimeh %0\n"
        "rdtime %1\n"
        "rdtimeh %2\n"
        "bne %0, %2, 1b"
        : "=&r"(hi), "=&r"(lo), "=&r"(tmp));
    return ((uint64_t)hi << 32) | lo;
#endif
}

// Hardcode timebase
static uint64_t timebase = 100000;

/* *
 * clock_init - initialize 8253 clock to interrupt 100 times per second,
 * and then enable IRQ_TIMER.
 * */
void clock_init(void) 
{
    // enable timer interrupt in sie
    // the clock int is disabled in default. so we need enable it explictly. 
    set_csr(sie, MIP_STIP); //即使 sstatus.SIE 为 1，也可以通过 sie 寄存器屏蔽特定类型的中断。例如，可以只允许时钟中断，而屏蔽软件中断。

    clock_set_next_event(); //设置第一个时钟中断事件

    // initialize time counter 'ticks' to zero
    ticks = 0;

    cprintf("++ setup timer interrupts\n");
}

void clock_set_next_event(void) 
{ 
    sbi_set_timer(get_cycles() + timebase); 
    /*
    相当于：
当前计数是 500000，间隔是 100000，请在计数达到 600000 时提醒我。

这里的 timebase 被这份代码用作计数间隔，并非直接以秒为单位。具体经过多久，取决于时间计数器的频率
    */
        // divided by 500 when using Spike(2MHz)
    // divided by 100 when using QEMU(10MHz)
    // timebase = sbi_timebase() / 500;
}

/*
about clock 
// ![image](../../../.vscode/image-comment/Screenshot-2026-10-29-103533.png)
我们用到的RISCV对时钟中断的硬件支持包括：

OpenSBI提供的sbi_set_timer()接口，可以传入一个时刻，让它在那个时刻触发一次时钟中断
rdtime伪指令，读取一个叫做time的CSR的数值，
表示CPU启动之后经过的真实时间。在不同硬件平台，时钟频率可能不同。
在QEMU上，这个时钟的频率是10MHz, 每过1s, rdtime返回的结果增大10000000


*/