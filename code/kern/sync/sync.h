#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <riscv.h>

static inline bool __intr_save(void) 
{
    //保存 sstatus寄存器中的中断使能位(SIE)信息并屏蔽中断
    if (read_csr(sstatus) & SSTATUS_SIE) 
    {
        intr_disable();
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag) 
{
    //根据保存的中断使能位信息来使能中断
    if (flag) intr_enable();
}

#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);

#endif /* !__KERN_SYNC_SYNC_H__ */
