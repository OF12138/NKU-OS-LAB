## 功能模块二·任务三：Buddy System（提示词二，测试函数）

[PROMPT]

**任务**：为提示词一生成的 Buddy System 分配器编写独立的启动测试。新建 `code/kern/mm/buddy_test.c`，提供 `buddy_test()`，在 `buddy_pmm.h` 中声明；由 `kern_init()` 在 `slub_check()` 之后、仅当 `pmm_manager == &buddy_pmm_manager` 时调用。测试负责人为 2411264-张远。

**操作要求**：直接修改真实文件，保留现有代码和学习注释。只通过 `pmm_manager` 的公开回调、`alloc_pages`/`free_pages`/`nr_free_pages` 和 `struct Page` 的公开字段检验行为，**不读写 `buddy_pmm.c` 的私有链表和计数，也不修改被测实现**；不修改评分脚本、Best-Fit 测试和 `struct Page`。判断当前管理器用运行时比较，不用 `PMM_buddy` 宏（Makefile 切换 PMM 时只重编 `pmm.c`）。新增代码用 `lab2 2411264` 标明，写简短中文注释。

**输出要求**：测试代码、每组测试的覆盖说明、实测输出。另外做一次变异测试：在临时副本中向 `buddy_pmm.c` 注入典型错误（关闭实现方自检和 SLUB 自测，只考察本测试），列出每种错误是否被发现、在哪条断言被发现。使用 QEMU 4.1.1，在 128 MiB 和 64 MiB 下各运行一次，默认 `make grade` 仍须为 20/20。

[RELY]

框架接口摘自导入提交 `84603fb`，省略注释；被测模块的接口摘自提示词一生成的 `buddy_pmm.h`。

```c
// code/kern/mm/memlayout.h
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};
#define SetPageReserved(page)       ((page)->flags |= (1UL << PG_reserved))
#define PageReserved(page)          (((page)->flags >> PG_reserved) & 1)
#define PageProperty(page)          (((page)->flags >> PG_property) & 1)
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)
```

```c
// code/kern/mm/pmm.h
struct pmm_manager {
    const char *name;
    void (*init)(
        void);
    void (*init_memmap)(
        struct Page *base,
        size_t n);
    struct Page *(*alloc_pages)(
        size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};
extern const struct pmm_manager *pmm_manager;
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
extern uint64_t va_pa_offset;
static inline ppn_t page2ppn(struct Page *page) { return page - pages + nbase; }
```

```c
// code/kern/mm/buddy_pmm.h（被测模块）
#define BUDDY_MAX_ORDER 15
extern const struct pmm_manager buddy_pmm_manager;
```

被测实现的约定（来自提示词一及 `buddy_pmm.h` 的注释）：

- 第 k 阶块有 2^k 页，起始物理页号是 2^k 的倍数；伙伴页号 = 本块页号 XOR 块页数，只有空闲且同阶的伙伴才合并。
- `alloc_pages(n)` 占用 2^⌈log2 n⌉ 页，从目标阶向上找第一个非空链表，分裂时保留左半块、右半块挂回链表；失败返回 NULL，空闲计数不变。
- 已分配块首页的 `property` 为实际块长，`PG_property` 只标记空闲块首页。`free_pages` 可以传原请求页数或实际块长，不支持部分释放。
- `init_memmap` 要求区间内的页都是保留页；它把区间贪心拆成“不越界且按页号对齐的最大块”，并与已有伙伴合并。
- 已分配页的 `page_link` 分配器不使用，测试可以借用它串起暂存的块。
- 可用 `assert`、`cprintf`。panic 会停机，所以只测不会 panic 的正常与边界路径。

[GUARANTEE]

```c
void buddy_test(void);                          // 入口，buddy_pmm.h 中声明
static struct Page *expect_alloc(size_t n, struct Page *expect);
static void expect_free(struct Page *p, size_t n);
static void drain(size_t before);               // 借出全部空闲页
static void restore(void);                      // 归还借出的页
static void check_split_merge(void);
static void check_round_and_data(void);
static void check_not_buddy(void);
static void check_unaligned_init(void);
```

[SPECIFICATION]

### buddy_test

**Pre-Condition**：`pmm_init()` 完成且当前管理器是 Buddy；单核，测试期间没有其他分配。

**Post-Condition**：

- 先 `alloc_pages(64)` 取得按 64 页对齐的测试区 R，再调用 `drain` 借出其余全部空闲页。此后分配器中只剩测试区，每次分配返回的地址都可以精确预测；测试区的伙伴也不会在区外意外合并。
- 依次执行四组测试，最后归还测试区和借出的全部块，`nr_free_pages()` 恢复为测试前的值，打印 `[BUDDY] all tests passed`。

**Requirements**：封装两个辅助函数，每一步都用它们检查。

- `expect_alloc(n, expect)`：返回值等于 `expect`（`expect` 为 NULL 时表示应当失败，且空闲计数不变）；成功时首页 `property` 等于向上取整后的块长，首页不带 `PG_property` 也不是保留页，起始页号按块长对齐，空闲计数正好减少块长。
- `expect_free(p, n)`：空闲计数正好增加实际块长。

### drain / restore

**Post-Condition**：

- `drain` 从第 15 阶到第 0 阶，每阶反复分配直到失败，把块用首页的 `page_link` 串入暂存链表。借出总页数必须等于调用前的空闲计数，此时空闲计数为 0。这一步本身检验“空闲计数与链表中的块一致”。
- `restore` 逐个摘下暂存块，按首页 `property` 释放。

### check_split_merge

**Pre-Condition**：测试区的 64 页由测试持有。

**Post-Condition**：把测试区标为保留页后调用 `init_memmap(R, 64)`，空闲计数为 64。

**Case 1**：`alloc(1)` 返回 R。之后剩下的块恰好是 R+1(1)、R+2(2)、R+4(4)、R+8(8)、R+16(16)、R+32(32)，依次分配应分别返回这些地址；空闲计数为 0 时 `alloc(1)` 返回 NULL。

**Case 2**：先释放 R 和 R+1，两者应合并成 2 页块：此时 `alloc(1)` 仍返回 R，释放后 `alloc(2)` 返回 R。

**Case 3**：以打乱的顺序（32、8、2、16、4）释放其余块，最终 `alloc(64)` 返回 R。

### check_round_and_data

**Case 1**：`alloc(3)`、`alloc(5)`、`alloc(9)` 分别返回 R、R+8、R+16，块长分别为 4、8、16。5 页请求不能落在 R+4 处只有 4 页的块上。

**Case 2**：三个块分别写满不同的字节值，读回时互不覆盖。

**Case 3**：`alloc(65)`、`alloc(64)`（此时空闲 36 页，但没有 64 页的块）、`alloc((size_t)-1)` 都返回 NULL，空闲计数不变。

**Case 4**：分别按原请求页数（3、9）和实际块长（8）释放，都归还整块；最终 `alloc(64)` 返回 R。

### check_not_buddy

**Case 1**：四次 `alloc(16)` 返回 R、R+16、R+32、R+48。释放 R+16 和 R+32：两块相邻且大小相同，但不是伙伴，空闲 32 页时 `alloc(32)` 必须返回 NULL。

**Case 2**：释放 R 后它与 R+16 合并，`alloc(32)` 返回 R；释放后再释放 R+48，最终合并回 64 页块。

### check_unaligned_init

**Case 1**：测试区重新标为保留页，`init_memmap(R+3, 13)` 后空闲 13 页。`alloc(16)` 失败；`alloc(8)`、`alloc(4)`、`alloc(1)` 分别返回 R+8、R+4、R+3，之后 `alloc(1)` 失败。这说明区间被拆成了按页号对齐的 1+4+8。

**Case 2**：释放后分段注册剩余部分：`init_memmap(R, 3)` 后 `alloc(16)` 返回 R；再 `init_memmap(R+16, 48)`，`alloc(64)` 返回 R。这说明分段注册的块能跨段逐级合并。
