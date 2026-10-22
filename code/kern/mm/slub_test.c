#include <slub.h>
#include <pmm.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* lab2 2414099：按缓存分配、跨页扩展、释放复用、大对象回退依次自测。 */
static void *objects[256];  // 批量指针放 BSS，避免占用内核栈

// 对象申请的 size 个字节都应保持写入时的值；逐字节检查可发现中间位置被覆盖。
static void check_data(const void *ptr, size_t size, unsigned char value) {
    for (size_t i = 0; i < size; i++) {
        assert(((const unsigned char *)ptr)[i] == value);
    }
}

// 检查一个 cache 从填满、扩展到释放的过程，以及各阶段的 slab 状态。
static void check_cache(size_t request, size_t expected) {
    // request 应映射到对象大小为 expected 的通用 cache。
    struct kmem_cache *cache = kmalloc_cache(request);
    assert(cache != NULL && cache->size == expected);
    size_t n = cache->objs_per_slab;
    assert(n > 1 && n < sizeof(objects) / sizeof(objects[0]));

    // 连续申请 n 个对象后，第一个 slab 应恰好填满，且每个对象都能独立保存数据。
    for (size_t i = 0; i < n; i++) {
        objects[i] = kmalloc(request);
        assert(objects[i] != NULL && (uintptr_t)objects[i] % SLUB_MIN_ALIGN == 0); // 返回地址应满足最小对齐要求
        assert(ksize(objects[i]) == expected); // 实际容量应等于所属 cache 的对象大小
        assert(ROUNDDOWN((uintptr_t)objects[i], PGSIZE) ==
               ROUNDDOWN((uintptr_t)objects[0], PGSIZE)); // 前 n 个对象应位于同一个 slab 页
        memset(objects[i], i + 1, request); // 每个对象写入不同的值，便于检查数据是否互相覆盖
    }
    assert(cache->nr_slabs == 1 && cache->nr_inuse == n && cache->nr_partial == 0); // 填满后应有一个 slab、n 个已分配对象，且没有 partial slab
    // 满页已无空位，再申请一个对象应新建第二个 slab，已分配对象数变为 n + 1。
    void *extra = kmalloc(request);
    assert(extra != NULL && cache->nr_slabs == 2 && cache->nr_inuse == n + 1);
    assert(ROUNDDOWN((uintptr_t)extra, PGSIZE) !=
           ROUNDDOWN((uintptr_t)objects[0], PGSIZE));
    memset(extra, 0xA5, request);
    cprintf("[SLUB] request=%lu cache=%lu capacity=%lu: filled, extra slab allocated\n",
            request, expected, n);

    // 第二个 slab 的分配不应改写第一页中已经分配的对象。
    for (size_t i = 0; i < n; i++) check_data(objects[i], request, i + 1);
    kfree(objects[0]);
    // 满页释放一个对象后，状态应当变成 partial，已分配对象总数也应减少一个，变成 n。
    assert(cache->nr_partial == 1 && cache->nr_inuse == n);
    // 释放第一页剩余的对象后，该空页应被回收，只留下第二个 slab。
    for (size_t i = 1; i < n; i++) kfree(objects[i]);
    assert(cache->nr_slabs == 1 && cache->nr_partial == 0);
    // 第一页回收不应影响第二页的数据；释放 extra 后，应留下一个空活动 slab。
    check_data(extra, request, 0xA5);
    kfree(extra);
    assert(cache->nr_inuse == 0 && cache->nr_slabs == 1);
    cprintf("[SLUB] cache=%lu: partial transition and empty slab reclaim PASS\n", expected);
}

// 分配 128 个 64 字节对象后释放一半，再申请一半，检查原有 slab 的空位能否复用。
// 复用以 slab 数和空闲页数不增加为准，不要求返回与之前相同的指针。
static void check_reuse(void) {
    // 分配 128 个 64B 对象
    const size_t count = 128;
    struct kmem_cache *cache = kmalloc_cache(64);
    size_t before = nr_free_pages();
    for (size_t i = 0; i < count; i++) {
        objects[i] = kmalloc(64);
        assert(objects[i] != NULL);
        memset(objects[i], i + 1, 64);
    }
    size_t slabs = cache->nr_slabs, free_pages = nr_free_pages();
    assert(slabs > 1 && cache->nr_inuse == count);
    // 释放偶数下标的 64 个对象后，应剩下 64 个已分配对象，并出现 partial slab。
    for (size_t i = 0; i < count; i += 2) kfree(objects[i]);
    assert(cache->nr_inuse == count / 2 && cache->nr_partial > 0);
    for (size_t i = 0; i < count; i += 2) {
        objects[i] = kmalloc(64);
        assert(objects[i] != NULL);
        memset(objects[i], i + 1, 64);
    }
    // 再申请 64 个对象后，已分配数应恢复到 128；slab 数和空闲页数不变，说明复用了旧页。
    assert(cache->nr_inuse == count && cache->nr_slabs == slabs);
    assert(nr_free_pages() == free_pages);  // 复用时不应额外占用物理页
    // 新申请的对象和一直存活的对象都应保留各自写入的数据。
    for (size_t i = 0; i < count; i++) check_data(objects[i], 64, i + 1);
    // 全部释放后，应只保留一个空活动 slab，空闲页数恢复到测试前。
    for (size_t i = 0; i < count; i++) kfree(objects[i]);
    assert(cache->nr_inuse == 0 && cache->nr_slabs == 1 && cache->nr_partial == 0);
    assert(nr_free_pages() == before);
    cprintf("[SLUB] 128 x 64B: free half, reuse, data check and reclaim PASS\n");
}

// 检查大对象的整页分配、可用容量、数据读写和页面回收。
static void check_large(void) {
    // 1025 刚超过 1024 字节上限；4096、8192 还检查请求数据跨页时的情况。
    const size_t sizes[] = {1025, 4096, 8192};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        // 记下本次申请前的空闲页数，稍后判断大块占用的页是否全部归还。
        size_t before = nr_free_pages();
        void *p = kmalloc(sizes[i]);
        // 超过 1024 字节时没有对应的通用 cache；p 应指向整页分配区域中头部之后的可用空间。
        assert(p != NULL && kmalloc_cache(sizes[i]) == NULL);
        // ksize 统计扣除大块头部后的可用容量，至少要装下请求数据；返回地址还应 16 字节对齐。
        assert(ksize(p) >= sizes[i] && (uintptr_t)p % 16 == 0);
        // 写满请求的每个字节再逐字节验读，检查跨页位置也能正常访问、数据没有被改写。
        memset(p, 0x5A, sizes[i]);
        check_data(p, sizes[i], 0x5A);
        // kfree 应根据大块头部记录的页数释放整块；空闲页数应回到申请前。
        kfree(p);
        assert(nr_free_pages() == before);
        cprintf("[SLUB] large %luB: page fallback and reclaim PASS\n", sizes[i]);
    }
}
// 内核在 slub_init() 后调用此入口：先检查无效参数，再依次检查七档 cache、复用和大对象。
void slub_check(void) {
    const size_t requests[] = {1, 17, 33, 65, 129, 257, 1024};

    // 记录初始空闲物理页数量
    size_t before = nr_free_pages();

    // 检查异常参数
    assert(kmalloc(0) == NULL);
    kfree(NULL);
    // 尺寸溢出时应拒绝创建 cache；这是前次测试发现的问题。
    assert(kmem_cache_create("overflow", (size_t)-1, 8) == NULL);
    cprintf("[SLUB] basic argument checks PASS\n");

    // 依次检查七档通用 cache 的分配、扩展、释放和数据完整性。
    for (size_t i = 0; i < KMALLOC_NR_CACHES; i++) {
        check_cache(requests[i], 16UL << i);
    }
    // 检查释放的对象能否复用
    check_reuse();
    // 检查大对象的整页分配
    check_large();
    // 测试结束后，七个通用 cache 应各保留一个空活动页，其余页面均应归还。
    assert(nr_free_pages() == before - KMALLOC_NR_CACHES);
    slub_print_stats();
    cprintf("[SLUB] all checks passed (retained %d active slab pages)\n", KMALLOC_NR_CACHES);
}
