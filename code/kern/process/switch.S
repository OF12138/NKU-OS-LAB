//LAB4 function3 2411264
#include <riscv.h>

# void switch_to(struct context *from, struct context *to)
# a0 = from（旧线程 context 的地址），a1 = to（新线程 context 的地址）。
# struct context 依次是 ra、sp、s0～s11 共 14 个 8 字节字段（见 proc.h），偏移依次为 0、1、…、13 个 REGBYTES。
# 只保存/恢复“被调用者保存”的寄存器（ra、sp、s0～s11）：switch_to 是被 C 代码当作普通函数调用的，
# 调用者保存的寄存器（t0～t6、a0～a7）按调用约定已由调用方自行保存，不需要这里处理。
# 只做寄存器的保存和恢复，不涉及调度策略。
# 恢复完成后 ret 跳到新的 ra：
#   - 新线程第一次运行：ra 是 copy_thread 设置的 forkret；
#   - 以前被切走的线程：ra 是它当时调用 switch_to 的返回地址（proc_run 中），于是从 proc_run 继续执行。
.text
.globl switch_to
switch_to:
    # 保存 from 的寄存器
    STORE ra, 0*REGBYTES(a0)
    STORE sp, 1*REGBYTES(a0)
    STORE s0, 2*REGBYTES(a0)
    STORE s1, 3*REGBYTES(a0)
    STORE s2, 4*REGBYTES(a0)
    STORE s3, 5*REGBYTES(a0)
    STORE s4, 6*REGBYTES(a0)
    STORE s5, 7*REGBYTES(a0)
    STORE s6, 8*REGBYTES(a0)
    STORE s7, 9*REGBYTES(a0)
    STORE s8, 10*REGBYTES(a0)
    STORE s9, 11*REGBYTES(a0)
    STORE s10, 12*REGBYTES(a0)
    STORE s11, 13*REGBYTES(a0)

    # 恢复 to 的寄存器
    LOAD ra, 0*REGBYTES(a1)
    LOAD sp, 1*REGBYTES(a1)
    LOAD s0, 2*REGBYTES(a1)
    LOAD s1, 3*REGBYTES(a1)
    LOAD s2, 4*REGBYTES(a1)
    LOAD s3, 5*REGBYTES(a1)
    LOAD s4, 6*REGBYTES(a1)
    LOAD s5, 7*REGBYTES(a1)
    LOAD s6, 8*REGBYTES(a1)
    LOAD s7, 9*REGBYTES(a1)
    LOAD s8, 10*REGBYTES(a1)
    LOAD s9, 11*REGBYTES(a1)
    LOAD s10, 12*REGBYTES(a1)
    LOAD s11, 13*REGBYTES(a1)

    ret
