/*
 * Copyright (C) 2015 Regents of the University of California
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#ifndef __SBI_H__
#define __SBI_H__

#include <defs.h>

#define SBI_SET_TIMER 0
#define SBI_CONSOLE_PUTCHAR 1
#define SBI_CONSOLE_GETCHAR 2
#define SBI_CLEAR_IPI 3
#define SBI_SEND_IPI 4
#define SBI_REMOTE_FENCE_I 5
#define SBI_REMOTE_SFENCE_VMA 6
#define SBI_REMOTE_SFENCE_VMA_ASID 7
#define SBI_SHUTDOWN 8

/*
SBI_CALL 先把参数放入约定的寄存器，再执行：
ecall
这是一条主动请求服务的指令，触发的是同步异常。
在本实验中，CPU 转入 M 态的 OpenSBI，处理完再返回 S 态，继续执行 ecall 后面的代码。

先区分两个名字：
- SBI：S 态软件请求底层服务的接口规范。
- OpenSBI：实现这些接口、运行在 M 态的固件

（lab3 中这是 libs/sbi.c 里的 sbi_call() 函数，lab4 框架改成了本头文件中的宏和 static inline 函数）
在本实验环境中，ecall 会触发一次 S → M 的 Trap；时钟事件则由 OpenSBI 和内核分层处理
*/
#define SBI_CALL(which, arg0, arg1, arg2) ({			\
	register uintptr_t a0 asm ("a0") = (uintptr_t)(arg0);	\
	register uintptr_t a1 asm ("a1") = (uintptr_t)(arg1);	\
	register uintptr_t a2 asm ("a2") = (uintptr_t)(arg2);	\
	register uintptr_t a7 asm ("a7") = (uintptr_t)(which);	\
	asm volatile ("ecall"					\
		      : "+r" (a0)				\
		      : "r" (a1), "r" (a2), "r" (a7)		\
		      : "memory");				\
	a0;							\
})

/* Lazy implementations until SBI is finalized */
#define SBI_CALL_0(which) SBI_CALL(which, 0, 0, 0)
#define SBI_CALL_1(which, arg0) SBI_CALL(which, arg0, 0, 0)
#define SBI_CALL_2(which, arg0, arg1) SBI_CALL(which, arg0, arg1, 0)

static inline void sbi_console_putchar(int ch)
{
	SBI_CALL_1(SBI_CONSOLE_PUTCHAR, ch);
}

static inline int sbi_console_getchar(void)
{
	return SBI_CALL_0(SBI_CONSOLE_GETCHAR);
}

static inline void sbi_set_timer(uint64_t stime_value)
{
    //当time寄存器(rdtime的返回值)为stime_value的时候触发一个时钟中断
    //设置定时器时进入 OpenSBI，与之后定时器到期，是两次不同的事件。
/*
S 态内核：sbi_set_timer(...)
    ↓ 执行 SBI_CALL 中的 ecall
M 态 OpenSBI：设置底层定时器
    ↓ 返回
S 态内核：继续执行
*/


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
#if __riscv_xlen == 32
	SBI_CALL_2(SBI_SET_TIMER, stime_value, stime_value >> 32);
#else
	SBI_CALL_1(SBI_SET_TIMER, stime_value);
#endif
}

static inline void sbi_shutdown(void)
{
	SBI_CALL_0(SBI_SHUTDOWN);
}

static inline void sbi_clear_ipi(void)
{
	SBI_CALL_0(SBI_CLEAR_IPI);
}

static inline void sbi_send_ipi(const unsigned long *hart_mask)
{
	SBI_CALL_1(SBI_SEND_IPI, hart_mask);
}

static inline void sbi_remote_fence_i(const unsigned long *hart_mask)
{
	SBI_CALL_1(SBI_REMOTE_FENCE_I, hart_mask);
}

static inline void sbi_remote_sfence_vma(const unsigned long *hart_mask,
					 unsigned long start,
					 unsigned long size)
{
	SBI_CALL_1(SBI_REMOTE_SFENCE_VMA, hart_mask);
}

static inline void sbi_remote_sfence_vma_asid(const unsigned long *hart_mask,
					      unsigned long start,
					      unsigned long size,
					      unsigned long asid)
{
	SBI_CALL_1(SBI_REMOTE_SFENCE_VMA_ASID, hart_mask);
}

#endif /* !__SBI_H__ */
