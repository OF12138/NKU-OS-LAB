## 功能模块二·任务三：Buddy System（提示词一，实现与接入）

[PROMPT]

**任务**：直接创建 `code/kern/mm/buddy_pmm.h`、`buddy_pmm.c`，实现按阶空闲链表组织的伙伴分配器，并在 `pmm.c`、`Makefile` 接入。复用现有 Page 描述符和链表，不另建大树数组，不依赖尚未启动的动态内存分配器。

**操作要求**：保留现有代码、用户学习注释及框架测试，禁止修改评分脚本、`struct Page`、page_init。默认仍用 Best-Fit；`make PMM=buddy` 选择 Buddy，切换前后无需手工 clean。新增代码写简短中文注释，标明 `lab2 function2-3 2414099`；不 commit / push。

**输出要求**：实现真实文件，附必要启动自检、简洁报告和验证输出；QEMU 使用 4.1.1。报告参照 `T4-slub.md` 的组织方式，说明按阶链表、初始化、分裂与合并，用一个例子解释整块分配/释放；复杂度须计入释放时的逐页清理。迭代过程只写实际问题与调整，验证部分简述 Buddy 启动、自检和默认功能回归，不安排 SLUB 冒烟测试。独立测试和提示词二按分工交给 openfar，不能把本轮实现自检写成已经完成的独立测试。

[RELY]

以下代码摘自框架导入提交 `84603fb`，省略注释。

```c
// code/kern/mm/memlayout.h
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};
#define PG_reserved                 0
#define PG_property                 1
#define SetPageReserved(page)       ((page)->flags |= (1UL << PG_reserved))
#define PageReserved(page)          (((page)->flags >> PG_reserved) & 1)
#define SetPageProperty(page)       ((page)->flags |= (1UL << PG_property))
#define ClearPageProperty(page)     ((page)->flags &= ~(1UL << PG_property))
#define PageProperty(page)          (((page)->flags >> PG_property) & 1)
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)
#define PHYSICAL_MEMORY_END         0x88000000
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
extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
static inline ppn_t page2ppn(struct Page *page) { return page - pages + nbase; }
static inline uintptr_t page2pa(struct Page *page) {
    return page2ppn(page) << PGSHIFT;
}
static inline void set_page_ref(struct Page *page, int val) { page->ref = val; }
```

- `Page` 数组覆盖物理页号 `[nbase,npage)`；页大小为 4096 字节。启动空闲区包含非 2 的幂大小、不按高阶块对齐的区间，且保留页不得分配。
- `code/libs/list.h` 提供双向循环链表、`list_init`、`list_next`、`list_add`、`list_del`、`list_empty`。
- 页分配接口允许返回至少 n 页；Buddy 可以占用向上取整后的整块。现有 SLUB 的大对象路径按原请求页数记录并调用 `free_pages(base,n)`，需与该调用兼容。
- `init_pmm_manager()` 选管理器并调用 `.init`；page_init 通过 `.init_memmap` 注册可用区；`check_alloc_page()` 调用 `.check`。
- 默认管理器在 T2 后为 Best-Fit；评分测试明确检查其指针。Makefile 自动编译 `kern/mm/*.c`，现有通用规则不跟踪命令行编译选项变化。

[GUARANTEE]

```c
static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);
extern const struct pmm_manager buddy_pmm_manager;
```

头文件公开 `BUDDY_MAX_ORDER` 和分配/释放约定；`init_pmm_manager()` 支持 `PMM_buddy` 宏，Makefile 的 `PMM` 只允许 `best_fit`、`buddy`。

[SPECIFICATION]

### buddy_init
- **Pre-Condition**：尚未注册可用物理页。
- **Post-Condition**：0～15 阶链表均为空，空闲页计数为 0；15 阶对应 32768 页，覆盖当前 128 MiB 上限。

### buddy_init_memmap
- **Pre-Condition**：n > 0，区间在描述符数组内、页均保留，且与已注册区间不重叠。
- **Post-Condition**：清除保留状态、旧长度与引用计数；按物理页号对齐和剩余长度分解为 2 的幂块，注册全部 n 页；同阶伙伴尽量合并，计数增加 n。
- **Case**：非幂长度、未按大块对齐的起点、单页尾部、分段及乱序注册均不能丢页；不得跨越保留页或数组边界。

### buddy_alloc_pages
- **Pre-Condition**：n > 0，空闲链表与计数一致。
- **Post-Condition**：将 n 向上取整为 S=2^k，从首个非空的 k 阶或更高阶取块，逐级二分；返回按物理页号对齐的连续 S 页，计数减 S。返回首页 `property=S` 且 `PG_property=0`，非首页长度为 0。
- **Case**：没有合适块时返回 NULL，状态不变；超大 n 在取整溢出前失败。

### buddy_free_pages
- **Pre-Condition**：base 是本管理器返回的未释放整块起点；传入原请求 n 或实际页数 S，禁止部分释放。
- **Post-Condition**：校验取整后的阶、块长、物理对齐、范围及页状态；清零实际 S 页的旧状态和引用计数，计数加 S；按 `PFN XOR S` 计算伙伴，仅在伙伴空闲且同阶时递归合并。
- **Requirements**：只有空闲首页设置 PG_property，首页 property 记录整块实际页数；边界判断先于伙伴描述符访问。无需承诺检测所有错误调用，但禁止把块内非首页当成独立分配释放。

### buddy_nr_free_pages
- **Pre-Condition**：管理器已初始化。
- **Post-Condition**：返回实际空闲页数，不改变状态；内部碎片同样属于已分配块，不计为空闲。

### buddy_check 与接入
- **Pre-Condition**：启动内存已注册，当前管理器为 Buddy，足以完成少量冒烟分配。
- **Post-Condition**：检查各阶块大小、对齐、链表、页状态、计数和充分合并；验证 1/3/9 页请求、超大请求失败及连续可写；释放后总页数和各阶块数恢复。
- **Requirements**：注册六个接口。验证 Buddy 启动与自检；不 clean 切回默认后运行 Best-Fit，`make grade` 保持基线 20/20。独立测试仍由 openfar 完成。
