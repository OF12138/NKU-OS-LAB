#include <buddy_pmm.h>
#include <list.h>
#include <stdio.h>
#include <assert.h>

// lab2 function2-3 test 2411264
// Buddy System 的独立测试：只通过 pmm_manager 的公开接口和 struct Page 的公开字段检验行为，
// 不读 buddy_pmm.c 的私有链表。做法是先把系统中所有空闲页借走（drain），
// 让分配器里只剩测试区 R（64 页、按 64 页对齐），这样每一次分配返回哪一页都可以精确预测。

#define REGION 64

static list_entry_t held;   // 借出的块，用已分配块首页的 page_link 串起来（分配器不使用已分配页的 page_link）
static struct Page *R;      // 测试区首页

#define NR_FREE() (pmm_manager->nr_free_pages())

// 分配 n 页并检查：返回 expect（为 NULL 时表示应当失败），首页 property 为向上取整后的块长，空闲计数减少块长
static struct Page *
expect_alloc(size_t n, struct Page *expect) {
    size_t before = NR_FREE();
    struct Page *p = alloc_pages(n);
    assert(p == expect);
    if (p == NULL) {
        assert(NR_FREE() == before);
        return NULL;
    }
    size_t size = 1;
    while (size < n) size <<= 1;
    assert(p->property == size && !PageProperty(p) && !PageReserved(p));
    assert((page2ppn(p) & (size - 1)) == 0);
    assert(NR_FREE() == before - size);
    return p;
}

// 释放 n 页（n 可以是原请求页数，也可以是实际块长），检查空闲计数增加实际块长
static void
expect_free(struct Page *p, size_t n) {
    size_t before = NR_FREE(), size = p->property;
    free_pages(p, n);
    assert(NR_FREE() == before + size);
}

// 把 [base, base + n) 标成保留页，模拟 page_init 交给 init_memmap 之前的状态
static void
reserve(struct Page *base, size_t n) {
    for (struct Page *p = base; p != base + n; p++) {
        p->flags = 0;
        p->property = 0;
        SetPageReserved(p);
    }
}

// 借出全部空闲页：从最大阶到 0 阶反复分配，直到每一阶都失败
static void
drain(size_t before) {
    size_t total = 0;
    list_init(&held);
    for (int k = BUDDY_MAX_ORDER; k >= 0; k--) {
        struct Page *p;
        while ((p = alloc_pages(1UL << k)) != NULL) {
            assert(p->property == (1UL << k) && (page2ppn(p) & ((1UL << k) - 1)) == 0);
            list_add(&held, &p->page_link);
            total += 1UL << k;
        }
    }
    // 借出的总页数必须正好等于之前的空闲计数：空闲计数与链表中的块一致，没有漏页也没有重复
    assert(total == before && NR_FREE() == 0);
}

static void
restore(void) {
    while (!list_empty(&held)) {
        list_entry_t *le = list_next(&held);
        list_del(le);
        struct Page *p = le2page(le, page_link);
        free_pages(p, p->property);
    }
}

// 用两种字节值填满 [p, p + n) 页，再读回检查，用于发现重叠分配
static void
fill(struct Page *p, size_t n, unsigned char v) {
    unsigned char *mem = (unsigned char *)(page2pa(p) + va_pa_offset);
    for (size_t i = 0; i < n * PGSIZE; i++) mem[i] = v;
}

static void
verify(struct Page *p, size_t n, unsigned char v) {
    unsigned char *mem = (unsigned char *)(page2pa(p) + va_pa_offset);
    for (size_t i = 0; i < n * PGSIZE; i++) assert(mem[i] == v);
}

// 测试一：整块注册后逐级分裂，再以打乱的顺序释放，最终合并回一个 64 页块
static void
check_split_merge(void) {
    reserve(R, REGION);
    pmm_manager->init_memmap(R, REGION);
    assert(NR_FREE() == REGION);
    // 申请 1 页：64 → 32+32 → … → 1+1，每次保留左半块，交出 R
    struct Page *a = expect_alloc(1, R);
    // 剩下的块恰好是 R+1(1)、R+2(2)、R+4(4)、…、R+32(32)
    struct Page *b = expect_alloc(1, R + 1);
    struct Page *c = expect_alloc(2, R + 2);
    struct Page *d = expect_alloc(4, R + 4);
    struct Page *e = expect_alloc(8, R + 8);
    struct Page *f = expect_alloc(16, R + 16);
    struct Page *g = expect_alloc(32, R + 32);
    assert(NR_FREE() == 0);
    expect_alloc(1, NULL);                      // 耗尽：没有任何空闲页
    // 两个 1 页伙伴先合并成 2 页块
    expect_free(a, 1);
    expect_free(b, 1);
    expect_alloc(1, R);                         // 若 a、b 已合并，1 页请求要从 2 页块中再分裂，仍返回 R
    expect_free(R, 1);
    expect_alloc(2, R);                         // 2 页请求直接拿到合并后的块
    expect_free(R, 2);
    // 打乱顺序释放其余块，合并结果不应依赖释放顺序
    expect_free(g, 32);
    expect_free(e, 8);
    expect_free(c, 2);
    expect_free(f, 16);
    expect_free(d, 4);
    assert(NR_FREE() == REGION);
    struct Page *all = expect_alloc(REGION, R); // 能整块拿回 64 页，说明逐级合并完整
    expect_free(all, REGION);
    cprintf("[BUDDY] split 64 -> 1 and merge back: PASS\n");
}

// 测试二：非 2 的幂请求向上取整、两种释放页数、数据互不覆盖，以及超大请求失败
static void
check_round_and_data(void) {
    struct Page *p = expect_alloc(3, R);        // 3 → 4 页
    struct Page *q = expect_alloc(5, R + 8);    // 5 → 8 页，R+4 的 4 页块放不下
    struct Page *r = expect_alloc(9, R + 16);   // 9 → 16 页
    assert(NR_FREE() == REGION - 28);
    fill(p, 4, 0xa1);
    fill(q, 8, 0xb2);
    fill(r, 16, 0xc3);
    verify(p, 4, 0xa1);
    verify(q, 8, 0xb2);
    verify(r, 16, 0xc3);
    expect_alloc(REGION + 1, NULL);             // 超过测试区总页数（其余空闲页都已借走）
    expect_alloc(64, NULL);                     // 空闲 36 页，但没有 64 页的块
    expect_alloc((size_t)-1, NULL);             // 极大请求不能因取整溢出而误分配
    expect_free(p, 3);                          // 按原请求页数释放，归还整块 4 页
    expect_free(q, 8);                          // 按实际块长释放
    expect_free(r, 9);
    assert(NR_FREE() == REGION);
    struct Page *all = expect_alloc(REGION, R);
    expect_free(all, REGION);
    cprintf("[BUDDY] round up 3/5/9 -> 4/8/16, data isolation, oversize NULL: PASS\n");
}

// 测试三：地址相邻、大小相同但不是伙伴的块不能合并；空闲页数够但没有足够大的块时分配失败
static void
check_not_buddy(void) {
    struct Page *a = expect_alloc(16, R);
    struct Page *b = expect_alloc(16, R + 16);
    struct Page *c = expect_alloc(16, R + 32);
    struct Page *d = expect_alloc(16, R + 48);
    expect_free(b, 16);
    expect_free(c, 16);                         // b、c 相邻，但 b 的伙伴是 a，c 的伙伴是 d
    assert(NR_FREE() == 32);
    expect_alloc(32, NULL);                     // 若错误合并了 b、c，这里会返回 R+16
    expect_free(a, 16);                         // a、b 是伙伴，合并成 R 处的 32 页块
    struct Page *ab = expect_alloc(32, R);
    expect_free(ab, 32);
    expect_free(d, 16);                         // c、d 合并，再与 R 处的 32 页块合并
    struct Page *all = expect_alloc(REGION, R);
    assert(all == R);
    cprintf("[BUDDY] adjacent non-buddies stay apart, fragmentation fails: PASS\n");
}

// 测试四：注册未对齐、非 2 的幂长度的区间，再分段注册剩余部分，最终合并成完整的 64 页块
static void
check_unaligned_init(void) {
    // 进入时 R 处的 64 页由测试三整块持有，重新标成保留页
    reserve(R, REGION);
    pmm_manager->init_memmap(R + 3, 13);        // [R+3, R+16)：应拆成 R+3(1)、R+4(4)、R+8(8)
    assert(NR_FREE() == 13);
    expect_alloc(16, NULL);
    struct Page *x = expect_alloc(8, R + 8);
    struct Page *y = expect_alloc(4, R + 4);
    struct Page *z = expect_alloc(1, R + 3);
    expect_alloc(1, NULL);
    expect_free(x, 8);
    expect_free(y, 4);
    expect_free(z, 1);
    pmm_manager->init_memmap(R, 3);             // [R, R+3) 与已有的块逐级合并成 R 处的 16 页块
    assert(NR_FREE() == 16);
    struct Page *sixteen = expect_alloc(16, R);
    expect_free(sixteen, 16);
    pmm_manager->init_memmap(R + 16, REGION - 16);
    assert(NR_FREE() == REGION);
    struct Page *all = expect_alloc(REGION, R);
    expect_free(all, REGION);
    cprintf("[BUDDY] unaligned/odd-length and segmented init_memmap: PASS\n");
}

void
buddy_test(void) {
    assert(pmm_manager == &buddy_pmm_manager);
    size_t before = NR_FREE();
    R = alloc_pages(REGION);
    assert(R != NULL && (page2ppn(R) & (REGION - 1)) == 0);
    drain(before - REGION);
    // 测试区的页现在由测试持有；先整块释放进空的分配器，各项测试从这里开始
    check_split_merge();
    check_round_and_data();
    check_not_buddy();
    check_unaligned_init();
    // 测试区重新整块释放，再归还借出的全部块，空闲页数必须恢复
    assert(NR_FREE() == REGION);
    struct Page *all = expect_alloc(REGION, R);
    expect_free(all, REGION);
    restore();
    assert(NR_FREE() == before);
    cprintf("[BUDDY] all tests passed (%lu free pages restored)\n", (unsigned long)before);
}
