#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <riscv.h>

// ![image](../../../.vscode/image-comment/Screenshot-2026-11-05-200729.png)

static inline bool __intr_save(void)  //如果中断本来开着,就关掉并返回 1;本来就关着,返回 0。
{
    //保存 sstatus寄存器中的中断使能位(SIE)信息并屏蔽中断
    if (read_csr(sstatus) & SSTATUS_SIE) //check 中断是否开启。
    {
        intr_disable();
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag) //flag 为 1 才重新开中断。这样的好处是嵌套安全:如果进入临界区前中断已经是关的,退出时就不能擅自打开。
{
    //根据保存的中断使能位信息来使能中断
    if (flag) intr_enable();
}

/*
about syntax: 
// ![image](../../../.vscode/image-comment/Screenshot-2026-11-05-200844.png)
*/

#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);

#endif /* !__KERN_SYNC_SYNC_H__ */
