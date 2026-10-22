#include <slub.h>
#include <pmm.h>
#include <assert.h>
#include <error.h>
#include <stdio.h>
#include <string.h>

/* lab2 2414099：独立测试；只通过公开接口操作分配器。 */
#ifndef SLUB_PANIC_TEST
#define SLUB_PANIC_TEST 0
#endif

// 指针表放在 BSS，避免批量测试占满内核栈；正常用例返回前释放全部对象。
static void *objects[256];

static void fill(void *ptr, size_t size, unsigned int seed) {
    assert(ptr != NULL);
    for (size_t i = 0; i < size; i++) {
        ((unsigned char *)ptr)[i] = (seed + 17 * i) % 251;
    }
}

static void verify(const void *ptr, size_t size, unsigned int seed) {
    assert(ptr != NULL);
    for (size_t i = 0; i < size; i++) {
        assert(((const unsigned char *)ptr)[i] == (seed + 17 * i) % 251);
    }
}

static void check_invalid(void) {
    size_t before = nr_free_pages();
    assert(kmalloc(0) == NULL && kmalloc_cache(0) == NULL);
    assert(kmalloc_cache(SLUB_MAX_SIZE + 1) == NULL);
    assert(kmalloc((size_t)-1) == NULL);
    assert(kmalloc(npage * PGSIZE + 1) == NULL);
    assert(ksize(NULL) == 0 && kmem_cache_alloc(NULL) == NULL);
    kfree(NULL);
    kmem_cache_free(NULL, NULL);
    assert(kmem_cache_destroy(NULL) == -E_INVAL);
    for (size_t size = 16; size <= SLUB_MAX_SIZE; size *= 2) {
        struct kmem_cache *cache = kmalloc_cache(size);
        assert(kmem_cache_destroy(cache) == -E_INVAL);
        kmem_cache_free(cache, NULL);
    }
    assert(kmem_cache_create("zero", 0, 8) == NULL);
    assert(kmem_cache_create("bad-align", 32, 12) == NULL);
    assert(kmem_cache_create("too-big", PGSIZE, 8) == NULL);
    assert(kmem_cache_create("overflow", (size_t)-1, 8) == NULL);
    assert(kmem_cache_create("overflow-add", (size_t)-15, 16) == NULL);
    assert(kmem_cache_create("huge-align", 1, (size_t)1 << 63) == NULL);
    assert(nr_free_pages() == before);
    cprintf("[SLUB] invalid arguments and overflow: PASS\n");
}

static void check_sizes(void) {
    const size_t sizes[] = {1, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 511, 512, 513, 1023, 1024};
    size_t count = sizeof(sizes) / sizeof(sizes[0]);
    for (size_t i = 0; i < count; i++) {
        objects[i] = kmalloc(sizes[i]);
        assert(objects[i] != NULL);
        size_t expected = 16;
        while (expected < sizes[i]) expected *= 2;
        assert(ksize(objects[i]) == expected);
        assert(kmalloc_cache(sizes[i])->size == expected);
        assert((uintptr_t)objects[i] % SLUB_MIN_ALIGN == 0);
        for (size_t j = 0; j < i; j++) assert(objects[i] != objects[j]);
        fill(objects[i], expected, i);
    }
    // 先释放交错对象，保留其余对象；重新写满整个可用区，检查相互覆盖。
    for (size_t i = 0; i < count; i += 2) {
        verify(objects[i], ksize(objects[i]), i);
        kfree(objects[i]);
        objects[i] = kmalloc(sizes[i]);
        fill(objects[i], ksize(objects[i]), i);
    }
    for (size_t i = count; i-- > 0;) {
        verify(objects[i], ksize(objects[i]), i);
        kfree(objects[i]);
    }
    // 所有合法小尺寸均走一遍，不仅抽取缓存边界。
    for (size_t size = 1; size <= SLUB_MAX_SIZE; size++) {
        void *p = kmalloc(size);
        assert(p != NULL && ksize(p) >= size);
        fill(p, ksize(p), size);
        verify(p, ksize(p), size);
        kfree(p);
    }
    for (size_t size = 16; size <= SLUB_MAX_SIZE; size *= 2) {
        struct kmem_cache *cache = kmalloc_cache(size);
        assert(cache->nr_inuse == 0 && cache->nr_partial == 0);
        assert(cache->nr_slabs == 1 && cache->cpu_slab->inuse == 0);
    }
    cprintf("[SLUB] sizes 1..1024 and live-object isolation: PASS\n");
}

static void check_partial(void) {
    size_t before = nr_free_pages();
    struct kmem_cache *cache = kmem_cache_create("test-partial", 64, 8);
    assert(cache != NULL);
    size_t n = cache->objs_per_slab;
    assert(n >= 2 && 2 * n <= sizeof(objects) / sizeof(objects[0]));
    for (size_t i = 0; i < 2 * n; i++) {
        objects[i] = kmem_cache_alloc(cache);
        fill(objects[i], 64, i);
    }
    struct slab *first = (void *)ROUNDDOWN((uintptr_t)objects[0], PGSIZE);
    struct slab *second = cache->cpu_slab;
    assert(first != second && cache->nr_slabs == 2);
    assert(cache->nr_partial == 0 && cache->nr_inuse == 2 * n);
    assert(nr_free_pages() == before - 2);
    kmem_cache_free(cache, objects[0]);
    assert(cache->nr_partial == 1);
    kmem_cache_free(cache, objects[1]);
    assert(cache->nr_partial == 1 && cache->nr_inuse == 2 * n - 2);
    // 第二页已满，必须从 partial 取第一页；不能再向 PMM 申请第三页。
    void *p = kmem_cache_alloc(cache), *q = kmem_cache_alloc(cache);
    assert(p == objects[1] && q == objects[0]);
    assert(cache->cpu_slab == first && cache->nr_partial == 0);
    assert(nr_free_pages() == before - 2 && cache->nr_slabs == 2);
    fill(p, 64, 1);
    fill(q, 64, 0);
    for (size_t i = 0; i < 2 * n; i++) verify(objects[i], 64, i);
    for (size_t i = n; i < 2 * n; i++) kmem_cache_free(cache, objects[i]);
    assert(cache->nr_slabs == 1 && cache->nr_partial == 0);
    assert(nr_free_pages() == before - 1);
    for (size_t i = 0; i < n; i++) kmem_cache_free(cache, objects[i]);
    assert(cache->nr_inuse == 0 && cache->nr_slabs == 1);
    assert(cache->cpu_slab == first && first->inuse == 0);
    assert(kmem_cache_destroy(cache) == 0 && nr_free_pages() == before);
    cprintf("[SLUB] full/partial/active transitions and reclaim: PASS\n");
}

static void check_custom(void) {
    const size_t sizes[] = {1, 40, 65, 3000, PGSIZE - sizeof(struct slab)};
    const size_t aligns[] = {0, 16, 64, 64, 8};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        size_t before = nr_free_pages();
        struct kmem_cache *cache = kmem_cache_create("test-custom", sizes[i], aligns[i]);
        assert(cache != NULL);
        size_t align = aligns[i] < SLUB_MIN_ALIGN ? SLUB_MIN_ALIGN : aligns[i];
        assert(cache->size == ROUNDUP(sizes[i], align));
        void *p = kmem_cache_alloc(cache), *q = kmem_cache_alloc(cache);
        assert(p != NULL && q != NULL && p != q);
        assert((uintptr_t)p % align == 0 && (uintptr_t)q % align == 0);
        fill(p, cache->size, 11);
        fill(q, cache->size, 22);
        size_t slabs = cache->nr_slabs, free_pages_before = nr_free_pages();
        assert(kmem_cache_destroy(cache) == -E_INVAL);
        assert(cache->nr_inuse == 2 && cache->nr_slabs == slabs);
        assert(nr_free_pages() == free_pages_before);
        verify(p, cache->size, 11);
        verify(q, cache->size, 22);
        kmem_cache_free(cache, p);
        if (cache->objs_per_slab == 1) {
            // 非活动单对象 slab 从满直接变空，不能从 partial 摘不存在的节点。
            assert(cache->nr_slabs == 1 && cache->nr_partial == 0);
            assert(nr_free_pages() == free_pages_before + 1);
        }
        verify(q, cache->size, 22);
        kmem_cache_free(cache, q);
        assert(kmem_cache_destroy(cache) == 0 && nr_free_pages() == before);
    }
    // 超过 cache_cache 一页容量，检验缓存描述符本身的分配与释放。
    size_t before = nr_free_pages();
    struct kmem_cache *caches[40];
    for (size_t i = 0; i < 40; i++) {
        caches[i] = kmem_cache_create("test-descriptor", 32, 8);
        assert(caches[i] != NULL);
    }
    struct slab *descriptor_slab = (void *)ROUNDDOWN((uintptr_t)caches[0], PGSIZE);
    struct kmem_cache *meta = descriptor_slab->cache;
    for (size_t i = 0; i < 40; i++) assert(kmem_cache_destroy(caches[i]) == 0);
    // 7 个内置缓存描述符仍存活，旧页不能释放；额外的空活动页也会按策略保留。
    assert(meta->nr_inuse == KMALLOC_NR_CACHES && meta->nr_slabs == 2);
    assert(meta->nr_partial == 1 && nr_free_pages() == before - 1);
    // 填满空活动页，再从内置描述符所在 partial 页取一个，令其重新成为活动页。
    size_t n = meta->objs_per_slab + 1;
    assert(n <= sizeof(caches) / sizeof(caches[0]));
    for (size_t i = 0; i < n; i++) {
        caches[i] = kmem_cache_create("test-descriptor-reclaim", 32, 8);
        assert(caches[i] != NULL);
    }
    for (size_t i = 0; i < n; i++) assert(kmem_cache_destroy(caches[i]) == 0);
    assert(meta->nr_inuse == KMALLOC_NR_CACHES && meta->nr_slabs == 1);
    assert(meta->nr_partial == 0);
    assert(nr_free_pages() == before);
    cprintf("[SLUB] alignment, single-object slabs and cache lifetime: PASS\n");
}

static size_t charged_pages(size_t pages) {
    // 读取实际管理器，避免 make 免 clean 切换 PMM 时复用旧测试对象的编译宏。
    if (strcmp(pmm_manager->name, "buddy_pmm_manager") != 0) return pages;
    size_t rounded = 1;
    while (rounded < pages) rounded *= 2;
    return rounded;
}

static void check_large(void) {
    const size_t header = ROUNDUP(sizeof(struct slab), 16);
    const size_t sizes[] = {1025, PGSIZE - header, PGSIZE - header + 1,
        PGSIZE, PGSIZE + 1, 9000, 20000};
    size_t before = nr_free_pages(), total = 0;
    size_t count = sizeof(sizes) / sizeof(sizes[0]);
    for (size_t i = 0; i < count; i++) {
        size_t pages = (sizes[i] + header + PGSIZE - 1) / PGSIZE;
        objects[i] = kmalloc(sizes[i]);
        assert(objects[i] != NULL && (uintptr_t)objects[i] % 16 == 0);
        assert(kmalloc_cache(sizes[i]) == NULL);
        assert(ksize(objects[i]) == pages * PGSIZE - header);
        total += charged_pages(pages);
        assert(nr_free_pages() == before - total);
        fill(objects[i], ksize(objects[i]), i);
    }
    for (size_t i = count; i-- > 0;) {
        verify(objects[i], ksize(objects[i]), i);
        kfree(objects[i]);
    }
    assert(nr_free_pages() == before);
    cprintf("[SLUB] large boundaries, contents and page accounting: PASS\n");
}

static void check_oom(void) {
    size_t before = nr_free_pages();
    struct kmem_cache *full = kmem_cache_create("test-oom-full", 3000, 8);
    struct kmem_cache *empty = kmem_cache_create("test-oom-empty", 64, 8);
    assert(full != NULL && empty != NULL && full->objs_per_slab == 1);
    void *p = kmem_cache_alloc(full);
    fill(p, 3000, 88);
    struct slab *active = full->cpu_slab;
    list_entry_t held;
    list_init(&held);
    struct Page *page;
    size_t available = nr_free_pages(), taken = 0;
    // 已分配页的 page_link 只用于测试记账，不读取或修改 PMM 私有空闲链。
    while ((page = alloc_page()) != NULL) {
        list_add(&held, &page->page_link);
        taken++;
    }
    assert(taken == available && nr_free_pages() == 0);
    assert(kmem_cache_alloc(full) == NULL && kmem_cache_alloc(empty) == NULL);
    assert(kmalloc(9000) == NULL);
    assert(full->cpu_slab == active && full->nr_slabs == 1);
    assert(full->nr_inuse == 1 && full->nr_partial == 0);
    assert(empty->cpu_slab == NULL && empty->nr_slabs == 0 && empty->nr_inuse == 0);
    verify(p, 3000, 88);
    kmem_cache_free(full, p);
    void *reused = kmem_cache_alloc(full);
    assert(reused == p && nr_free_pages() == 0);
    kmem_cache_free(full, reused);
    while (!list_empty(&held)) {
        list_entry_t *le = list_next(&held);
        list_del(le);
        free_page(le2page(le, page_link));
    }
    assert(nr_free_pages() == available);
    p = kmem_cache_alloc(empty);
    fill(p, 64, 99);
    verify(p, 64, 99);
    kmem_cache_free(empty, p);
    assert(kmem_cache_destroy(empty) == 0 && kmem_cache_destroy(full) == 0);
    assert(nr_free_pages() == before);
    cprintf("[SLUB] real out-of-memory failure and recovery: PASS\n");
}

// panic 会停机：每个负向用例由 tests/run_slub_tests.py 单独启动一次 QEMU。
static void check_panic(void) {
    void *p;
    cprintf("[SLUB panic %d] begin\n", SLUB_PANIC_TEST);
    switch (SLUB_PANIC_TEST) {
    case 1:
        p = kmalloc(32);
        assert(p != NULL);
        kfree(p);
        kfree(p);
        break;
    case 2:
        p = kmalloc(32);
        assert(p != NULL);
        kmem_cache_free(kmalloc_cache(64), p);
        break;
    case 3:
        p = kmalloc(32);
        assert(p != NULL);
        kfree((char *)p + 1);
        break;
    case 4:
        p = kmalloc(5000);
        assert(p != NULL);
        kfree((char *)p + 1);
        break;
    case 5: {
        struct Page *page = alloc_page();
        assert(page != NULL);
        p = (void *)(page2pa(page) + va_pa_offset);
        memset(p, 0, PGSIZE);
        kfree((char *)p + 64);
        break;
    }
    case 6:
        p = kmalloc(5000);
        assert(p != NULL);
        kfree(p);
        kfree(p);
        break;
    }
    panic("SLUB negative test unexpectedly returned");
}

void slub_check(void) {
    if (SLUB_PANIC_TEST != 0) {
        check_panic();
        return;
    }
    // 第一次预热会留下 7 个通用缓存活动页；第二次必须没有额外页损失。
    for (int round = 1; round <= 2; round++) {
        size_t before = nr_free_pages(), cold = 0;
        for (size_t size = 16; size <= SLUB_MAX_SIZE; size *= 2) {
            if (kmalloc_cache(size)->cpu_slab == NULL) cold++;
        }
        check_invalid();
        check_sizes();
        check_partial();
        check_custom();
        check_large();
        check_oom();
        assert(nr_free_pages() == before - cold);
        cprintf("[SLUB] round %d: PASS (free pages %lu -> %lu, retained %lu)\n",
                round, before, nr_free_pages(), cold);
    }
    slub_print_stats();
    cprintf("[SLUB] ALL TESTS PASSED\n");
}
