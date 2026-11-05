#include <sbi.h>
#include <defs.h>


uint64_t SBI_SET_TIMER = 0;
uint64_t SBI_CONSOLE_PUTCHAR = 1; 
uint64_t SBI_CONSOLE_GETCHAR = 2;
uint64_t SBI_CLEAR_IPI = 3;
uint64_t SBI_SEND_IPI = 4;
uint64_t SBI_REMOTE_FENCE_I = 5;
uint64_t SBI_REMOTE_SFENCE_VMA = 6;
uint64_t SBI_REMOTE_SFENCE_VMA_ASID = 7;
uint64_t SBI_SHUTDOWN = 8;

uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0, uint64_t arg1, uint64_t arg2) 
{

/*
sbi_call() 先把参数放入约定的寄存器，再执行：
ecall
这是一条主动请求服务的指令，触发的是同步异常。
在本实验中，CPU 转入 M 态的 OpenSBI，处理完再返回 S 态，继续执行 ecall 后面的代码。

先区分两个名字：
- SBI：S 态软件请求底层服务的接口规范。
- OpenSBI：实现这些接口、运行在 M 态的固件
*/
    uint64_t ret_val;
    __asm__ volatile (
        "mv x17, %[sbi_type]\n"
        "mv x10, %[arg0]\n"
        "mv x11, %[arg1]\n"
        "mv x12, %[arg2]\n"
        "ecall\n"
        "mv %[ret_val], x10"
        : [ret_val] "=r" (ret_val)
        : [sbi_type] "r" (sbi_type), [arg0] "r" (arg0), [arg1] "r" (arg1), [arg2] "r" (arg2)
        : "memory"
    );

    return ret_val;
} //在本实验环境中，sbi_call 内的 ecall 会触发一次 S → M 的 Trap；时钟事件则由 OpenSBI 和内核分层处理

void sbi_console_putchar(unsigned char ch) 
{
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
}

void sbi_set_timer(unsigned long long stime_value) 
{
    //当time寄存器(rdtime的返回值)为stime_value的时候触发一个时钟中断
    //设置定时器时进入 OpenSBI，与之后定时器到期，是两次不同的事件。
    sbi_call(SBI_SET_TIMER, stime_value, 0, 0);
/*
S 态内核：sbi_set_timer(...)
    ↓ 调用 sbi_call，执行 ecall
M 态 OpenSBI：设置底层定时器
    ↓ 返回
S 态内核：继续执行
*/

}


/*
btw, 定时器到期后的路径是

硬件定时器到期
    ↓
M 态 OpenSBI 处理机器级定时器中断
    ↓ 设置 S 态定时器中断待处理位 STIP
返回 S 态，满足使能条件后
    ↓
CPU 进入 S 态 Trap 入口 __alltraps
    ↓
内核处理：统计 ticks、安排下一次定时事件等

// ![image](../../.vscode/image-comment/Screenshot-2026-10-29-112712.png)

*/

int sbi_console_getchar(void) {
    return sbi_call(SBI_CONSOLE_GETCHAR, 0, 0, 0);
}

void sbi_shutdown(void)
{
	sbi_call(SBI_SHUTDOWN, 0, 0, 0);
}