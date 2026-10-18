#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/* All physical memory mapped at this address */
#define KERNBASE            0xFFFFFFFFC0200000 // = 0x80200000(物理内存里内核的起始位置, KERN_BEGIN_PADDR) + 0xFFFFFFFF40000000(偏移量, PHYSICAL_MEMORY_OFFSET)
//把原有内存映射到虚拟内存空间的最后一页
#define KMEMSIZE            0x7E00000          // the maximum amount of physical memory
// 0x7E00000 = 0x8000000 - 0x200000
// QEMU 缺省的RAM为 0x80000000到0x88000000, 128MiB, 0x80000000到0x80200000被OpenSBI占用
#define KERNTOP             (KERNBASE + KMEMSIZE) // 0x88000000对应的虚拟地址

#define PHYSICAL_MEMORY_END         0x88000000
#define PHYSICAL_MEMORY_OFFSET      0xFFFFFFFF40000000
#define KERNEL_BEGIN_PADDR          0x80200000
#define KERNEL_BEGIN_VADDR          0xFFFFFFFFC0200000


#define KSTACKPAGE          2                           // # of pages in kernel stack
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // sizeof kernel stack

#ifndef __ASSEMBLER__

#include <defs.h>
#include <list.h>

typedef uintptr_t pte_t;
typedef uintptr_t pde_t;

/* *
 * struct Page - Page descriptor structures. Each Page describes one
 * physical page. In kern/mm/pmm.h, you can find lots of useful functions
 * that convert Page to other data types, such as physical address.
 * */
/*
内核启动时，page_init 会给每一个物理页建一个 struct Page，存成数组 pages[]。
数组下标就是物理页号，所以页的 Page 地址和它的物理地址可以互相换算
（page2pa、pa2page，见 pmm.h）。
*/
struct Page 
{
    int ref;                        // page frame's reference counter 引用计数（被页表映射/使用了几次）
    uint64_t flags;                 // array of flags that describe the status of the page frame
    unsigned int property;          //空闲块的页数（只有空闲块的首页才有意义） the num of free block, used in first fit pm manager
    list_entry_t page_link;         //用来挂到空闲链表上的"链子" free list link

// Page 里嵌了一个 page_link，空闲页通过它串成链表。free_area_t 里的 free_list 是链表头（哨兵），nr_free 是空闲页总数：


// free_list ⇄ [Page A].page_link ⇄ [Page B].page_link ⇄ ... ⇄ f

// 链表里串起来的是 page_link，不是 Page 本身。这时从链表节点反推出整个 Page，就用 le2page：


// #define le2page(le, member)  to_struct((le), struct Page, member)
// // to_struct = (type*)((char*)ptr - offsetof(type, member))
// 意思是：节点地址减去 page_link 在 Page 内的偏移，就得到所在 Page 的首地址。典型用法：


// list_entry_t *le = list_next(&free_list);
// struct Page *p = le2page(le, page_link);

//这里就用到了 le2page 宏。它的作用是，给定 list_entry* le，以及 member = page_link，利用 to_struct 宏，从 le 的地址向前偏移，得到 Page 结构体的首地址，最终返回 Page*。这种技巧本质上是 “container_of” 的用法（Linux 内核里大量使用），即从结构体中的某个成员指针，反推出整个结构体指针。

};

/* Flags describing the status of a page frame */
#define PG_reserved                 0       // if this bit=1: the Page is reserved for kernel, cannot be used in alloc/free_pages; otherwise, this bit=0 
#define PG_property                 1       // if this bit=1: the Page is the head page of a free memory block(contains some continuous_addrress pages), and can be used in alloc_pages; if this bit=0: if the Page is the the head page of a free memory block, then this Page and the memory block is alloced. Or this Page isn't the head page.

#define SetPageReserved(page)       ((page)->flags |= (1UL << PG_reserved))
#define ClearPageReserved(page)     ((page)->flags &= ~(1UL << PG_reserved))
#define PageReserved(page)          (((page)->flags >> PG_reserved) & 1)
#define SetPageProperty(page)       ((page)->flags |= (1UL << PG_property))
#define ClearPageProperty(page)     ((page)->flags &= ~(1UL << PG_property))
#define PageProperty(page)          (((page)->flags >> PG_property) & 1)

// convert list entry to page
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)

/* free_area_t - maintains a doubly linked list to record free (unused) pages */
typedef struct 
{
    list_entry_t free_list;         // the list header
    unsigned int nr_free;           // number of free pages in this free list
} free_area_t;

#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEMLAYOUT_H__ */
