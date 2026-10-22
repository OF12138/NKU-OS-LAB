# Lab 2 提示词汇总

各任务按指导书的四段式模板整理的最终版提示词。


---

<!-- prompt:T1-page-init.prompt.md -->
## 功能模块一：物理内存探测与页描述符初始化（page_init）

[PROMPT]

**任务**：在 `code/kern/mm/pmm.c` 中实现 `static void page_init(void)`：根据设备树探测到的物理内存范围，在内核镜像之后建立覆盖全部可管理物理页的 `struct Page` 描述符数组，把除 OpenSBI、内核镜像和描述符数组自身之外的完整空闲页交给当前的 `pmm_manager`，为后续的页面分配提供正确的初始状态。

**操作要求**：你必须在项目中直接修改实际文件，而不是只展示代码片段。把函数写在 `pmm.c` 中注释 `//lab2 function1 YOUR CODE` 的下方、`pmm_init()` 之前（`pmm_init()` 直接调用它，没有前置声明），并把注释中的 `YOUR CODE` 替换为学号 `2411264`，保留 `lab2 function1`。不要修改 `pmm_init()`、`init_pmm_manager()` 等现有函数和任何头文件。关键步骤写简短的中文注释。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中提供的信息，以工程中的真实代码为准。

[RELY]

```c
// code/kern/mm/memlayout.h（省略注释）
#define KERNBASE            0xFFFFFFFFC0200000
#define KMEMSIZE            0x7E00000
#define KERNTOP             (KERNBASE + KMEMSIZE)

#define PHYSICAL_MEMORY_END         0x88000000
#define PHYSICAL_MEMORY_OFFSET      0xFFFFFFFF40000000
#define KERNEL_BEGIN_PADDR          0x80200000
#define KERNEL_BEGIN_VADDR          0xFFFFFFFFC0200000

struct Page {
    int ref;                        // page frame's reference counter
    uint64_t flags;                 // array of flags that describe the status of the page frame
    unsigned int property;          // the num of free block, used in first fit pm manager
    list_entry_t page_link;         // free list link
};

#define SetPageReserved(page)       ((page)->flags |= (1UL << PG_reserved))
#define PageReserved(page)          (((page)->flags >> PG_reserved) & 1)
```

```c
// code/kern/mm/pmm.h（省略注释）
#define PADDR(kva)                                                 \
    ({                                                             \
        uintptr_t __m_kva = (uintptr_t)(kva);                      \
        if (__m_kva < KERNBASE) {                                  \
            panic("PADDR called with invalid kva %08lx", __m_kva); \
        }                                                          \
        __m_kva - va_pa_offset;                                    \
    })

extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;

static inline struct Page *pa2page(uintptr_t pa) {
    if (PPN(pa) >= npage) {
        panic("pa2page called with invalid pa");
    }
    return &pages[PPN(pa) - nbase];
}
```

```c
// code/kern/mm/pmm.c（省略注释）
struct Page *pages;
size_t npage = 0;
uint64_t va_pa_offset;
const size_t nbase = DRAM_BASE / PGSIZE;

static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

void pmm_init(void) {
    init_pmm_manager();
    page_init();
    check_alloc_page();
    ...
}
```

```c
// code/kern/driver/dtb.h
uint64_t get_memory_base(void);
uint64_t get_memory_size(void);

// code/kern/mm/mmu.h
#define PGSIZE          4096                    // bytes mapped by a page
#define PPN(la) (((uintptr_t)(la)) >> PGSHIFT)

// code/libs/riscv.h
#define DRAM_BASE          0x80000000

// code/libs/defs.h：ROUNDDOWN(a, n)、ROUNDUP(a, n) 按 n 对齐，返回值类型与 a 相同
```

```asm
# code/kern/init/entry.S：进入 kern_init 前已开启 Sv39，启动页表用一个 1 GiB 大页
# 把 VA 0xffffffffc0000000 映射到 PA 0x80000000
    li      t1, 0xffffffffc0000000 - 0x80000000
```

- 链接脚本提供符号 `end`（内核镜像结束处的虚拟地址），用 `extern char end[];` 声明。
- `panic(fmt, ...)` 打印信息后停机，会自动换行；`cprintf` 与 printf 用法相同。
- QEMU virt 默认 128 MiB 内存：设备树报告 base = 0x80000000，size = 0x8000000。0x80000000～0x80200000 是 OpenSBI。

[GUARANTEE]

```c
static void page_init(void);   // code/kern/mm/pmm.c，由 pmm_init() 调用
```

[SPECIFICATION]

## page_init

**Pre-Condition**:
- 已开启 Sv39 分页，内核运行在高地址；`dtb_init()` 已解析设备树，`get_memory_base()` / `get_memory_size()` 返回物理内存的起始地址和大小（解析失败时为 0）。
- `init_pmm_manager()` 已调用，`pmm_manager` 的空闲区为空。

**Post-Condition**:
- `va_pa_offset` 等于启动页表使用的虚拟/物理地址偏移 `PHYSICAL_MEMORY_OFFSET`。
- 输出探测到的物理内存范围，格式为：
  ```
  physical memory map:
    memory: 0x<大小>, [0x<起始>, 0x<结束-1>].
  ```
- `npage` 是可管理物理内存上界对应的物理页号；`pages` 指向内核 `end` 之后第一个页对齐处，数组共 `npage - nbase` 项，`pages[i]` 描述物理页号 `nbase + i`。
- 所有描述符先标为保留；OpenSBI、内核镜像、描述符数组本身所在的页保持保留，不进入空闲区。
- 描述符数组之后、上界之前的完整物理页恰好被 `init_memmap()` 注册一次，`nr_free_pages()` 返回这些页的数量。

  **Case 1**（默认 128 MiB）:
  - 上界为 0x88000000，`npage = 0x88000`，空闲区从描述符数组末尾向上对齐的页开始，到 0x88000000 结束。

  **Case 2**（物理内存超过可管理上界，例如 `-m 256M`）:
  - 只管理到 `KERNTOP` 对应的物理地址。注意 `KERNTOP` 是虚拟地址，必须换算成物理地址（即 `PHYSICAL_MEMORY_END`）再与物理地址比较。

  **Case 3**（设备树信息无效：大小为 0、起始地址低于 `DRAM_BASE` 或范围溢出）:
  - 调用 `panic` 终止初始化，信息中给出 base 和 size。

  **Case 4**（对齐后没有完整的空闲页）:
  - 不调用 `init_memmap()`。

**Requirements**:
- 严格区分虚拟地址（`end`、`pages`）、物理地址、物理页号和 Page 描述符地址：虚拟地址经 `PADDR` 转为物理地址，物理地址经 `pa2page` 转为描述符。
- 空闲区起点向上、终点向下对齐到页，只交出完整的页。
- 运行 `make grade` 时，`[Test 1] Page Init` 的检查项全部通过：`va_pa_offset`、`npage`、`pages` 对齐且位于 `end` 之后、空闲页数合理、内核末页和描述符数组所在页为保留状态、`pa2page(page2pa(p)) == p`。


---

<!-- prompt:T7-firstfit-challenge.prompt.md -->
# T7 反推提示词（Final Prompt）

本任务包含两个独立分析主题，分别反推对应的标准化四段式提示词。

---

## 提示词一：First-Fit 内存分配算法机理深度分析

```markdown
[PROMPT]
任务：阅读并深度分析 ucore 内核中的 First-Fit（首次适应）物理内存分配器实现（kern/mm/default_pmm.c），结合 kern/mm/memlayout.h 中的核心数据结构，输出符合操作系统实验报告技术规范的分析章节。
操作要求：紧扣内核真实代码，用严谨的技术语言阐述数据结构字段语义、链表组织模式、四个核心函数的执行流与边界保护，并对算法的局限性提出具体的工程改进思路与对比。
输出要求：Markdown 格式，结构清晰，表格字段明确，严禁使用 Emoji。

[RELY]
// 来自 kern/mm/memlayout.h
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

typedef struct {
    list_entry_t free_list;
    unsigned int nr_free;
} free_area_t;

#define PG_reserved                 0
#define PG_property                 1

#define SetPageReserved(page)       ((page)->flags |= (1UL << PG_reserved))
#define ClearPageReserved(page)     ((page)->flags &= ~(1UL << PG_reserved))
#define PageReserved(page)          (((page)->flags >> PG_reserved) & 1)
#define SetPageProperty(page)       ((page)->flags |= (1UL << PG_property))
#define ClearPageProperty(page)     ((page)->flags &= ~(1UL << PG_property))
#define PageProperty(page)          (((page)->flags >> PG_property) & 1)

#define le2page(le, member)         to_struct((le), struct Page, member)

// 来自 kern/mm/default_pmm.c
static void default_init(void);
static void default_init_memmap(struct Page *base, size_t n);
static struct Page *default_alloc_pages(size_t n);
static void default_free_pages(struct Page *base, size_t n);
static size_t default_nr_free_pages(void);

const struct pmm_manager default_pmm_manager;

[GUARANTEE]
必须交付符合实验报告要求的分析内容，明确回答并论证以下核心问题：
1. 数据结构组织：阐明 free_area_t 与 struct Page 之间的逻辑关联，明确链表只链接空闲块首页 page_link 的设计，界定 PG_property 与 property 在块首页与非首页中的取值语义；
2. 核心函数执行流：逐一解析 default_init、default_init_memmap、default_alloc_pages、default_free_pages 的算法步骤、指针操作与全局状态维护；
3. 相邻空闲块双向合并机制：详述释放内存时向物理低地址（前驱）与物理高地址（后继）合并的连续性数学判定公式（p + p->property == base 以及 base + base->property == p）；
4. 改进空间探讨：指出 First-Fit 在链表遍历时间开销 O(N) 及低地址小碎片堆积的缺陷，提出引入 Buddy System、平衡二叉树索引、Next-Fit 或边界标记法（Boundary Tag）的改进策略，并提供多维度对比矩阵。

[SPECIFICATION]
default_init():
  Pre-Condition: 系统进入物理内存管理初始化阶段
  Post-Condition: free_list 形成空循环链表，nr_free 赋值为 0

default_init_memmap(base, n):
  Pre-Condition: base 指向有效 Page 数组元素，n > 0，区间页面均处于保留状态
  Post-Condition: base->property 设为 n，SetPageProperty(base)，对应页面清空保留态，按物理地址升序插入 free_list，nr_free 累加 n
  Case 1: free_list 为空，直接作为首个节点插入
  Case 2: free_list 非空，遍历找到首个基地址大于 base 的节点前驱插入；若至末尾则追加至尾部

default_alloc_pages(n):
  Pre-Condition: n > 0
  Post-Condition: 若 n > nr_free 返回 NULL；否则返回首个满足 property >= n 的空闲块首页，更新 nr_free -= n，清除返回首页的 PG_property
  Case 1: 命中块大小等于 n，直接从链表摘除原节点
  Case 2: 命中块大小大于 n，切分原块，将剩余部分 (base + n) 设置为新块并插入原前驱之后，保持链表物理地址有序

default_free_pages(base, n):
  Pre-Condition: base 非 NULL，n > 0，待释放区间各页无 PageReserved 与 PageProperty 标记
  Post-Condition: base 重新标记为空闲块并接入链表，nr_free 累加 n，成功触发前后物理连续块的合并
  Case 1: 存在前驱相邻块（p + p->property == base），前驱扩大 property 并吞并 base
  Case 2: 存在后继相邻块（base + base->property == p），base 扩大 property 并吞并后继 p
  Case 3: 前后均不相邻，base 作为独立节点保留在有序链表中
```

---

## 提示词二：物理内存范围获取思考题（Challenge）

```markdown
[PROMPT]
任务：针对操作系统“在无法预先硬编码当前硬件物理内存范围时，如何动态获取可用物理内存”这一系统级挑战题目（Challenge），输出一篇系统性、多维度的技术原理解析报告。
操作要求：结合现代 RISC-V 体系规范（SBI / DTB）、经典 x86 体系（BIOS E820 / UEFI）、芯片组硬件机制以及裸机极限探测算法，层层递进地展开论述。
输出要求：技术概念严谨，流程逻辑完备，包含 ASCII 或流程示意图、表格横向对比，严禁使用 Emoji。

[RELY]
// 来自 kern/mm/pmm.c
static void page_init(void);
uint64_t get_memory_base(void);
uint64_t get_memory_size(void);

// 架构常量与偏移定义
#define DRAM_BASE                   0x80000000
#define PHYSICAL_MEMORY_END         0x88000000
#define PHYSICAL_MEMORY_OFFSET      0xFFFFFFFF40000000

[GUARANTEE]
必须交付包含以下技术方案与实现细节的深度论述：
1. 体系背景分析：分析物理内存为何不能静态硬编码（板级异构性、虚拟机动态配额、MMIO/保留空洞）；
2. 方案一（设备树机制）：详解 RISC-V SBI 固件（M-Mode）通过 a1 寄存器向内核传递 DTB 地址，内核解析 /memory 节点 reg 属性的完整流程；
3. 方案二（固件内存表）：剖析 x86 BIOS INT 0x15 AX=0xE820 中断及 UEFI GetMemoryMap() 传递描述符数组的标准化协议；
4. 方案三（硬件寄存器探测）：阐释内核直接与内存控制器通信、读取 DIMM SPD（I2C/SMBus）及计算内存容量的硬件级方案；
5. 方案四（裸机写-读-恢复探测与 Trap 异常截断）：详细设计无任何固件支持下的主动内存探测算法流程，重点说明测试魔数写入、内存屏障、地址回卷（Wrap-around）防御以及接管 stvec 捕获 Load/Store Access Fault 实现安全截断的机制；
6. 横向对比矩阵：构建包含依赖环境、复杂度、安全性及适用场景的综合对比表格。

[SPECIFICATION]
Manual Probing Algorithm Specification:
  Pre-Condition: CPU 处于内核态，探测起始地址置于已知安全内存边界（如内核映像尾部对齐地址）
  Post-Condition: 探测安全终止，输出可用的连续物理内存最大有效边界 max_pa，系统异常向量表完整恢复
  Requirements:
    1. 临时接管 stvec 异常向量表，设立专用的 fault recovery 上下文
    2. 执行 Write-Read-Restore 流程：暂存旧值 -> 写入魔数 0xAA55AA55 -> 内存屏障 -> 回读检验 -> 恢复旧值
    3. 校验低地址关键基准点以防止物理地址线未接通产生的回卷镜像
    4. 一旦触碰无应答总线触发 Load/Store Access Fault，Trap 处理程序优雅捕获并判定探测终止，不发生内核崩溃
```


---

<!-- prompt:T2-best-fit.prompt.md -->
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


---

<!-- prompt:T3-buddy.prompt.md -->
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


---

<!-- prompt:T3-buddy-test.prompt.md -->
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


---

<!-- prompt:T4-slub.prompt.md -->
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


---

<!-- prompt:T4-slub-test.prompt.md -->
## Challenge：SLUB 分配器（提示词二：测试函数）

[PROMPT]

**任务**：为 openfar 已实现的 SLUB 编写简洁的启动自测。参考前辈报告中“各大小缓存分配与释放、填满后扩展、批量复用、大对象页级回退”的测试思路，针对本项目的真实接口独立设计断言，不复制前辈代码。测试负责人为 2414099-李云鹏。

**操作要求**：直接修改 `code/kern/mm/slub_test.c`，提供 `slub_check()`，由 `kern_init()` 在 `slub_init()` 之后调用。保留现有实现和学习注释，不修改评分脚本、Best-Fit 原始测试及 `struct Page`。全部测试在一次正常 `make qemu` 中完成，采用 `assert` 和简短输出，不引入额外运行脚本。

**输出要求**：交付测试函数、覆盖说明和实际编译/启动/评分结果。使用 QEMU 4.1.1，默认 Best-Fit 环境即可。

[RELY]

页管理接口来自导入框架的提交 `84603fb`；SLUB 接口是提示词一已生成的被测模块，省略注释。

```c
// code/kern/mm/pmm.h
size_t nr_free_pages(void);
// code/kern/mm/mmu.h
#define PGSIZE          4096

// code/kern/mm/slub.h
#define SLUB_MIN_ALIGN      sizeof(void *)
#define SLUB_MAX_SIZE       1024
#define KMALLOC_NR_CACHES   7
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t ksize(const void *ptr);
struct kmem_cache *kmalloc_cache(size_t size);
void slub_print_stats(void);
```

- 读取现有 `slub.h` 中 `struct kmem_cache` 的 `size`、`objs_per_slab`、`nr_slabs`、`nr_partial`、`nr_inuse`，不修改这些字段。
- 七种缓存为 16～1024 字节；每个 slab 占一页。活动 slab 不在 partial 链上，变空保留；非活动满 slab 释放首个对象后进入 partial，变空则归还整页。
- 请求超过 1024 字节由 `kmalloc` 直接申请连续页，`kfree` 根据头部归还，`ksize` 返回可用容量。
- 可使用 `assert`、`cprintf`、`memset`、`ROUNDDOWN`；panic 会停机。

[GUARANTEE]

```c
void slub_check(void);
static void check_cache(size_t request, size_t expected);
static void check_reuse(void);
static void check_large(void);
```

[SPECIFICATION]

### slub_check

**Pre-Condition**：`pmm_init()`、`slub_init()` 已完成；单核，无并发分配；七个通用缓存尚未分配对象页。

**Post-Condition**：依次执行参数、各大小缓存、批量复用和大对象测试，全部成功后打印统计和完成标记。所有对象已释放，底层只比测试前少七个通用缓存保留的活动页。

**Case 1**：`kmalloc(0)` 返回 NULL，`kfree(NULL)` 可安全返回。

**Case 2**：`kmem_cache_create("overflow", (size_t)-1, 8)` 返回 NULL，保留此前已修复溢出的回归。

### check_cache

**Pre-Condition**：请求与期望缓存对应：1/17/33/65/129/257/1024 字节分别选择 16/32/64/128/256/512/1024 字节缓存；该缓存无在用对象、最多保留一个空活动页。

**Post-Condition**：按 `objs_per_slab` 填满一页，核对地址对齐、同页关系、`ksize`、在用数；额外申请一个对象，必须位于另一页。向每个对象写入不同的字节值并逐字节校验。释放非活动页第一个对象时出现 partial，释放剩余对象后整页回收；最后释放额外对象，只保留一个空活动页。

### check_reuse

**Pre-Condition**：64 字节缓存已预热且无在用对象。

**Post-Condition**：分配 128 个对象，释放偶数号对象，再补齐；在用数依次为 128、64、128，补齐时不增加 slab 或消耗新页。重新校验所有对象内容，全部释放后页数恢复至本测试开始时。

**Requirements**：指针表放 BSS，避免批量对象地址占满内核栈。

### check_large

**Pre-Condition**：页分配器正常工作。

**Post-Condition**：分别申请 1025、4096、8192 字节，覆盖小对象上限之外、恰好一页的请求和多页请求；检查返回地址、可用容量，写满请求范围并读回，释放后空闲页数恢复。页首还有管理头，不把 4096 字节请求误认为只需一页。


---

<!-- prompt:T5-walk.prompt.md -->
## 分支任务：用双重 GDB 观察 QEMU 的地址翻译与页表遍历

[PROMPT]

**任务**：在 lab2 的 ucore 上用双重 GDB 调试 QEMU 4.1.1（`--enable-debug` 编译），完成分支任务的调试要求 1、2：找出一次访存从 TCG 生成的代码走到 Sv39 页表遍历的调用路径和关键分支，演示一个虚拟地址如何被翻译成物理地址，并单步解释页表遍历的关键操作。结果写入 `report/sections/T5-walk.md`，截图放在 `report/images/T5-walk-*.png`。负责人为 2411264-张远（TLB 部分由李云鹏完成，不在本提示词范围内）。

**操作要求**：所有命令和输出都要实际运行得到，不能凭源码推测；地址以 lab2 分支最终提交的内核为准（用 `.claude/tools/sync-wsl.sh` 在 WSL 中同步后编译）。先用脚本自动跑通流程，确认断点和输出后再截图。截图使用浅色主题，画面里不能有误输入的字符。

**输出要求**：Markdown。依次给出调试流程、调用路径（调用栈截图加逐层说明表）、关键分支表、页表遍历的单步结果（变量表加计算过程）、翻译结果演示，以及调试中发现的有趣细节和借助大模型解决的问题。gdb 打印的十进制地址要换算成十六进制再解释。

[RELY]

- `code/kern/init/entry.S`（框架原始代码）：用一个 1 GiB 大页建立启动页表，写 `satp` 后执行 `sfence.vma`，再跳到高地址。

```asm
kern_entry:
    la t0, boot_hartid
    sd a0, 0(t0)
    la t0, boot_dtb
    sd a1, 0(t0)
    lui     t0, %hi(boot_page_table_sv39)
    li      t1, 0xffffffffc0000000 - 0x80000000
    sub     t0, t0, t1
    srli    t0, t0, 12
    li      t1, 8 << 60
    or      t0, t0, t1
    csrw    satp, t0
    sfence.vma
    lui sp, %hi(bootstacktop)
    lui t0, %hi(kern_init)
    addi t0, t0, %lo(kern_init)
    jr t0
...
boot_page_table_sv39:
    .zero 8 * 511
    .quad (0x80000 << 10) | 0xcf # VRWXAD
```

- `code/Makefile`：`make debug` 以 `-s -S` 启动 QEMU 并等待 gdb；`make gdb` 用 `riscv64-unknown-elf-gdb` 执行 `target remote localhost:1234`。
- QEMU 4.1.1 中需要关注的函数：`target/riscv/cpu_helper.c` 的 `get_physical_address`、`riscv_cpu_tlb_fill`、`riscv_cpu_get_phys_page_debug`；`accel/tcg/cputlb.c` 的 `get_page_addr_code`、`store_helper`、`tlb_fill`、`tlb_set_page_with_attrs`；`target/riscv/csr.c` 的 `write_satp`。
- 环境：WSL Ubuntu，宿主机 `gdb` 可以直接 `gdb -p <QEMU 的 PID>` attach（WSL2 下不需要 sudo）。

[GUARANTEE]

需要回答的问题：

1. 一次取指和一次数据写分别经过哪些 QEMU 函数才到达页表遍历？路径上哪些分支决定是否查页表、是否直接返回物理地址？
2. `get_physical_address` 每一级做了什么？lab2 的启动页表为什么只遍历一级？大页的物理地址怎样拼出来？
3. 翻译结果以什么粒度、什么权限写入 QEMU 的 TLB？
4. 调试中还观察到哪些与“软件模拟硬件”有关的现象？

需要交付的产出：调用栈截图与逐层说明表、关键分支表、页表遍历变量截图与变量表、TLB 写入截图，以及上述问题的结论。

[SPECIFICATION]

**Pre-Condition**：内核按最终提交编译；QEMU 4.1.1 带调试信息；`kern_init` 的地址用 `riscv64-unknown-elf-nm bin/kernel` 查得（本次为 `0xffffffffc02000d6`）。

**Post-Condition**：

- 启动顺序写清楚：终端 1 `make debug` → 终端 2 attach 并 `handle SIGPIPE nostop noprint`、设断点、`continue` → 终端 3 `make gdb` 并 `continue`；说明顺序颠倒为什么会连不上。
- **Case 1（取指路径）**：`break get_physical_address if addr == <kern_init 的地址>`，`bt` 得到从 `qemu_tcg_cpu_thread_fn` 经 `code_gen_buffer`、`helper_lookup_tb_ptr`、`tb_htable_lookup`、`get_page_addr_code`、`tlb_fill`、`riscv_cpu_tlb_fill` 到 `get_physical_address` 的调用栈，并解释为什么跳转目标要先翻译成物理地址（翻译块按物理地址缓存）。
- **Case 2（数据写路径）**：在 ucore 一侧停在 `kern_init` 的 `sd ra,8(sp)`，QEMU 一侧在 `riscv_cpu_tlb_fill` 上按写入地址设条件断点，`si` 后得到 `helper_le_stq_mmu → store_helper → tlb_fill` 的调用栈，并核对 `val` 与 ucore 一侧的 `$ra`。
- **Case 3（单步页表遍历）**：在读出页表项之后（第 254 行）打印 `i`、`idx`、`pte_addr`、`pte`、`base`，在计算物理地址处（第 334 行）打印 `ppn`、`ptshift`、`vpn` 并单步得到 `*physical`；逐位解释 `0xcf`，说明第一级就是叶子，以及大页时低 18 位虚拟页号如何拼进物理地址。
- **Case 4（翻译结果）**：`finish` 得到返回值 0（`TRANSLATE_SUCCESS`），在 `tlb_set_page_with_attrs` 处打印 `vaddr`、`paddr`、`prot`、`size`，说明 QEMU 只把 4 KiB 写进 TLB；结合 `satp` 解释根页表的物理地址。
- **Case 5（有趣细节）**：用“打印后继续”的断点记录启动阶段所有 S 态的 `tlb_fill`，检查 `csrw satp` 之后、`sfence.vma` 之前的取指为什么没有缺页，用源码（`write_satp`）和条件断点（`tlb_hit`）验证；另外核对 gdbstub 读内存的翻译路径，以及 `env->pc` 是否准确。

**Requirements**：关键分支表要给出源码行号和分支语句原文。截图里的数值要与正文一致；地址变化（例如代码修改导致页表地址改变）时重新实测，不要沿用旧数据。


---

<!-- prompt:T6-overall.prompt.md -->
## 实验整体逻辑分析

[PROMPT]

**任务**：为 lab2 实验报告撰写“实验整体逻辑分析”一章，写入 `report/sections/T6-overall.md`，包含“逻辑主线”“功能的逐步实现”“各功能的核心函数”三节。

**操作要求**：先通读本 lab 已经完成的代码和各任务的 section（功能模块一、功能模块二的三个任务、SLUB、分支任务），以真实代码为准，不复述指导书原文。参考 lab1 报告中“整体分析”一章的写法：先用一句话概括主线，再用表格列出实现顺序，逐条说明“为什么排在这里”。

**输出要求**：Markdown，语言平实；章节标题由集成脚本编号，文件内从二级标题开始。每个结论都要能在代码中找到依据；涉及其他章节的细节只做概括，并写明在报告中的位置。

[RELY]

- 指导书给出的实现链：`pmm_init() -> init_pmm_manager() -> page_init() -> init_memmap() -> alloc_pages()/free_pages()`。
- `code/kern/init/entry.S`：用一个 1 GiB 大页建立 `boot_page_table_sv39`，写入 `satp` 后跳到高地址的 `kern_init`。
- `code/kern/mm/pmm.h` 中的 `struct pmm_manager`（`init`、`init_memmap`、`alloc_pages`、`free_pages`、`nr_free_pages`、`check`）和 `memlayout.h` 中的 `struct Page`。
- 各分配器的实现：`default_pmm.c`、`best_fit_pmm.c`、`buddy_pmm.c`、`slub.c`，以及 `page_init`。
- 分支任务中观察到的 `get_physical_address` 页表遍历过程（`temp/gdb-guide.md`）。

[GUARANTEE]

需要回答的问题：

1. 本章围绕什么主题展开，要解决什么问题？用一句话概括主线。
2. 各功能按什么顺序实现，为什么是这个顺序？每一步依赖前面哪一步的结果？
3. 每个功能的核心函数是什么，它的关键逻辑是什么？
4. 几个页分配器之间有什么共同点，为什么可以互相替换？

需要交付的产出：分层示意图、实现顺序表、`pmm_manager` 接口表、`struct Page` 字段在各分配器中的含义对照表。

[SPECIFICATION]

**Pre-Condition**：T1～T5 的代码已经完成，各 section 已有初稿。

**Post-Condition**：

- 逻辑主线按层次组织（地址 → 内存范围 → 页描述符 → 连续页 → 对象），并点明区分虚拟地址、物理地址、物理页号和描述符地址这条副线。
- 实现顺序覆盖框架已完成的启动页表、`page_init`、First-Fit、Best-Fit、Buddy、SLUB 和分支任务，每一步都说明排序的理由。
- 核心函数的描述与代码一致，例如 Best-Fit 在大小相同时选地址最低的块，First-Fit 分配后不清零 `property`，Buddy 已分配块的首页仍记录块长。

**Requirements**：不重复各功能模块章节中的实现细节和迭代过程，只做汇总。


---

<!-- prompt:T8-common-sections.prompt.md -->
## 公共章节：实验目的、实验环境、测试与验证、AI 协作经验

[PROMPT]

**任务**：为 lab2 实验报告撰写四个公共章节：实验目的、实验环境、测试与验证、AI 协作开发的经验。

**操作要求**：直接写入 `report/sections/T8-purpose.md`、`T8-env.md`、`T8-test.md`、`T8-summary.md`。章节标题由集成脚本添加，文件中不写一级标题。所有事实（版本号、输出内容、测试结果、协作方式）必须来自真实记录：运行结果从终端复制，截图在统一环境中实拍，成员信息向本人确认；不得编写未发生的过程或未出现的输出。

**输出要求**：Markdown，语言平实，与指导书和 `report-template.md` 的对应部分一致。图片放 `report/images/`，文件名以 `T8-` 开头，图题写成 `<p align="center">图 T8-x　…</p>`。

[RELY]

- 指导书“实验目的”：理解页表的建立和使用方法；理解物理内存的管理方法；理解页面分配算法。
- `temp/env.md`：全组统一 QEMU 4.1.1（OpenSBI v0.4），原因是框架的 `-device loader` 加载方式依赖 fw_jump 固件。
- 各成员的宿主系统、工具链、AI 工具和模型（向本人确认）。
- `make grade` 的实现：`tools/inject.py` 在 `pmm_init()` 末尾插入 `test_page_init()` 和 `test_best_fit()`，`tools/grade.sh` 检查 `[Test 1] Page Init: PASS` 和 `[Test 2] Best-Fit: PASS`。

```c
// code/tools/test_lab2.c（节选）
void
test_best_fit(void)
{
    if (pmm_manager != &best_fit_pmm_manager) {
        cprintf("[Test 2] Best-Fit: FAIL (pmm_manager=%s)\n", pmm_manager->name);
        return;
    }
    cprintf("[Test 2] Best-Fit: PASS\n");
}
```

- 各模块的验证记录：`.handoff/tasks/` 中 T1、T2、T4 的「验证结果」与「迭代素材」。

[GUARANTEE]

1. 实验目的：与指导书的三条目的对应，另加一条“用四段式提示词借助 AI 完成内核开发”。
2. 实验环境：说明统一 QEMU 版本的原因；软件环境表（成员、宿主系统、QEMU、工具链）；AI 工具表（成员、工具、模型）。
3. 测试与验证：`make qemu` 与 `make grade` 的真实截图；逐行解释 `make qemu` 输出对应的启动阶段；说明 `make grade` 两项检查各自实际检查的内容；列出各模块的补充测试及结果。
4. AI 协作经验：基于本 lab 真实发生的事例总结经验。

[SPECIFICATION]

## T8-test.md

**Pre-Condition**:
- T1、T2 已完成，统一环境下 `make grade` 可以运行。

**Post-Condition**:
- 两张截图对应已提交的代码，图中输出与正文引用的字符串完全一致。

  **Case 1**：`check best fit` 只检查 `pmm_manager` 指针，正文不得把它描述成检验分配、分裂或合并。
  **Case 2**：尚未完成的模块测试（Buddy、SLUB 的提示词二）写“待补”，不得预先编写结果。

## T8-summary.md

**Post-Condition**:
- 每一条经验都对应本 lab 中可以在仓库记录里找到的事例（环境统一、参考代码中的错误、官方测试覆盖不足、说明文字与真实输出不符、`[RELY]` 校验、注释前后的编译比对）。

**Requirements**:
- 知识点与 OS 原理的对照属于 T6，本任务不写。
