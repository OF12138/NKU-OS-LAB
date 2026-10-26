#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>
#include <list.h>

/* *
 * SLUB 对象分配器（简化版），两层结构：
 *   第一层：页级分配，直接使用当前 pmm_manager 的 alloc_pages()/free_pages()；
 *   第二层：在页之上按对象大小切分，提供 kmem_cache_* 和 kmalloc()/kfree()。
 * 每个 slab 占一页，页首放 struct slab 头，后面是等长的对象；空闲对象的前 8 字节存放
 * 下一个空闲对象的地址（freelist 嵌在对象里，这是 SLUB 与 SLAB 的主要区别之一）。
 * 具体 SLUB 解释：
 * https://claude.ai/share/b2e43b92-8566-47c0-ba08-d334d8b268ec
 * */

#define SLUB_MIN_ALIGN      sizeof(void *)  // 对象至少按指针对齐，空闲时要存下一个指针
#define SLUB_MAX_SIZE       1024            // kmalloc 走 slab 的最大请求大小，更大的请求直接按页分配
#define KMALLOC_NR_CACHES   7               // kmalloc 的通用缓存：16 32 64 128 256 512 1024

#define SLAB_MAGIC          0x51ABu         // slab 页
#define LARGE_MAGIC         0x1A26u         // kmalloc 大块页

// struct slab - 位于每个 slab 页（或大块的首页）开头的管理头
struct slab 
{
    uint32_t magic;                 // SLAB_MAGIC 或 LARGE_MAGIC，kfree 据此区分两类内存
    uint32_t inuse;                 // 已分配出去的对象数
    struct kmem_cache *cache;       // 所属缓存（大块时为 NULL）

    void *freelist;                 // 本 slab 第一个空闲对象，NULL 表示已满
    size_t npages;                  // 大块占用的页数（slab 页为 1）

    list_entry_t slab_link;         // 挂在 cache->partial 上
};

// struct kmem_cache - 管理一种大小的对象
struct kmem_cache //kernal memory pool . 
{
    const char *name;
    size_t object_size;             // 用户请求的对象大小
    size_t size;                    // 实际步长：object_size 向上对齐到 align
    size_t align;
    size_t offset;                  // 第一个对象相对页首的偏移（跳过 struct slab）
    unsigned int objs_per_slab;

    struct slab *cpu_slab;          // 当前活动 slab，分配优先从这里取（对应 Linux 的 per-CPU slab）
    list_entry_t partial;           // 部分空闲的 slab；满的 slab 不挂在任何链表上

    size_t nr_partial;
    size_t nr_slabs;                // 本缓存当前持有的 slab 页数
    size_t nr_inuse;                // 本缓存已分配出去的对象总数
    
    list_entry_t cache_link;        // 挂在全局缓存链表上
};

void slub_init(void);
void slub_check(void);  // lab2 2414099：初始化后的独立测试

struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align);
int kmem_cache_destroy(struct kmem_cache *cache);
void *kmem_cache_alloc(struct kmem_cache *cache);
void kmem_cache_free(struct kmem_cache *cache, void *obj);

void *kmalloc(size_t size);
void kfree(void *ptr);
size_t ksize(const void *ptr);

struct kmem_cache *kmalloc_cache(size_t size);  // 返回 kmalloc 处理 size 时使用的缓存，大块返回 NULL
void slub_print_stats(void);

#endif /* !__KERN_MM_SLUB_H__ */

/* *
 * ====================================================================
 *  SLUB 组件与链表关系示意图（图中字段名对应 slub.c / 本文件的结构体）
 *  图内用英文标识符，中文说明写在图外，避免全角字符破坏对齐。
 * ====================================================================
 *
 * 【图 1】分层：谁调用谁
 *
 *   内核其它模块
 *     |  kmalloc(n) / kfree(p) / ksize(p)      |  kmem_cache_create / alloc / free / destroy
 *     v                                         v
 *   +-----------------------------------------------------------------+
 *   | 第二层 slub.c：按对象大小切分                                     |
 *   |   kmalloc  --(n <= 1024)--> kmalloc_caches[i] --> kmem_cache_alloc |
 *   |   kmalloc  --(n >  1024)--> 直接向第一层要整页（大块）            |
 *   +-----------------------------------------------------------------+
 *     |  alloc_page() / alloc_pages(n)         ^  free_page() / free_pages(p, n)
 *     v                                         |
 *   +-----------------------------------------------------------------+
 *   | 第一层 pmm_manager（default_pmm / best_fit_pmm ...）              |
 *   |   用 struct Page 描述每个物理页，空闲块用 Page.page_link 串在     |
 *   |   pmm 自己的 free_list 上                                         |
 *   +-----------------------------------------------------------------+
 *     slub 拿到 struct Page 后，用 page2kva(page) 换算成内核虚拟地址使用；
 *     释放时用 kva2page(addr) 换回 Page（PA = VA - va_pa_offset，再 pa2page）。
 *
 *
 * 【图 2】全局：cache_list 把所有cache串起来（链接字段 kmem_cache.cache_link）
 *
 *   cache_list (static 链表头)
 *     |next                                                        ^prev
 *     v                                                            |
 *   [cache_cache] <-> [kmalloc-16] <-> [kmalloc-32] <-> ... <-> [kmalloc-1024] <-> [用户缓存...]
 *        ^                  ^                 ^                        ^
 *        |                  +---------+-------+-------- kmalloc_caches[0..6] 数组里存的就是这 7 个指针
 *        |                            kmalloc_cache(size) 按 size 选下标 i：16<<i >= size 的最小 i
 *        +-- 自举：专门用来分配 struct kmem_cache 本身。它自己是静态变量，
 *            其它缓存（包括 kmalloc-xx）的 struct kmem_cache 都从它里面 kmem_cache_alloc 出来。
 *   用途：只给 slub_print_stats 遍历统计，分配/释放路径不依赖它。
 *
 *
 * 【图 3】一个 kmem_cache 和它的 slab（以 kmalloc-64 为例）
 *
 *   struct kmem_cache                          每个 slab = 一页（4096 字节）
 *   +--------------------------------+
 *   | object_size=64  size=64 (步长)  |        +-----------+
 *   | offset=48  objs_per_slab=63    |  +---->|  slab A   |  cpu_slab：当前活动 slab，
 *   | cpu_slab ----------------------+--+     +-----------+  分配优先从它的 freelist 取，
 *   |                                |           (不在 partial 链上)  即使变空也不归还
 *   | partial  (链表头)  ------------+--+
 *   | nr_partial nr_slabs nr_inuse   |  |     partial 链（链接字段 slab.slab_link）：
 *   | cache_link  (挂 cache_list)    |  |       partial <-> [slab B] <-> [slab C] <-> partial
 *   +--------------------------------+  |       B、C 是“部分空闲”：既有空闲对象，也有在用对象
 *                                       |
 *                                       |     [slab D] 已满：freelist == NULL，
 *                                       +-->  不挂在任何链表上（SLUB 不跟踪满 slab）。
 *                                             只能靠 D 页首的 slab->cache 指回 cache；
 *                                             释放它的一个对象时才重新挂回 partial。
 *
 *
 * 【图 4】一个 slab 页的内部布局（页首 4KiB 对齐，kmalloc-64：header 48 字节，63 个对象）
 *
 *   offset:  0                    48       112      176             4016     4080   4096
 *            +---------------------+--------+--------+--------+-----+--------+------+
 *            | struct slab (header)| obj 0  | obj 1  | obj 2  | ... | obj 62 | 零头 |
 *            | magic inuse cache   | 在用   | 空闲   | 空闲   |     | 空闲   | 16 B |
 *            | freelist npages     |        |        |        |     |        | 不足  |
 *            | slab_link           |        |        |        |     |        | 一个  |
 *            +---------------------+--------+--------+--------+-----+--------+------+
 *   (48 = ROUNDUP(sizeof(struct slab), align)；63 = (4096 - 48) / 64，向下取整)
 *
 *   **freelist 就嵌在空闲对象里，不占额外内存**：空闲对象的前 8 字节存“下一个空闲对象”的地址。
 *     slab->freelist --> obj 1 --> obj 2 --> ... --> obj 62 --> NULL   (obj 0 在用，不在链上)
 *   分配 = 摘链头（freelist 指向下一个），释放 = 把对象插到链头。-> 时间复杂度 O1. 
 *
 *   由对象地址找 slab：addr2slab(obj) = obj 向下取整到页边界（ROUNDDOWN），
 *   因为每个 slab 恰好一页，页首就是 struct slab。kfree 靠 header->magic 区分：
 *     SLAB_MAGIC  -> 属于某个缓存，调 kmem_cache_free(header->cache, ptr)
 *     LARGE_MAGIC -> 大块，整体还给第一层
 *
 *
 * 【图 5】大块（kmalloc 请求 > 1024 字节）：不走缓存，直接按页分配
 *
 *   n = ROUNDUP(size + LARGE_HDR_SIZE, PGSIZE) / PGSIZE，alloc_pages(n) 得到 n 个连续页：
 *   +--------------------------+--------------------------------------------+
 *   | struct slab (header)     | 返回给调用者的区域                         |
 *   | magic = LARGE_MAGIC      | ptr = header + LARGE_HDR_SIZE              |
 *   | npages = n  cache = NULL | 可用大小 ksize = n*PGSIZE - LARGE_HDR_SIZE |
 *   +--------------------------+--------------------------------------------+
 *   <------------------------------- n 个连续页 -------------------------------->
 *   kfree：ptr 向下取整到页得到 header，magic 为 LARGE_MAGIC 就 free_pages(.., npages)。
 *
 *
 * 【图 6】三种“链”不要混淆
 *
 *   名称            串起来的是            链接字段（list_entry_t）      表头
 *   --------------  --------------------  ----------------------------  ------------------
 *   pmm free_list   空闲物理页块的首页    struct Page.page_link         free_area.free_list
 *   cache_list      所有 kmem_cache       struct kmem_cache.cache_link  cache_list
 *   cache->partial  部分空闲的 slab       struct slab.slab_link         cache->partial
 *   slab->freelist  一个 slab 里的空闲对象 对象前 8 字节的裸指针         slab->freelist
 *                   （不是 list_entry_t，是单向链，以 NULL 结尾）
 *   前三条是 list.h 的双向循环链表，用 le2page / le2cache / le2slab 由节点反推所属结构体。
 *
 *
 * 【图 7】一个 slab 的生命周期
 *
 *   alloc_page()                 活动 slab 的 freelist 耗尽
 *   (new_slab)  ------------> cpu_slab  --------------------------> 已满，脱离链表
 *                                ^   ^                                   |
 *                                |   | partial 非空时，从链头取一个       | 释放它的某个对象
 *                                |   |（或 new_slab）接替 cpu_slab        | (was_full)
 *                                |   +----------------------- partial <--+
 *                                |                              |
 *                                |   释放对象时：               | 释放对象后 inuse == 0
 *                                |    - 属于 cpu_slab：只压回 freelist，不做别的
 *                                |    - 其它 slab：inuse==0 则 list_del 后 discard_slab
 *                                |                                  -> free_page 还给第一层
 *                                |    - 其它 slab 且原先已满：挂回 partial
 *                                +-- 活动 slab 即使变空也保留，避免反复申请/释放同一页
 * */
