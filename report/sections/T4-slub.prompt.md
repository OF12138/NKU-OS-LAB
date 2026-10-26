## Challenge：SLUB 分配器（提示词一：实现并接入内核）

[PROMPT]

**任务**：在 ucore lab2 中从零实现一个简化的 SLUB 内存对象分配器，新建 `code/kern/mm/slub.h` 和 `code/kern/mm/slub.c`，并接入内核。它采用两层结构：第一层以页为单位，直接调用现有的 `alloc_pages()` / `free_pages()`（由当前 `pmm_manager` 实现，默认是 Best-Fit）；第二层在页之上实现任意大小的对象分配，对外提供 `kmem_cache_*` 系列接口和通用的 `kmalloc()` / `kfree()`。实现要体现 SLUB 的主体思想：freelist 嵌在空闲对象内部、每个缓存有一个活动 slab、只维护部分空闲（partial）链表而不跟踪满 slab、slab 变空后把页还给页分配器。

**操作要求**：你必须在项目中直接修改实际文件，而不是只展示代码片段。新建上述两个文件；在 `code/kern/init/init.c` 中 `#include <slub.h>`，并在 `pmm_init();` 之后调用 `slub_init();`。**不要**把 SLUB 注册成 `pmm_manager`，不要修改 `pmm_init()`、`init_pmm_manager()`、任何已有分配器和评测脚本：`make grade` 会检查当前 `pmm_manager` 是否为 `best_fit_pmm_manager`，测试代码也插在 `pmm_init()` 末尾。Makefile 会自动编译 `kern/mm/` 下的所有 `.c` 文件，不需要修改。用中文注释说明设计，以及与 Linux SLUB 的对应关系和简化之处。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中提供的信息，以工程中的真实代码为准。本提示词只要求实现和接入，测试函数由另一份提示词生成，这里不要编写测试。

[RELY]

```c
// code/kern/mm/pmm.h（省略注释）
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void); // number of free pages

#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)

extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;

static inline uintptr_t page2pa(struct Page *page) {
    return page2ppn(page) << PGSHIFT;
}

static inline struct Page *pa2page(uintptr_t pa) {
    if (PPN(pa) >= npage) {
        panic("pa2page called with invalid pa");
    }
    return &pages[PPN(pa) - nbase];
}
```

- `PADDR(kva)` 把内核虚拟地址换算成物理地址（`kva - va_pa_offset`）。pmm.h 中的 `KADDR` 被注释掉了，**不能使用**，物理地址转虚拟地址要自己写成 `pa + va_pa_offset`。
- 内核运行在 Sv39 分页下，所有物理内存都按固定偏移映射；`alloc_pages()` 返回的是 `struct Page *` 描述符，不是可以直接读写的地址。
- `struct Page` 只有 `ref`、`flags`、`property`、`page_link` 四个字段，都已被页分配器使用，**不要在里面存 SLUB 的元数据，也不要修改 `struct Page`**（改了会影响 page_init 的内存布局）。

```c
// code/libs/list.h（省略注释）
struct list_entry {
    struct list_entry *prev, *next;
};

typedef struct list_entry list_entry_t;

static inline void list_init(list_entry_t *elm) __attribute__((always_inline));
static inline void list_add(list_entry_t *listelm, list_entry_t *elm) __attribute__((always_inline));
static inline void list_add_before(list_entry_t *listelm, list_entry_t *elm) __attribute__((always_inline));
static inline void list_del(list_entry_t *listelm) __attribute__((always_inline));
static inline bool list_empty(list_entry_t *list) __attribute__((always_inline));
static inline list_entry_t *list_next(list_entry_t *listelm) __attribute__((always_inline));
```

```c
// code/libs/defs.h
#define to_struct(ptr, type, member)                               \
    ((type *)((char *)(ptr) - offsetof(type, member)))
// 另有 ROUNDUP(a, n)、ROUNDDOWN(a, n)，按 n 对齐，返回值类型与 a 相同

// code/kern/mm/mmu.h
#define PGSIZE          4096                    // bytes mapped by a page

// code/libs/error.h
#define E_INVAL             3   // Invalid parameter
#define E_NO_MEM            4   // Request failed due to memory shortage
```

```c
// code/kern/init/init.c（框架原始版本，修改前）
int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    dtb_init();
    cons_init();  // init the console
    const char *message = "(THU.CST) os is loading ...\0";
    //cprintf("%s\n\n", message);
    cputs(message);

    print_kerninfo();

    // grade_backtrace();
    pmm_init();  // init physical memory management

    /* do nothing */
    while (1)
        ;
}
```

- 可用 `cprintf`（支持 `%lu`、`%p`、`%-14s` 等）、`panic(fmt, ...)`（不返回，会自动换行）、`assert(x)`、`memset`。
- 默认 128 MiB 内存下，`pmm_init()` 之后约有 31900 个空闲页。

[GUARANTEE]

```c
// code/kern/mm/slub.h
struct slab;          // 每个 slab 页页首的管理头，字段自行设计，需在头文件中给出完整定义
struct kmem_cache;    // 一种对象大小的缓存，字段自行设计，需在头文件中给出完整定义（测试要读统计字段）

void slub_init(void);

struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align);
int kmem_cache_destroy(struct kmem_cache *cache);
void *kmem_cache_alloc(struct kmem_cache *cache);
void kmem_cache_free(struct kmem_cache *cache, void *obj);

void *kmalloc(size_t size);
void kfree(void *ptr);
size_t ksize(const void *ptr);

struct kmem_cache *kmalloc_cache(size_t size);  // kmalloc 处理 size 时使用的缓存；走页级分配时返回 NULL
void slub_print_stats(void);
```

`struct kmem_cache` 至少要公开以下统计字段，供测试读取：`size`（实际对象步长）、`objs_per_slab`、`nr_slabs`（当前持有的 slab 页数）、`nr_partial`（partial 链表长度）、`nr_inuse`（已分配出去的对象数）。

[SPECIFICATION]

## slub_init

**Pre-Condition**:
- `pmm_init()` 已完成，`alloc_pages()` 可用。

**Post-Condition**:
- 用一个静态分配的“缓存的缓存”（cache_cache，存放 `struct kmem_cache` 本身）自举，再用 `kmem_cache_create` 建立 kmalloc 通用缓存 16、32、64、128、256、512、1024 字节，共 7 个；打印一行初始化信息。

## kmem_cache_create / kmem_cache_destroy

**Post-Condition**:
- create：`align` 小于 8 时按 8 处理，且必须是 2 的幂；对象步长为 `size` 向上对齐到 `align`；slab 头之后的第一个对象也要满足对齐。成功时返回新缓存并挂入全局缓存链表。
- destroy：缓存中没有在用对象时，释放它持有的所有 slab 页，并把 `struct kmem_cache` 还给 cache_cache，返回 0。

  **Case 1**：`size == 0`、`align` 不是 2 的幂，或一页放不下一个对象 → create 返回 NULL。对齐计算之前先限定 `size <= PGSIZE`、归一化后的 `align < PGSIZE`，避免极大输入导致 `ROUNDUP` 或 `offset + stride` 溢出（lyp 独立测试后补充）。
  **Case 2**：destroy 时还有对象未释放，或者要销毁的是 cache_cache 或 kmalloc 通用缓存 → 返回 `-E_INVAL`，不做任何改动。

## kmem_cache_alloc

**Post-Condition**:
- 优先从活动 slab 的 freelist 头部取对象（快速路径）。活动 slab 已满时，它不挂在任何链表上；先从 partial 链表取一个 slab 作为新的活动 slab，partial 为空时再申请一页新建 slab。
- 新 slab 的页首放 slab 头，其后的对象依次串成 freelist：每个空闲对象的前 8 字节保存下一个空闲对象的地址。

  **Case 1**：页分配失败 → 返回 NULL，缓存状态不变。

## kmem_cache_free

**Post-Condition**:
- 对象地址向下对齐到页，就得到它所在的 slab 头。对象压回该 slab 的 freelist 头部。
- 若该 slab 原来是满的（不在链表上），释放后挂回 partial 链表。
- 若该 slab 不是活动 slab 且变为全空，则从链表摘下，把整页还给页分配器；活动 slab 即使变空也保留。

  **Case 1**：`obj == NULL` → 直接返回。
  **Case 2**：`obj` 不属于该缓存、不在对象边界上，或已在 freelist 上（重复释放）→ `panic`，信息中给出地址和缓存名。
  **Case 3**：每个 slab 只能放一个对象时，slab 会从满直接变空：这时它不在 partial 链表上，不能对它做 `list_del`。

## kmalloc / kfree / ksize

**Post-Condition**:
- kmalloc：`size` 不超过 1024 时，交给能容纳它的最小通用缓存；更大时直接 `alloc_pages(ROUNDUP(size + 头部, PGSIZE) / PGSIZE)`，在首页页首写入一个带不同魔数的头（记录页数），返回头部之后的地址。返回地址至少 8 字节对齐。
- kfree：读取所在页页首的魔数，区分 slab 对象和页级大块，分别归还。
- ksize：返回指针实际可用的字节数（slab 对象为缓存步长，大块为页数 × PGSIZE 减去头部）。

  **Case 1**：`kmalloc(0)` → NULL；请求超过物理内存总量 → NULL，不能因加法溢出而分配出过小的块。
  **Case 2**：`kfree(NULL)` → 直接返回；魔数不对或大块指针不是头部之后的地址 → `panic`。

**Requirements**:
- slab 元数据放在页首，不使用、不修改 `struct Page`；slab 页被释放前要清掉魔数，防止页被重新使用后仍被当成 slab。
- 每个 slab 固定占一页。单核环境下每个缓存只有一个活动 slab，不实现 per-node 链表和 min_partial。
- 接入后 `make qemu` 正常启动，`make grade` 的结果不受影响。
