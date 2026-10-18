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
 * */

#define SLUB_MIN_ALIGN      sizeof(void *)  // 对象至少按指针对齐，空闲时要存下一个指针
#define SLUB_MAX_SIZE       1024            // kmalloc 走 slab 的最大请求大小，更大的请求直接按页分配
#define KMALLOC_NR_CACHES   7               // kmalloc 的通用缓存：16 32 64 128 256 512 1024

#define SLAB_MAGIC          0x51ABu         // slab 页
#define LARGE_MAGIC         0x1A26u         // kmalloc 大块页

// struct slab - 位于每个 slab 页（或大块的首页）开头的管理头
struct slab {
    uint32_t magic;                 // SLAB_MAGIC 或 LARGE_MAGIC，kfree 据此区分两类内存
    uint32_t inuse;                 // 已分配出去的对象数
    struct kmem_cache *cache;       // 所属缓存（大块时为 NULL）
    void *freelist;                 // 本 slab 第一个空闲对象，NULL 表示已满
    size_t npages;                  // 大块占用的页数（slab 页为 1）
    list_entry_t slab_link;         // 挂在 cache->partial 上
};

// struct kmem_cache - 管理一种大小的对象
struct kmem_cache {
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
