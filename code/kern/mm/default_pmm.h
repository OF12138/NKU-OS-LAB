#ifndef __KERN_MM_DEFAULT_PMM_H__
#define  __KERN_MM_DEFAULT_PMM_H__

#include <pmm.h>

extern const struct pmm_manager default_pmm_manager; //显式写 extern 的主要作用是：声明一个符号，但不在当前翻译单元中生成定义。

#endif /* ! __KERN_MM_DEFAULT_PMM_H__ */

