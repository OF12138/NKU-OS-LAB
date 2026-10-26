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
