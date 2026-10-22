#include <slub.h>
#include <pmm.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* lab2 2414099：按缓存分配、跨页扩展、释放复用、大对象回退依次自测。 */
static void *objects[256];  // 批量指针放 BSS，避免占用内核栈

static void check_data(const void *ptr, size_t size, unsigned char value) {
    for (size_t i = 0; i < size; i++) {
        assert(((const unsigned char *)ptr)[i] == value);
    }
}

static void check_cache(size_t request, size_t expected) {
    struct kmem_cache *cache = kmalloc_cache(request);
    assert(cache != NULL && cache->size == expected);
    size_t n = cache->objs_per_slab;
    assert(n > 1 && n < sizeof(objects) / sizeof(objects[0]));
    for (size_t i = 0; i < n; i++) {
        objects[i] = kmalloc(request);
        assert(objects[i] != NULL && (uintptr_t)objects[i] % SLUB_MIN_ALIGN == 0);
        assert(ksize(objects[i]) == expected);
        assert(ROUNDDOWN((uintptr_t)objects[i], PGSIZE) ==
               ROUNDDOWN((uintptr_t)objects[0], PGSIZE));
        memset(objects[i], i + 1, request);
    }
    assert(cache->nr_slabs == 1 && cache->nr_inuse == n && cache->nr_partial == 0);
    void *extra = kmalloc(request);
    assert(extra != NULL && cache->nr_slabs == 2 && cache->nr_inuse == n + 1);
    assert(ROUNDDOWN((uintptr_t)extra, PGSIZE) !=
           ROUNDDOWN((uintptr_t)objects[0], PGSIZE));
    memset(extra, 0xA5, request);
    cprintf("[SLUB] request=%lu cache=%lu capacity=%lu: filled, extra slab allocated\n",
            request, expected, n);

    for (size_t i = 0; i < n; i++) check_data(objects[i], request, i + 1);
    kfree(objects[0]);
    assert(cache->nr_partial == 1 && cache->nr_inuse == n);
    for (size_t i = 1; i < n; i++) kfree(objects[i]);
    assert(cache->nr_slabs == 1 && cache->nr_partial == 0);
    check_data(extra, request, 0xA5);
    kfree(extra);
    assert(cache->nr_inuse == 0 && cache->nr_slabs == 1);
    cprintf("[SLUB] cache=%lu: partial transition and empty slab reclaim PASS\n", expected);
}

static void check_reuse(void) {
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
    for (size_t i = 0; i < count; i += 2) kfree(objects[i]);
    assert(cache->nr_inuse == count / 2 && cache->nr_partial > 0);
    for (size_t i = 0; i < count; i += 2) {
        objects[i] = kmalloc(64);
        assert(objects[i] != NULL);
        memset(objects[i], i + 1, 64);
    }
    assert(cache->nr_inuse == count && cache->nr_slabs == slabs);
    assert(nr_free_pages() == free_pages);  // 复用已有 slab，没有申请新页
    for (size_t i = 0; i < count; i++) check_data(objects[i], 64, i + 1);
    for (size_t i = 0; i < count; i++) kfree(objects[i]);
    assert(cache->nr_inuse == 0 && cache->nr_slabs == 1 && cache->nr_partial == 0);
    assert(nr_free_pages() == before);
    cprintf("[SLUB] 128 x 64B: free half, reuse, data check and reclaim PASS\n");
}

static void check_large(void) {
    const size_t sizes[] = {1025, 4096, 8192};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        size_t before = nr_free_pages();
        void *p = kmalloc(sizes[i]);
        assert(p != NULL && kmalloc_cache(sizes[i]) == NULL);
        assert(ksize(p) >= sizes[i] && (uintptr_t)p % 16 == 0);
        memset(p, 0x5A, sizes[i]);
        check_data(p, sizes[i], 0x5A);
        kfree(p);
        assert(nr_free_pages() == before);
        cprintf("[SLUB] large %luB: page fallback and reclaim PASS\n", sizes[i]);
    }
}

void slub_check(void) {
    const size_t requests[] = {1, 17, 33, 65, 129, 257, 1024};
    size_t before = nr_free_pages();
    assert(kmalloc(0) == NULL);
    kfree(NULL);
    // 保留前次测试发现的尺寸溢出回归。
    assert(kmem_cache_create("overflow", (size_t)-1, 8) == NULL);
    cprintf("[SLUB] basic argument checks PASS\n");
    for (size_t i = 0; i < KMALLOC_NR_CACHES; i++) {
        check_cache(requests[i], 16UL << i);
    }
    check_reuse();
    check_large();
    // 七个通用缓存各保留一个空活动页，其余测试申请的页均应归还。
    assert(nr_free_pages() == before - KMALLOC_NR_CACHES);
    slub_print_stats();
    cprintf("[SLUB] all checks passed (retained %d active slab pages)\n", KMALLOC_NR_CACHES);
}
