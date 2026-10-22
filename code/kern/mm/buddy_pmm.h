#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

// 2^15 页为 128 MiB，覆盖当前框架的物理内存管理上限。
#define BUDDY_MAX_ORDER 15

// alloc_pages(n) 占用 2^ceil(log2(n)) 页。
// free_pages 必须传回分配起点和原请求 n（或实际块页数），不支持部分释放。
// 空闲/已分配块的首页 property 均记录实际页数，PG_property 仅标记空闲首页。
extern const struct pmm_manager buddy_pmm_manager;

#endif /* !__KERN_MM_BUDDY_PMM_H__ */
