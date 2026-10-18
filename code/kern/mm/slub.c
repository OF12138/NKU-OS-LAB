#include <slub.h>
#include <pmm.h>
#include <error.h>
#include <stdio.h>
#include <assert.h>

/* *
 * 简化的 SLUB 分配器。
 *
 * 分配路径：kmalloc(size) 先按大小选一个通用缓存，再从该缓存的活动 slab（cpu_slab）的
 * freelist 弹出一个对象；活动 slab 用完时从 partial 链表取一个部分空闲的 slab，没有就向
 * 第一层申请一页新建 slab。大于 SLUB_MAX_SIZE 的请求直接按页分配。
 *
 * 释放路径：对象地址向下对齐到页即得到 slab 头。对象压回该 slab 的 freelist；
 * 满 slab 释放出第一个对象时重新挂回 partial；非活动 slab 变空时把整页还给第一层。
 *
 * 与 Linux SLUB 的对应和简化：
 *   - freelist 嵌在空闲对象里，只维护 partial 链表、不跟踪满 slab，有活动 slab：与 SLUB 一致；
 *   - Linux 把 slab 元数据放在 struct page 里，这里 struct Page 没有空余字段，改放在页首；
 *   - 每个 slab 固定一页，单核所以只有一个活动 slab，没有 per-node 链表和 min_partial。
 * */

#define LARGE_HDR_SIZE  ROUNDUP(sizeof(struct slab), 16)   // 大块分配返回地址相对页首的偏移

static list_entry_t cache_list;                  // 所有缓存，供统计和调试
static struct kmem_cache cache_cache;            // 存放 struct kmem_cache 本身的缓存（自举用，静态分配）
static struct kmem_cache *kmalloc_caches[KMALLOC_NR_CACHES];
static const char *kmalloc_names[KMALLOC_NR_CACHES] = {
    "kmalloc-16", "kmalloc-32", "kmalloc-64", "kmalloc-128",
    "kmalloc-256", "kmalloc-512", "kmalloc-1024",
};

#define le2slab(le) to_struct((le), struct slab, slab_link)
#define le2cache(le) to_struct((le), struct kmem_cache, cache_link)

// pmm.h 中的 KADDR 被注释掉了，这里按固定偏移自己换算
static inline void *page2kva(struct Page *page) {
    return (void *)(page2pa(page) + va_pa_offset);
}

static inline struct Page *kva2page(void *kva) {
    return pa2page(PADDR(kva));
}

static inline struct slab *addr2slab(const void *addr) {
    return (struct slab *)ROUNDDOWN((uintptr_t)addr, PGSIZE);
}

// cache_setup - 计算对象步长、页内偏移和每页对象数，初始化链表
static int cache_setup(struct kmem_cache *cache, const char *name, size_t size, size_t align) {
    if (align < SLUB_MIN_ALIGN) {
        align = SLUB_MIN_ALIGN;
    }
    if (size == 0 || (align & (align - 1)) != 0) {
        return -E_INVAL;
    }
    size_t stride = ROUNDUP(size, align);  // 至少 8 字节，空闲时能放下 next 指针
    size_t offset = ROUNDUP(sizeof(struct slab), align);
    if (offset + stride > PGSIZE) {
        return -E_INVAL;  // 一页放不下一个对象，应当用 kmalloc 的大块路径
    }
    cache->name = name;
    cache->object_size = size;
    cache->size = stride;
    cache->align = align;
    cache->offset = offset;
    cache->objs_per_slab = (PGSIZE - offset) / stride;
    cache->cpu_slab = NULL;
    list_init(&cache->partial);
    cache->nr_partial = 0;
    cache->nr_slabs = 0;
    cache->nr_inuse = 0;
    return 0;
}

// new_slab - 向第一层申请一页，把它切成对象并串成 freelist
static struct slab *new_slab(struct kmem_cache *cache) {
    struct Page *page = alloc_page();
    if (page == NULL) {
        return NULL;
    }
    struct slab *slab = page2kva(page);
    slab->magic = SLAB_MAGIC;
    slab->inuse = 0;
    slab->cache = cache;
    slab->npages = 1;
    list_init(&slab->slab_link);

    char *obj = (char *)slab + cache->offset;
    slab->freelist = obj;
    for (unsigned int i = 0; i + 1 < cache->objs_per_slab; i++, obj += cache->size) {
        *(void **)obj = obj + cache->size;
    }
    *(void **)obj = NULL;

    cache->nr_slabs++;
    return slab;
}

static void discard_slab(struct kmem_cache *cache, struct slab *slab) {
    slab->magic = 0;  // 防止页被重新使用后仍被当成 slab
    free_page(kva2page(slab));
    cache->nr_slabs--;
}

void slub_init(void) {
    list_init(&cache_list);
    int ret = cache_setup(&cache_cache, "kmem_cache", sizeof(struct kmem_cache), SLUB_MIN_ALIGN);
    assert(ret == 0);
    list_add(&cache_list, &cache_cache.cache_link);

    for (int i = 0; i < KMALLOC_NR_CACHES; i++) {
        kmalloc_caches[i] = kmem_cache_create(kmalloc_names[i], 16 << i, SLUB_MIN_ALIGN);
        assert(kmalloc_caches[i] != NULL);
    }
    cprintf("slub: %d kmalloc caches (16 ~ %d bytes), larger requests use whole pages\n",
            KMALLOC_NR_CACHES, SLUB_MAX_SIZE);
}

struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align) {
    struct kmem_cache *cache = kmem_cache_alloc(&cache_cache);
    if (cache == NULL) {
        return NULL;
    }
    if (cache_setup(cache, name, size, align) != 0) {
        kmem_cache_free(&cache_cache, cache);
        return NULL;
    }
    list_add_before(&cache_list, &cache->cache_link);
    return cache;
}

// kmem_cache_destroy - 销毁缓存；还有对象未释放时拒绝销毁
int kmem_cache_destroy(struct kmem_cache *cache) {
    if (cache == NULL || cache == &cache_cache) {
        return -E_INVAL;
    }
    for (int i = 0; i < KMALLOC_NR_CACHES; i++) {
        if (cache == kmalloc_caches[i]) {
            return -E_INVAL;
        }
    }
    if (cache->nr_inuse != 0) {
        return -E_INVAL;
    }
    // 没有在用的对象，就不存在满 slab：剩下的只有活动 slab 和 partial 上的空 slab
    while (!list_empty(&cache->partial)) {
        list_entry_t *le = list_next(&cache->partial);
        list_del(le);
        discard_slab(cache, le2slab(le));
    }
    cache->nr_partial = 0;
    if (cache->cpu_slab != NULL) {
        discard_slab(cache, cache->cpu_slab);
        cache->cpu_slab = NULL;
    }
    assert(cache->nr_slabs == 0);
    list_del(&cache->cache_link);
    kmem_cache_free(&cache_cache, cache);
    return 0;
}

void *kmem_cache_alloc(struct kmem_cache *cache) {
    if (cache == NULL) {
        return NULL;
    }
    struct slab *slab = cache->cpu_slab;
    if (slab == NULL || slab->freelist == NULL) {
        // 活动 slab 已满：它不挂在任何链表上（SLUB 不跟踪满 slab），换一个
        if (!list_empty(&cache->partial)) {
            list_entry_t *le = list_next(&cache->partial);
            list_del(le);
            cache->nr_partial--;
            slab = le2slab(le);
        } else if ((slab = new_slab(cache)) == NULL) {
            return NULL;
        }
        cache->cpu_slab = slab;
    }
    void *obj = slab->freelist;
    slab->freelist = *(void **)obj;
    slab->inuse++;
    cache->nr_inuse++;
    return obj;
}

// check_object - 确认 obj 是该缓存某个 slab 中合法对象的起始地址，且当前不在 freelist 上
static void check_object(struct kmem_cache *cache, struct slab *slab, void *obj) {
    if (slab->magic != SLAB_MAGIC || slab->cache != cache) {
        panic("slub: %p does not belong to cache %s", obj, cache->name);
    }
    uintptr_t first = (uintptr_t)slab + cache->offset;
    uintptr_t addr = (uintptr_t)obj;
    if (addr < first || (addr - first) % cache->size != 0 ||
        (addr - first) / cache->size >= cache->objs_per_slab) {
        panic("slub: %p is not an object boundary in cache %s", obj, cache->name);
    }
    for (void *p = slab->freelist; p != NULL; p = *(void **)p) {
        if (p == obj) {
            panic("slub: double free of %p in cache %s", obj, cache->name);
        }
    }
}

void kmem_cache_free(struct kmem_cache *cache, void *obj) {
    if (cache == NULL || obj == NULL) {
        return;
    }
    struct slab *slab = addr2slab(obj);
    check_object(cache, slab, obj);

    int was_full = (slab->freelist == NULL);
    *(void **)obj = slab->freelist;
    slab->freelist = obj;
    slab->inuse--;
    cache->nr_inuse--;

    if (slab == cache->cpu_slab) {
        return;  // 活动 slab 即使变空也留着，避免反复申请和释放同一页
    }
    if (slab->inuse == 0) {
        if (!was_full) {  // 满 slab 不在链表上；只有一个对象的 slab 会从满直接变空
            list_del(&slab->slab_link);
            cache->nr_partial--;
        }
        discard_slab(cache, slab);
    } else if (was_full) {
        list_add(&cache->partial, &slab->slab_link);
        cache->nr_partial++;
    }
}

struct kmem_cache *kmalloc_cache(size_t size) {
    if (size == 0 || size > SLUB_MAX_SIZE) {
        return NULL;
    }
    int i = 0;
    while ((16UL << i) < size) {
        i++;
    }
    return kmalloc_caches[i];
}

void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    if (size <= SLUB_MAX_SIZE) {
        return kmem_cache_alloc(kmalloc_cache(size));
    }
    if (size > npage * PGSIZE) {  // 防止下面的加法溢出，也不可能分配成功
        return NULL;
    }
    size_t n = ROUNDUP(size + LARGE_HDR_SIZE, PGSIZE) / PGSIZE;
    struct Page *page = alloc_pages(n);
    if (page == NULL) {
        return NULL;
    }
    struct slab *hdr = page2kva(page);
    hdr->magic = LARGE_MAGIC;
    hdr->inuse = 1;
    hdr->cache = NULL;
    hdr->freelist = NULL;
    hdr->npages = n;
    return (char *)hdr + LARGE_HDR_SIZE;
}

void kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    struct slab *hdr = addr2slab(ptr);
    if (hdr->magic == SLAB_MAGIC) {
        kmem_cache_free(hdr->cache, ptr);
    } else if (hdr->magic == LARGE_MAGIC && (char *)ptr == (char *)hdr + LARGE_HDR_SIZE) {
        hdr->magic = 0;
        free_pages(kva2page(hdr), hdr->npages);
    } else {
        panic("kfree: invalid pointer %p", ptr);
    }
}

// ksize - 返回 ptr 实际可用的字节数
size_t ksize(const void *ptr) {
    if (ptr == NULL) {
        return 0;
    }
    struct slab *hdr = addr2slab(ptr);
    if (hdr->magic == SLAB_MAGIC) {
        return hdr->cache->size;
    }
    if (hdr->magic == LARGE_MAGIC) {
        return hdr->npages * PGSIZE - LARGE_HDR_SIZE;
    }
    panic("ksize: invalid pointer %p", ptr);
}

void slub_print_stats(void) {
    cprintf("slub caches:\n");
    for (list_entry_t *le = list_next(&cache_list); le != &cache_list; le = list_next(le)) {
        struct kmem_cache *c = le2cache(le);
        cprintf("  %-14s size %4lu  objs/slab %3u  slabs %3lu  partial %3lu  inuse %5lu\n",
                c->name, c->size, c->objs_per_slab, c->nr_slabs, c->nr_partial, c->nr_inuse);
    }
}
