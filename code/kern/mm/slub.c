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

// 大块分配时，用户可用区域从页首偏移 LARGE_HDR_SIZE 处开始：先留出一个 struct slab 作头部，
// 再向上取整到 16 的倍数以保证返回地址 16 字节对齐。ROUNDUP 是 libs/defs.h 里的宏。
#define LARGE_HDR_SIZE  ROUNDUP(sizeof(struct slab), 16)   // 大块分配返回地址相对页首的偏移

// 全局变量说明：
//   cache_list      list_entry_t 链表头（哨兵），把所有 kmem_cache 通过 cache_link 串起来
//   cache_cache     一个 struct kmem_cache 变量（不是指针），管理“struct kmem_cache 对象”本身，
//                   它是静态分配的，所以能在没有任何缓存的时候启动，这就是“自举”
//   kmalloc_caches  数组，7 个元素，每个元素是指向 struct kmem_cache 的指针，下标 i 对应对象大小 16<<i
static list_entry_t cache_list;                  // 所有缓存，供统计和调试
static struct kmem_cache cache_cache;            // 存放 struct kmem_cache 本身的缓存（自举用，静态分配）
static struct kmem_cache *kmalloc_caches[KMALLOC_NR_CACHES];
// 字符串数组：const char * 表示指向只读字符的指针，数组每个元素指向一个字符串常量，
// 下标与 kmalloc_caches 一一对应。静态存储，所以 cache->name 直接保存这个指针即可，不用拷贝
static const char *kmalloc_names[KMALLOC_NR_CACHES] = 
{
    "kmalloc-16", "kmalloc-32", "kmalloc-64", "kmalloc-128",
    "kmalloc-256", "kmalloc-512", "kmalloc-1024",
};

// 由链表节点指针反推所在结构体（to_struct 来自 libs/defs.h，等同 Linux 的 container_of）：
//   结构体首地址 = 节点地址 - 节点成员在结构体内的偏移（offsetof）
// le2slab(le)  : le 是某个 struct slab 的 slab_link 成员的地址，返回该 struct slab 的地址
// le2cache(le) : le 是某个 struct kmem_cache 的 cache_link 成员的地址，返回该 kmem_cache 的地址
#define le2slab(le) to_struct((le), struct slab, slab_link)
#define le2cache(le) to_struct((le), struct kmem_cache, cache_link)

// 三个地址换算小函数：
//   page2kva  struct Page* -> 该页起始的内核虚拟地址（VA）：page2pa 得到物理地址，再加固定偏移
//   kva2page  内核虚拟地址 -> 所在页的 struct Page*：PADDR 减偏移得物理地址，pa2page 找描述符
//   addr2slab 任意地址 -> 它所在页的页首，页首就是 struct slab（强制转换成 struct slab *）
// 其中 (uintptr_t)addr 把指针转成整数才能做位运算/取整，(struct slab *) 再把整数转回指针；
// 参数写成 const void * 表示“只读不改”，void * 是万能指针，可以接受任何类型的地址
// pmm.h 中的 KADDR 被注释掉了，这里按固定偏移自己换算
static inline void *page2kva(struct Page *page) 
{
    return (void *)(page2pa(page) + va_pa_offset);
}

static inline struct Page *kva2page(void *kva) {
    return pa2page(PADDR(kva));
}

static inline struct slab *addr2slab(const void *addr) {
    return (struct slab *)ROUNDDOWN((uintptr_t)addr, PGSIZE);
}

// cache_setup - 计算对象步长、页内偏移和每页对象数，初始化链表
static int cache_setup(struct kmem_cache *cache, const char *name, size_t size, size_t align) 
{
    // align 至少为指针大小（8 字节）：对象空闲时它的前 8 字节要用来存 next 指针
    if (align < SLUB_MIN_ALIGN) align = SLUB_MIN_ALIGN;
    // (align & (align - 1)) == 0 是判断 align 是不是 2 的幂的经典写法：
    //   2 的幂只有一位是 1，如 8 = 1000b，减 1 得 0111b，按位与为 0；
    //   非 2 的幂，如 12 = 1100b，减 1 得 1011b，按位与 = 1000b，不为 0
    // 对齐值必须是 2 的幂，否则不合法，返回 -E_INVAL
    // lab2 2414099：先限定单页缓存的输入范围，避免 ROUNDUP 或 offset + stride 溢出。
    if (size == 0 || size > PGSIZE || align >= PGSIZE ||
        (align & (align - 1)) != 0) return -E_INVAL;
    // stride（步长）= 每个对象实际占的字节数：用户请求的 size 向上取整到 align 的整数倍
    // offset = 第一个对象的页内偏移：跳过页首的 struct slab 头，同样向上取整到 align，保证第一个对象对齐
    size_t stride = ROUNDUP(size, align);  // 至少 8 字节，空闲时能放下 next 指针
    size_t offset = ROUNDUP(sizeof(struct slab), align);
    if (offset + stride > PGSIZE)  return -E_INVAL;  // 一页放不下一个对象，应当用 kmalloc 的大块路径
    
    cache->name = name;
    cache->object_size = size;
    cache->size = stride;
    cache->align = align;
    cache->offset = offset;
    // 一页能放几个对象：除去页首头部后的剩余字节数 / 步长，整数除法自动向下取整，
    // 末尾放不下一个对象的零头浪费掉
    cache->objs_per_slab = (PGSIZE - offset) / stride;
    cache->cpu_slab = NULL;
    list_init(&cache->partial);
    cache->nr_partial = 0;
    cache->nr_slabs = 0;
    cache->nr_inuse = 0;
    return 0;
}

// new_slab - 向第一层申请一页，把它切成对象并串成 freelist
static struct slab *new_slab(struct kmem_cache *cache) 
{
    struct Page *page = alloc_page();
    if (page == NULL) return NULL;
    
    // page2kva 返回 void *。C 语言里 void * 可以直接赋给任意类型的指针，不用强制转换。
    // 这里把这一页的起始地址当作 struct slab 的地址，即在页首“就地”建立 slab 头
    struct slab *slab = page2kva(page);
    slab->magic = SLAB_MAGIC;
    slab->inuse = 0;
    slab->cache = cache;
    slab->npages = 1;
    list_init(&slab->slab_link);

    // ---- 把这一页切成等长的对象，并把它们串成单向链表（freelist） ----
    //
    // 先说几个指针类型：
    //   void *      “万能指针”，只记录一个地址，不知道指向的数据是什么类型，
    //               所以不能直接解引用（*p），也不能按元素做 p + 1
    //   char *      指向 1 字节的指针，p + n 就是地址加 n 个字节，这里用来按字节计算地址
    //   void **     指向“一个 void * 变量”的指针，即：这个地址处存放着的是另一个地址
    //   *(void **)obj
    //               把 obj 当作 void ** 看待（强制转换 (void **)obj），再解引用 *，
    //               得到“位于 obj 这个地址处的一个 void * 变量”，即 obj 开头的 8 字节
    //               当它在赋值号左边，就是往 obj 的前 8 字节写入一个地址；
    //               当它在右边，就是从 obj 的前 8 字节读出一个地址
    //
    // 为什么要这么写：空闲对象里本来没有任何有用数据，SLUB 就借它的前 8 字节存
    // “下一个空闲对象的地址”，不需要额外的链表节点（所以对象大小至少 8 字节）：
    //
    //   obj ---> +-----------------+
    //            | next (8 字节)   |-----> 下一个空闲对象的地址（也就是 obj + cache->size）
    //            | ...其余字节未用 |
    //            +-----------------+
    //
    // 页内对象 0 .. n-1（n = objs_per_slab）连成：obj0 -> obj1 -> ... -> obj(n-1) -> NULL
    // 变量 obj 是 char *，因此 obj + cache->size 正好是“向后移动 size 个字节”，即下一个对象。
    //
    // 循环做了 n-1 次：第 i 次让对象 i 的前 8 字节指向对象 i+1；for 的第三部分用了逗号运算符，
    // 每轮结束同时执行 i++ 和 obj += cache->size（obj 向前移一个对象）。
    // 循环结束时 obj 恰好指向最后一个对象 n-1，它没有后继，所以单独把它的 next 置为 NULL。
    // slab->freelist 保存链头 obj0，因此刚建好时整页都是空闲对象。
    char *obj = (char *)slab + cache->offset;
    slab->freelist = obj;
    for (unsigned int i = 0; i + 1 < cache->objs_per_slab; i++, obj += cache->size) 
    {
        *(void **)obj = obj + cache->size; //写地址，串出freelist（obj)
    }
    *(void **)obj = NULL;

    cache->nr_slabs++;
    return slab;
}

// discard_slab - 把一个空 slab 整页还给第一层（free_page），并更新缓存的 slab 计数
static void discard_slab(struct kmem_cache *cache, struct slab *slab) 
{
    slab->magic = 0;  // 防止页被重新使用后仍被当成 slab
    free_page(kva2page(slab));
    cache->nr_slabs--;
}

// slub_init - 初始化 SLUB：建好 cache_list、自举缓存 cache_cache 和 7 个 kmalloc 通用缓存
void slub_init(void) 
{
    list_init(&cache_list);
    int ret = cache_setup(&cache_cache, "kmem_cache", sizeof(struct kmem_cache), SLUB_MIN_ALIGN); //cache_cache 自举
    assert(ret == 0);
    list_add(&cache_list, &cache_cache.cache_link); //cache_list -> cache_cache -> other cache list

    for (int i = 0; i < KMALLOC_NR_CACHES; i++) 
    {
        // 16 << i 是对象大小：左移 1 位等于乘 2，所以 i = 0..6 依次得到 16, 32, 64, 128, 256, 512, 1024
        kmalloc_caches[i] = kmem_cache_create(kmalloc_names[i], 16 << i, SLUB_MIN_ALIGN);
        assert(kmalloc_caches[i] != NULL);
    }
    cprintf("slub: %d kmalloc caches (16 ~ %d bytes), larger requests use whole pages\n", KMALLOC_NR_CACHES, SLUB_MAX_SIZE);
}

// kmem_cache_create - 创建一个管理固定大小对象的缓存：从 cache_cache 取一个 struct kmem_cache，配置后挂入 cache_list
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align) 
{
    struct kmem_cache *cache = kmem_cache_alloc(&cache_cache);
    if (cache == NULL) return NULL;
    
    if (cache_setup(cache, name, size, align) != 0) 
    {
        kmem_cache_free(&cache_cache, cache);
        return NULL;
    }
    // cache_list 是循环链表的头。list_add_before(头, 新节点) 把新节点插在“头的前面”，
    // 而头的前面就是链表末尾，所以新缓存追加到链表尾，遍历时按创建顺序出现
    list_add_before(&cache_list, &cache->cache_link);
    return cache;
}

// kmem_cache_destroy - 销毁缓存；还有对象未释放时拒绝销毁
int kmem_cache_destroy(struct kmem_cache *cache) 
{
    if (cache == NULL || cache == &cache_cache)   return -E_INVAL;
    for (int i = 0; i < KMALLOC_NR_CACHES; i++) if (cache == kmalloc_caches[i]) return -E_INVAL;
    if (cache->nr_inuse != 0) return -E_INVAL;
    
    // 没有在用的对象，就不存在满 slab：剩下的只有活动 slab 和 partial 上的空 slab
    // 逐个取下 partial 链上的 slab 并归还给第一层。list_next(&cache->partial) 就是链表第一个节点，
    // le2slab 把节点换算成它所在的 struct slab
    while (!list_empty(&cache->partial)) 
    {
        list_entry_t *le = list_next(&cache->partial);
        list_del(le);
        discard_slab(cache, le2slab(le));
    }
    cache->nr_partial = 0;
    if (cache->cpu_slab != NULL) 
    {
        discard_slab(cache, cache->cpu_slab);
        cache->cpu_slab = NULL;
    }
    assert(cache->nr_slabs == 0);
    list_del(&cache->cache_link);
    kmem_cache_free(&cache_cache, cache);
    return 0;
}

// kmem_cache_alloc - 从缓存取一个对象：优先用活动 slab，用完换 partial 上的 slab，再没有就新建 slab
void *kmem_cache_alloc(struct kmem_cache *cache) 
{
    if (cache == NULL)  return NULL;
    
    struct slab *slab = cache->cpu_slab;
    if (slab == NULL || slab->freelist == NULL) 
    {
        // 活动 slab 已满：它不挂在任何链表上（SLUB 不跟踪满 slab），换一个
        if (!list_empty(&cache->partial)) 
        {
            list_entry_t *le = list_next(&cache->partial);
            list_del(le);
            cache->nr_partial--;
            slab = le2slab(le);
        // 在条件里赋值：先执行 slab = new_slab(cache)，赋值表达式的值就是被赋的值，
        // 再与 NULL 比较。写成两步等价：slab = new_slab(cache); if (slab == NULL) {...}
        } 
        else if ((slab = new_slab(cache)) == NULL) return NULL;
        
        cache->cpu_slab = slab;
    }
    // 从活动 slab 的 freelist 弹出链头（单向链表的“出栈”）：
    //   obj 就是链头对象，要返回给调用者；
    //   *(void **)obj 读出 obj 前 8 字节里存的“下一个空闲对象的地址”，作为新的链头。
    // 返回的对象前 8 字节里留着旧的 next 指针，不会被清零，调用者应当自己初始化
    void *obj = slab->freelist;
    slab->freelist = *(void **)obj;
    slab->inuse++;
    cache->nr_inuse++;
    return obj;
}

// check_object - 确认 obj 是该缓存某个 slab 中合法对象的起始地址，且当前不在 freelist 上
static void check_object(struct kmem_cache *cache, struct slab *slab, void *obj) 
{
    if (slab->magic != SLAB_MAGIC || slab->cache != cache) {
        panic("slub: %p does not belong to cache %s", obj, cache->name);
    }
    // 合法性检查：把指针转成整数 uintptr_t 后才能做减法、取模。
    //   first      本 slab 第一个对象的地址
    //   (addr - first) % size == 0       obj 必须落在某个对象的起点，不能指向对象中间
    //   (addr - first) / size < n        对象编号不能超出本 slab 的对象数
    uintptr_t first = (uintptr_t)slab + cache->offset;
    uintptr_t addr = (uintptr_t)obj;
    if (addr < first || (addr - first) % cache->size != 0 ||
        (addr - first) / cache->size >= cache->objs_per_slab) {
        panic("slub: %p is not an object boundary in cache %s", obj, cache->name);
    }
    // 沿着 freelist 逐个检查：如果 obj 已经在空闲链上，说明被释放了两次（double free）。
    // for 的第三部分 p = *(void **)p 就是“移动到下一个空闲对象”：读出 p 指向的对象前 8 字节里存的下一个地址
    for (void *p = slab->freelist; p != NULL; p = *(void **)p) {
        if (p == obj) {
            panic("slub: double free of %p in cache %s", obj, cache->name);
        }
    }
}

// kmem_cache_free - 把对象还给它所在的 slab，并按 slab 的新状态调整 partial 链或归还整页
void kmem_cache_free(struct kmem_cache *cache, void *obj) 
{
    if (cache == NULL || obj == NULL) return;
    
    struct slab *slab = addr2slab(obj);
    check_object(cache, slab, obj);

    int was_full = (slab->freelist == NULL);
    // 把 obj 压回空闲链表（“入栈”）：先把原来的链头地址写进 obj 前 8 字节，
    // 让 obj 指向旧链头；再令 slab->freelist = obj，obj 成为新链头。
    // 上一行的 was_full 记录释放前链是否为空（freelist == NULL 表示这个 slab 原先已满）
    *(void **)obj = slab->freelist;
    slab->freelist = obj;
    slab->inuse--;
    cache->nr_inuse--;

    if (slab == cache->cpu_slab) {
        return;  // 活动 slab 即使变空也留着，避免反复申请和释放同一页
    }
    if (slab->inuse == 0) 
    {
        if (!was_full) {  // 满 slab 不在链表上；只有一个对象的 slab 会从满直接变空
            list_del(&slab->slab_link);
            cache->nr_partial--;
        }
        discard_slab(cache, slab);
    } 
    else if (was_full) 
    {
        list_add(&cache->partial, &slab->slab_link);
        cache->nr_partial++;
    }
}

// kmalloc_cache - 按请求大小选 kmalloc 通用缓存，size 为 0 或超过 SLUB_MAX_SIZE 时返回 NULL
struct kmem_cache *kmalloc_cache(size_t size) 
{
    if (size == 0 || size > SLUB_MAX_SIZE) return NULL;
    
    int i = 0;
    // 选缓存：找最小的 i 使 16<<i >= size，即把 size 向上取整到 2 的幂（16, 32, ..., 1024）。
    // 16UL 表示 unsigned long 常量，保证移位在 64 位下进行，避免 int 溢出
    while ((16UL << i) < size)  i++;
    
    return kmalloc_caches[i];
}

// kmalloc - 通用分配：小请求走通用缓存，大请求直接按页分配并在页首放大块头
void *kmalloc(size_t size) 
{
    if (size == 0)  return NULL;
    if (size <= SLUB_MAX_SIZE) return kmem_cache_alloc(kmalloc_cache(size));
    if (size > npage * PGSIZE) 
    {  // 防止下面的加法溢出，也不可能分配成功
        return NULL;
    }
    // 大块：需要容纳头部 + 用户数据，向上取整到页数 n（向上取整除法）
    size_t n = ROUNDUP(size + LARGE_HDR_SIZE, PGSIZE) / PGSIZE;
    struct Page *page = alloc_pages(n);
    if (page == NULL) return NULL;
    
    struct slab *hdr = page2kva(page);
    hdr->magic = LARGE_MAGIC;
    hdr->inuse = 1;
    hdr->cache = NULL;
    hdr->freelist = NULL;
    hdr->npages = n;
    // 先转成 char * 再加偏移，才是“加 LARGE_HDR_SIZE 个字节”（struct slab * 加 1 会跳过整个结构体）；
    // 返回值的类型是 void *，char * 会自动转换
    return (char *)hdr + LARGE_HDR_SIZE;
}

// kfree - 通用释放：靠页首 magic 判断是 slab 对象还是大块，分别还给缓存或第一层
void kfree(void *ptr) 
{
    if (ptr == NULL) return;
    
    struct slab *hdr = addr2slab(ptr);
    if (hdr->magic == SLAB_MAGIC) 
    {
        kmem_cache_free(hdr->cache, ptr);
    // 大块的合法用户指针必须正好等于 hdr + LARGE_HDR_SIZE，否则说明传入的是中间地址，按非法处理。
    // 两边都转成 char *，比较的是字节地址（hdr 是 struct slab *，直接加会按结构体大小移动）
    } 
    else if (hdr->magic == LARGE_MAGIC && (char *)ptr == (char *)hdr + LARGE_HDR_SIZE) 
    {
        hdr->magic = 0;
        free_pages(kva2page(hdr), hdr->npages);
    } 
    else panic("kfree: invalid pointer %p", ptr);
    
}

// ksize - 返回 ptr 实际可用的字节数
size_t ksize(const void *ptr) 
{
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

// slub_print_stats - 遍历 cache_list，打印每个缓存的 slab 数、partial 数和在用对象数
void slub_print_stats(void) 
{
    cprintf("slub caches:\n");
    // 遍历 cache_list：从头的下一个节点开始，一直走到回到头为止（循环链表）。
    // 每个节点用 le2cache 换算回所在的 struct kmem_cache
    for (list_entry_t *le = list_next(&cache_list); le != &cache_list; le = list_next(le)) 
    {
        struct kmem_cache *c = le2cache(le);
        // 格式串：%-14s 字符串左对齐占 14 列；%4lu 无符号 long 右对齐占 4 列；%3u 是 unsigned int。
        // size_t 在 64 位 RISC-V 上是 unsigned long，所以用 %lu
        cprintf("  %-14s size %4lu  objs/slab %3u  slabs %3lu  partial %3lu  inuse %5lu\n",
                c->name, c->size, c->objs_per_slab, c->nr_slabs, c->nr_partial, c->nr_inuse);
    }
}
