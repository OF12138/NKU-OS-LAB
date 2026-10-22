#include <buddy_pmm.h>
#include <list.h>
#include <stdio.h>

// lab2 function2-3 2414099
/*free_lists ： 16 个链表头组成的数组
free_lists[0] → 所有空闲的 1 页块
free_lists[1] → 所有空闲的 2 页块
free_lists[2] → 所有空闲的 4 页块
free_lists[3] → 所有空闲的 8 页块
……
*/
static list_entry_t free_lists[BUDDY_MAX_ORDER + 1];
static size_t nr_free; // 当前空闲页总数

// 计算请求属于哪一阶
// 超过最大阶时返回哨兵，避免对超大 n 取整时溢出。
static unsigned int
buddy_order(size_t n) {
    unsigned int order = 0;
    while (order <= BUDDY_MAX_ORDER && (1UL << order) < n) {
        order++;
    }
    return order;
}

// 初始化：把所有链表初始化为空、计数清零
static void
buddy_init(void) {
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&free_lists[order]);
    }
    nr_free = 0;
}

// buddy_insert_block() ：用于把一个块放回空闲区，初始化和释放都会调用它
// 只合并同阶且真正互为伙伴的块；按物理页号找伙伴，不能按相邻地址合并。
static void
buddy_insert_block(struct Page *base, unsigned int order) {
    while (order < BUDDY_MAX_ORDER) {
        size_t size = 1UL << order;
        size_t buddy_ppn = page2ppn(base) ^ size;
        if (buddy_ppn < nbase || buddy_ppn >= npage ||
            size > npage - buddy_ppn) {
            break;
        }
        struct Page *buddy = pages + (buddy_ppn - nbase);
        if (!PageProperty(buddy) || buddy->property != size) {
            break;
        }
        list_del(&buddy->page_link);
        ClearPageProperty(buddy);
        buddy->property = 0;
        if (buddy < base) {
            base = buddy;
        }
        order++;
    }

    // 最后把合并后的块加入对应阶数的空闲链表
    base->property = 1UL << order;
    SetPageProperty(base);
    list_add(&free_lists[order], &base->page_link);
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    // 先检查范围
    assert(n > 0 && base >= pages && base < pages + (npage - nbase));
    assert(n <= (size_t)(pages + (npage - nbase) - base));

    // 逐页检查并复位：flags 清零，引用计数置 0
    for (struct Page *p = base; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    nr_free += n;

    // 接下来，把区间拆成合法的 Buddy 块：
    // 贪心切出不越界且按物理页号对齐的最大块，尾部不足大块的页也全部保留。
    while (n != 0) {
        unsigned int order = BUDDY_MAX_ORDER;
        // 必须满足：块大小不能超过剩余页数、且块起始页号必须是块大小的整数倍（即按物理页号对齐）。
        while ((1UL << order) > n ||
               (page2ppn(base) & ((1UL << order) - 1)) != 0) {
            order--;
        }
        size_t size = 1UL << order;
        buddy_insert_block(base, order);
        base += size;
        n -= size;
    }
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    // 计算需要的阶数
    unsigned int order = buddy_order(n);
    if (order > BUDDY_MAX_ORDER || (1UL << order) > nr_free) {
        return NULL;
    }

    // 从目标阶开始，找第一个非空链表
    unsigned int found = order;
    while (found <= BUDDY_MAX_ORDER && list_empty(&free_lists[found])) {
        found++;
    }
    if (found > BUDDY_MAX_ORDER) {
        return NULL;
    }

    struct Page *base = le2page(list_next(&free_lists[found]), page_link);
    list_del(&base->page_link);
    ClearPageProperty(base);
    base->property = 0;
    // 留下左半块继续分裂，右半块挂回对应阶。
    while (found > order) {
        found--;
        struct Page *right = base + (1UL << found);
        right->property = 1UL << found;
        SetPageProperty(right);
        list_add(&free_lists[found], &right->page_link);
    }
    base->property = 1UL << order;
    nr_free -= base->property;
    return base;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0 && base >= pages && base < pages + (npage - nbase));
    unsigned int order = buddy_order(n);
    assert(order <= BUDDY_MAX_ORDER);
    size_t size = 1UL << order;
    assert(size <= (size_t)(pages + (npage - nbase) - base));
    assert((page2ppn(base) & (size - 1)) == 0);
    assert(!PageProperty(base) && base->property == size);
    for (struct Page *p = base; p != base + size; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    nr_free += size;
    buddy_insert_block(base, order);
}

static size_t
buddy_nr_free_pages(void) {
    return nr_free;
}

// 检查分阶、物理对齐、页状态、计数及伙伴合并不变量。
static void
buddy_check_state(size_t counts[BUDDY_MAX_ORDER + 1]) {
    size_t total = 0;
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        counts[order] = 0;
        size_t size = 1UL << order;
        list_entry_t *head = &free_lists[order];
        assert(list_next(list_prev(head)) == head);
        assert(list_prev(list_next(head)) == head);
        for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
            assert(++counts[order] <= npage - nbase);
            assert(list_next(list_prev(le)) == le);
            assert(list_prev(list_next(le)) == le);
            struct Page *p = le2page(le, page_link);
            assert(p >= pages && p < pages + (npage - nbase));
            assert(size <= (size_t)(pages + (npage - nbase) - p));
            assert((page2ppn(p) & (size - 1)) == 0);
            assert(PageProperty(p) && p->property == size);
            for (size_t i = 0; i < size; i++) {
                assert(!PageReserved(p + i) && page_ref(p + i) == 0);
                if (i != 0) {
                    assert(!PageProperty(p + i) && p[i].property == 0);
                }
            }
            size_t buddy_ppn = page2ppn(p) ^ size;
            if (order < BUDDY_MAX_ORDER && buddy_ppn >= nbase && buddy_ppn < npage) {
                struct Page *buddy = pages + (buddy_ppn - nbase);
                assert(!PageProperty(buddy) || buddy->property != size);
            }
            total += size;
        }
    }
    assert(total == nr_free);
}

// 实现方启动自检；独立测试与提示词二由 openfar 补充。
static void
buddy_check(void) {
    size_t before[BUDDY_MAX_ORDER + 1], after[BUDDY_MAX_ORDER + 1];
    size_t saved = nr_free;
    buddy_check_state(before);
    assert(alloc_pages((size_t)-1) == NULL && nr_free == saved);
    struct Page *a = alloc_pages(1);
    struct Page *b = alloc_pages(3);
    struct Page *c = alloc_pages(9);
    assert(a != NULL && b != NULL && c != NULL);
    assert(a->property == 1 && b->property == 4 && c->property == 16);
    assert((page2ppn(b) & 3) == 0 && (page2ppn(c) & 15) == 0);
    assert(nr_free == saved - 21);
    volatile unsigned char *data = (void *)(page2pa(c) + va_pa_offset);
    data[0] = 0x5a;
    data[9 * PGSIZE - 1] = 0xa5;
    assert(data[0] == 0x5a && data[9 * PGSIZE - 1] == 0xa5);
    free_pages(b, 3);
    free_pages(a, 1);
    free_pages(c, 9);
    buddy_check_state(after);
    assert(nr_free == saved);
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        assert(before[order] == after[order]);
    }
    cprintf("buddy: check passed (%lu free pages)\n", (unsigned long)nr_free);
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
