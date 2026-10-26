## 功能模块二·任务二：Best-Fit 连续物理内存分配

[PROMPT]

**任务**：参考 `code/kern/mm/default_pmm.c`，直接在 `code/kern/mm/best_fit_pmm.c` 实现完整 Best-Fit 分配器，并将 `pmm.c:init_pmm_manager()` 切换为 `best_fit_pmm_manager`。

**操作要求**：保留现有代码、学习注释以及 `basic_check()`、`best_fit_check()` 的原文；保留 `lab2 function2-2` 标记，将 `YOUR CODE` 替换为 `2414099`。不要修改评测脚本、测试函数或 `struct Page`。关键步骤写简短中文注释。

**输出要求**：修改真实文件，验证编译、QEMU 4.1.1 启动及 `make grade`，报告真实结果。补充检查最小块选择、分裂、失败不改变状态、前后合并和初始化状态恢复；新增检查独立运行，不改框架测试。不执行 git commit 或 push。

[RELY]

以下来自框架导入提交 `84603fb`，省略注释；管理器各接口的完整声明以该提交的 `code/kern/mm/pmm.h` 为准。

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

typedef struct {
    list_entry_t free_list;
    unsigned int nr_free;
} free_area_t;
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

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)
static inline void set_page_ref(struct Page *page, int val) { page->ref = val; }

// code/kern/mm/best_fit_pmm.h
extern const struct pmm_manager best_fit_pmm_manager;
```

- `code/libs/list.h` 提供双向循环链表：`list_init`、`list_next`、`list_prev`、`list_add`（插在节点后）、`list_add_before`、`list_del`。
- `default_pmm.c` 展示空闲块首页记录长度、分裂与按地址合并的约定，但不能直接照搬其尾插循环：应先定位后继，再在循环外插入一次。
- `best_fit_pmm.c` 已有 `static void basic_check(void)` 和 `static void best_fit_check(void)`。它们直接访问 `free_list`、`nr_free`，还会暂存并替换二者以隔离测试；新增状态必须与此兼容。
- 调用链为 `pmm_init()` → `init_pmm_manager()` → `page_init()` → `init_memmap()` → `check_alloc_page()`；后续分配、释放与计数均经 `pmm_manager` 转发。

[GUARANTEE]

```c
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
static void best_fit_check(void);  // 框架已有，原文保留
extern const struct pmm_manager best_fit_pmm_manager;
```

只修改 `pmm.c:init_pmm_manager()` 的管理器选择，使其默认指向 Best-Fit。

[SPECIFICATION]

### best_fit_init
- **Pre-Condition**：尚未注册空闲内存。
- **Post-Condition**：`free_list` 是空的双向循环链表，`nr_free == 0`。

### best_fit_init_memmap
- **Pre-Condition**：`n > 0`；`[base, base+n)` 是合法、连续、尚未注册且均标为保留的页。
- **Post-Condition**：清除保留状态、引用计数和旧长度，注册全部 n 页并更新计数；链表按地址严格递增，只在空闲块首页设置 `PG_property` 和有效长度，相邻空闲块合并。
- **Case**：支持分段乱序注册；覆盖空链表、头插、尾插及桥接两个空闲块。

### best_fit_alloc_pages
- **Pre-Condition**：`n > 0`，链表和计数一致。
- **Post-Condition**：返回满足请求的最小空闲块的首页，只分配 n 页；同大小选地址较低者。剩余块保留原地址次序；已分配首页清除 `PG_property` 和旧长度；`nr_free` 减 n。
- **Case 1**：精确匹配可提前结束搜索；否则必须继续寻找更小的适合块。
- **Case 2**：总空闲页不足，或虽足够却没有连续 n 页，均返回 `NULL`，状态不变。

### best_fit_free_pages
- **Pre-Condition**：`n > 0`；区间合法、连续、已分配，既非保留页，也不与现有空闲块重叠。
- **Post-Condition**：逐页清除旧状态和引用计数，按地址插入，与相邻前驱、后继合并；失去首页身份的页清除标志及长度，`nr_free` 仅增加 n。
- **Case**：覆盖无合并、仅前合并、仅后合并、同时合并前后块，尤其检验尾插。
- **Requirements**：可复用内部插入/合并函数；无需改变框架元数据格式或实现任意非法指针检测。

### best_fit_nr_free_pages
- **Pre-Condition**：管理器已初始化。
- **Post-Condition**：返回全部空闲页总数，不修改状态。

### best_fit_check 与管理器注册
- **Pre-Condition**：`page_init()` 已注册可用内存。
- **Post-Condition**：`.check` 指向原有 `best_fit_check`，六个接口全部注册；框架自检后空闲块及页数恢复，`make grade` 两项均通过。
