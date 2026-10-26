// T2 补充回归：仅由 run_best_fit.sh 放入临时内核，不改框架测试。
#include <pmm.h>
#include <best_fit_pmm.h>
#include <stdio.h>

void
best_fit_regression_check(void) {
    assert(pmm_manager == &best_fit_pmm_manager);
    size_t total = nr_free_pages();
    assert(total >= 64);
    // 启动自检之后空闲区应恢复为一整块，暂时取走以隔离测试。
    struct Page *arena = alloc_pages(total);
    assert(arena != NULL && nr_free_pages() == 0);
    assert(alloc_page() == NULL);

    free_pages(arena + 2, 8);
    free_pages(arena + 20, 4);
    free_pages(arena + 32, 6);
    free_pages(arena + 44, 4);
    assert(nr_free_pages() == 22);
    assert(alloc_pages(3) == arena + 20);  // 最小适合块，同大小选低地址
    assert(alloc_page() == arena + 23);   // 分裂的剩余块
    assert(alloc_pages(4) == arena + 44);  // 精确匹配
    assert(alloc_pages(5) == arena + 32);  // 不选择前面的 8 页块
    assert(alloc_page() == arena + 37);
    assert(alloc_pages(8) == arena + 2);
    assert(nr_free_pages() == 0);

    free_pages(arena + 20, 4);
    free_pages(arena + 44, 4);
    assert(nr_free_pages() == 8);
    assert(alloc_pages(5) == NULL);        // 总数够，但不连续
    assert(alloc_pages(9) == NULL);        // 总数也不够
    assert(nr_free_pages() == 8);          // 失败不能改变状态
    assert(alloc_pages(4) == arena + 20);
    assert(alloc_pages(4) == arena + 44);
    cprintf("[T2 regression] selection/split/failure: PASS\n");

    free_pages(arena, 2);
    free_pages(arena + 10, 2);
    free_pages(arena + 2, 8);              // 一次桥接前后两个块
    assert(nr_free_pages() == 12);
    assert(alloc_pages(12) == arena);
    free_pages(arena + 16, 4);
    free_pages(arena + 20, 4);             // 尾插并向前合并
    assert(alloc_pages(8) == arena + 16);
    free_pages(arena + 24, 4);
    free_pages(arena + 20, 4);             // 头插并向后合并
    assert(alloc_pages(8) == arena + 20);
    assert(nr_free_pages() == 0);
    cprintf("[T2 regression] predecessor/successor/bridge merge: PASS\n");

    // 分段、乱序注册空闲区，检验初始化路径也能合并相邻块。
    for (size_t i = 0; i < 64; i++) {
        SetPageReserved(arena + i);
    }
    pmm_manager->init_memmap(arena + 16, 16);
    pmm_manager->init_memmap(arena, 16);
    pmm_manager->init_memmap(arena + 32, 32);
    assert(nr_free_pages() == 64);
    for (size_t i = 0; i < 64; i++) {
        assert(!PageReserved(arena + i) && page_ref(arena + i) == 0);
        assert(PageProperty(arena + i) == (i == 0));
        assert(arena[i].property == (i == 0 ? 64 : 0));
    }
    assert(alloc_pages(64) == arena);
    assert(nr_free_pages() == 0);
    free_pages(arena, total);
    assert(nr_free_pages() == total);
    cprintf("[T2 regression] memmap/state restoration: PASS (%lu pages)\n",
            (unsigned long)total);
}
