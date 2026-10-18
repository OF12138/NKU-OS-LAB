#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/* *
 * default_pmm.c - 默认的物理内存管理器：First-Fit算法
 *
 * 数据结构：
 *   把所有【连续的空闲页块】用一条双向链表串起来，链表按物理地址从低到高排序。
 *   每个空闲块只有“首页”的 Page 结构体挂在链表上（通过 page_link），并且：
 *       首页.property = 该块包含的页数；首页的 PG_property 标志位 = 1
 *   块内其余页的 property、PG_property 均为 0，不在链表上。例如：
 *
 *     free_list ⇄ [页10: property=3] ⇄ [页20: property=5] ⇄ free_list
 *                  └ 页10~12 空闲 ┘     └ 页20~24 空闲 ┘
 *
 * 算法：
 *   分配 n 页：从链表头开始找第一个 property >= n 的块，从块头切下 n 页，剩余部分仍留在链表中。
 *   释放 n 页：按地址顺序插回链表，并与地址上紧邻的前、后空闲块合并，减少碎片。
 *
 * 与 pmm.c 的关系：pmm.c 的 init_pmm_manager 令 pmm_manager = &default_pmm_manager，
 * 之后 alloc_pages / free_pages 等都转调到本文件末尾的函数指针表里。
 * */

// 全局唯一的空闲区描述：free_list 是链表头（哨兵节点，自身不对应任何页），
// nr_free 是所有空闲块的页数总和
static free_area_t free_area;

// 用宏起别名，下面的代码（包括检查函数）就可以直接写 free_list、nr_free，
// 而不用每次写 free_area.xxx。注意这两个名字是宏，不是变量。
#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

// default_init - 初始化管理器：把空闲链表置空（头结点 prev/next 都指向自己），空闲页数清零
// 由 pmm.c 的 init_pmm_manager 调用，此时还没有任何内存交给本管理器
static void
default_init(void) 
{
    list_init(&free_list);
    nr_free = 0;
}

/* *
 * default_init_memmap - 把一段连续的空闲物理页登记为一个空闲块
 * @base: 这段内存第一页的 Page 描述符（pages 数组里的某个元素）
 * @n:    页数
 * 由 pmm.c 的 page_init 调用：此时所有页都已被标为 PG_reserved，这里把
 * [base, base+n) 这 n 页“释放”出来，变成可分配的内存。
 * */
static void default_init_memmap(struct Page *base, size_t n) 
{
    assert(n > 0);
    struct Page *p = base;

    // 逐页清理：确认页之前确实是保留状态（page_init 设置的），然后清空 flags
    // （这同时清掉了 PG_reserved）和 property，引用计数置 0
    for (; p != base + n; p ++) 
    {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }

    // 只有块的首页记录块大小，并打上 PG_property 标记“我是空闲块的首页”
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    // 把这个块按地址顺序插入空闲链表
    if (list_empty(&free_list)) 
    {
        // 链表为空：直接挂在头结点后面
        list_add(&free_list, &(base->page_link));
    } 
    else 
    {
        // 从第一个块开始遍历（le 初值是头结点，while 条件里先取 next）
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);  // 链表节点 -> 所在的 Page
            if (base < page) {
                // 找到第一个地址比 base 大的块：插在它前面，保持地址递增
                list_add_before(le, &(base->page_link));
                break;
            } 
            else if (list_next(le) == &free_list) {
                // 已是最后一个块，base 比所有块都靠后：插在末尾
                // NOTE: 这里没有 break。插入后循环会再取一次 le = base 的节点，又走进本分支，
                //       对 base 自己再 list_add 一次，使 base->page_link.prev 指向自己（已用小程序验证）。
                //       本函数只在启动时对一整块内存调用一次，链表为空，不会走到这里；
                //       default_free_pages 里有同样的写法，见那里的说明。
                list_add(le, &(base->page_link));
            }
        }
    }
}

/* *
 * default_alloc_pages - First-Fit 分配：返回 n 个连续物理页的首页，失败返回 NULL
 * 注意：返回的 Page 对应的物理页由调用者负责，不会清零。
 * */
static struct Page *default_alloc_pages(size_t n) 
{
    assert(n > 0);
    // 快速失败：全部空闲页加起来都不够 n 页
    if (n > nr_free) {
        return NULL;
    }

    // 第一步：从链表头开始，找第一个够大（property >= n）的空闲块 —— 这就是“首次适应”
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) 
    {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) 
        {
            page = p;
            break;
        }
    }

    // 第二步：如果找到了，从该块头部切出 n 页
    if (page != NULL) 
    {
        // 先记下前驱节点，用来把剩余部分插回原来的位置（保持地址有序）
        list_entry_t* prev = list_prev(&(page->page_link));
        // 把整块摘下链表
        list_del(&(page->page_link));
        // 块比需要的大：剩余的后半段 [page+n, page+property) 成为新的空闲块，
        // 它的首页是 page+n，重新设置 property 和 PG_property，挂回原位置
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        // 被分配出去的首页不再是空闲块首页，清掉 PG_property
        // （page->property 没清零，但只要没有 PG_property 标志就没有意义）
        ClearPageProperty(page);
    }
    return page;
}

/* *
 * default_free_pages - 释放 [base, base+n) 这 n 个连续页，并尽量与相邻空闲块合并
 * 前提：这些页必须是之前分配出去的（不是保留页，也不是空闲块首页），否则 assert 失败。
 * */
static void default_free_pages(struct Page *base, size_t n) 
{
    assert(n > 0);
    struct Page *p = base;
    // 逐页检查并复位：flags 清零，引用计数置 0
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    // 把这 n 页登记为一个新的空闲块（首页记录大小并打 PG_property 标记）
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    // 第一步：按地址顺序把新块插入链表（逻辑与 default_init_memmap 相同）
    // NOTE: 同样缺少 break。当 base 的地址比所有已有空闲块都大（插在链表末尾）时，
    //       插入后循环多执行一次，使 base->page_link.prev 指向自己而不是前一个块；
    //       结果是下面“与前一个块合并”的检查会漏掉这种情况（该块的前驱指针已经错了）。
    //       框架自带的 default_check 其实会走到这条路径（free_pages(p1, 3)），
    //       但前驱指针随后在分配摘链时被改回，所以测试没有暴露它。
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }

    // 第二步：向前合并。取链表中 base 的前一个块 p；
    // 若 p 的末尾正好接上 base（p + p->property == base，Page 数组下标连续就等于物理地址连续），
    // 就把 base 并入 p：p 变大，base 不再是块首页，摘下链表，之后以 p 作为合并后的块
    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) 
    {
        p = le2page(le, page_link);
        if (p + p->property == base) 
        {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    // 第三步：向后合并。取 base（可能已是合并后的块）的后一个块 p；
    // 若 base 的末尾正好接上 p，就把 p 并入 base
    le = list_next(&(base->page_link));
    if (le != &free_list) 
    {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}

// default_nr_free_pages - 返回当前空闲页的总数
static size_t
default_nr_free_pages(void) {
    return nr_free;
}

/* *
 * basic_check - 基础测试：只分配/释放单页，验证最基本的正确性（框架提供，不要修改）
 * 做法：先分配 3 页，再把真实的空闲链表“藏起来”换成空链表，在干净环境里测试，
 * 最后恢复链表并把页还回去。
 * */
static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    // 三个页互不相同，引用计数为 0，且物理地址在合法范围内
    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    // 暂存真实的空闲链表和空闲页数，换上空链表：此后可用页只有我们自己放回去的
    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    // 没有空闲页时分配必须失败
    assert(alloc_page() == NULL);

    // 放回 3 页，空闲页数应为 3
    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    // 再分配 3 页应成功，之后又没有空闲页了
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    // 释放 p0 后链表非空，再分配一页应正好拿到 p0
    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    // 恢复真实的链表和计数，把借用的页还回去
    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1)
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
/* *
 * default_check - First-Fit 的完整测试（框架提供，不要修改）
 * 先统计链表里的块数 count 和页数 total 作为“基线”，测试结束后再统计一次，
 * 两者都要减回 0，说明测试没有破坏原有的空闲链表。
 * */
static void
default_check(void) {
    // 基线：遍历链表，每个块的首页必须带 PG_property，页数之和必须等于 nr_free
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    // 分配 5 页（p0..p0+4），这 5 页是后面测试的“试验田”
    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    // 再次藏起真实链表，换上空链表
    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    // 只放回后 3 页 [p0+2, p0+5)：链表里只有一个 3 页的块
    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);                       // 4 页放不下
    assert(PageProperty(p0 + 2) && p0[2].property == 3);  // 块首页记录了大小 3
    assert((p1 = alloc_pages(3)) != NULL);                // 正好 3 页，整块拿走
    assert(alloc_page() == NULL);                         // 之后没有空闲页
    assert(p0 + 2 == p1);

    // 释放 p0 和 p1 起的 3 页：中间隔着 p2=p0+1（仍被占用），两块不相邻，不应合并
    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    // 首次适应：1 页的请求取链表里第一个块（p0），而不是更大的那一块
    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    // 2 页的请求：p0 块只有 1 页放不下，取第二个块 p1 的前 2 页，即 p2+1
    assert((p0 = alloc_pages(2)) == p2 + 1);

    // 释放 2 页和 p2：此时 p0 块、p2、新释放的 2 页、剩下的 1 页应能全部合并回一块 5 页
    free_pages(p0, 2);
    free_page(p2);

    // 合并成功的证明：一次能再分配出连续的 5 页，之后再无空闲页
    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    // 恢复现场：还原空闲页数和真实链表，把 5 页还回去
    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    // 再统计一遍：count、total 减回 0，说明与测试前完全一致
    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

// 函数指针表：把本文件的各个函数按 struct pmm_manager（pmm.h）规定的接口登记起来
// （C99 指定初始化器 .成员 = 值）。pmm.c 里 pmm_manager = &default_pmm_manager，
// 之后对 pmm_manager->xxx 的调用就转到这里的函数。非 static，声明见 default_pmm.h
const struct pmm_manager default_pmm_manager =
{
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,

    /*
    这里的 . 是 C99 指定初始化器（designated initializer） 的语法。
    作用
    它用来按名字初始化结构体成员，而不是按顺序。
    */
};
