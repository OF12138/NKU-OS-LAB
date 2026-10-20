## 练习：First-Fit 内存分配算法分析

**负责人：** 2413074-刘昀皓

### 1. 核心数据结构与状态标志

ucore 的物理内存分配以物理页帧为基本单元（大小为 4096 字节，即 `PGSIZE`）。First-Fit（首次适应）物理内存分配器在 [`code/kern/mm/default_pmm.c`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/default_pmm.c) 中实现，其状态维护依赖 [`code/kern/mm/memlayout.h`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/memlayout.h) 中定义的两个核心数据结构：[`struct Page`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/memlayout.h#L34-L39) 与 [`free_area_t`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/memlayout.h#L57-L60)。

#### 1.1 数据结构定义与关键字段说明

```c
struct Page {
    int ref;                        // 物理页的引用计数
    uint64_t flags;                 // 描述物理页状态的标志位集合
    unsigned int property;          // 连续空闲物理页块的页数（仅在连续空闲块的首页有效）
    list_entry_t page_link;         // 双向链表节点，用于串联空闲物理页块
};

typedef struct {
    list_entry_t free_list;         // 双向循环空闲链表的哨兵头节点
    unsigned int nr_free;           // 当前可用空闲物理页的总数量（以页为单位）
} free_area_t;
```

在 First-Fit 算法中，相关字段和标志位的严格语义约定如下：

| 结构体 / 字段 / 宏 | 类型 / 掩码 | 语义与生命周期约定 |
| :--- | :--- | :--- |
| [`free_area_t.free_list`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/memlayout.h#L58) | `list_entry_t` | 双向循环链表头。**链表中仅链接每个连续空闲物理块的首页（Head Page）的 `page_link`**，且所有空闲块严格按物理地址升序排列。 |
| [`free_area_t.nr_free`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/memlayout.h#L59) | `unsigned int` | 记录全局可用空闲物理页总数。在分配时扣减，在释放及初始化时累加。 |
| `PG_reserved` | `flags` 第 0 位 | 保留页标志。若为 1，表示该页保留给内核静态代码段、数据段、OpenSBI 固件或 Page 数组本身，不可用于动态分配与释放。 |
| `PG_property` | `flags` 第 1 位 | 连续空闲块首页标志。**若且仅若该 Page 是某个连续空闲物理块的首个页面时，该位置为 1**；块内部后续页面以及已被分配的页面该位必须保持为 0。 |
| [`struct Page.property`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/memlayout.h#L37) | `unsigned int` | 记录以当前 Page 为首的连续空闲页块所包含的页面总数 $N$。该字段仅在 `PG_property` 为 1 的首页有效；块内其余 $N-1$ 个页面的 `property` 均重置为 0。 |
| [`struct Page.page_link`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/memlayout.h#L38) | `list_entry_t` | 双向链表连接件。通过宏 [`le2page(le, page_link)`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/memlayout.h#L53-L54) 使用 `to_struct` 计算出宿主 [`struct Page`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/memlayout.h#L34-L39) 的起始地址。 |

#### 1.2 空闲链表组织结构图解

空闲物理内存块在逻辑上表现为由多个物理地址连续的 `Page` 构成的集合，链表仅跟踪每个集合的首节点：

```
free_area.free_list (哨兵节点)
  |
  +---> [ Page A (Head) ] <==> [ Page B (Head) ] <==> [ Page C (Head) ] <---+
  |     - property = 4         - property = 2         - property = 8         |
  |     - PG_property = 1      - PG_property = 1      - PG_property = 1      |
  |     - 物理地址 PA_A        - 物理地址 PA_B        - 物理地址 PA_C        |
  |     (物理页帧 A ~ A+3)     (物理页帧 B ~ B+1)     (物理页帧 C ~ C+7)     |
  +-------------------------------------------------------------------------+
  注：PA_A < PA_B < PA_C（按物理地址严格升序）
```

---

### 2. 四个核心函数执行流程剖析

First-Fit 分配器的完整生命周期由 [`code/kern/mm/default_pmm.c`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/default_pmm.c) 中的四个函数驱动，具体执行流如下：

#### 2.1 default_init

[`default_init`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/default_pmm.c#L11-L15) 负责分配器私有状态的初态构建：

```c
static void default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}
```

1. 调用 `list_init(&free_list)` 将空闲双向循环链表头节点的前驱和后继指针均指向自身，形成空双向链表。
2. 将全局空闲页总计数器 `nr_free` 清零。

#### 2.2 default_init_memmap

[`default_init_memmap`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/default_pmm.c#L17-L43) 在系统启动探测到物理内存区间后，由 [`page_init`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/pmm.c#L67-L111) 调用，将基地址为 `base`、长度为 $n$ 的连续可用物理页初始化为空闲块并接入管理：

```c
static void default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
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
}
```

**执行流与关键操作：**
1. **前置条件断言与单页状态重置**：验证 $n > 0$；遍历 `[base, base + n)` 内的每一个 Page，断言其处于 `PageReserved` 状态。将每个页面的 `flags` 和 `property` 置为 0，引用计数 `ref` 清零。
2. **首页属性建立**：仅将首页面 `base->property` 赋值为 $n$，并通过 `SetPageProperty(base)` 将其 `flags` 的 `PG_property` 置 1。
3. **全局计数累加**：执行 `nr_free += n`。
4. **按物理地址升序插入链表**：
   - 若 `free_list` 为空，直接调用 `list_add(&free_list, &(base->page_link))` 将块加入；
   - 若非空，沿链表顺序后移查找首个物理地址大于 `base`（即 `base < page`）的空闲块节点，调用 `list_add_before` 将其插入其前；
   - 若遍历至链表尾部仍未找到更大地址，则调用 `list_add` 追加到最后一个节点之后。

#### 2.3 default_alloc_pages

[`default_alloc_pages`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/default_pmm.c#L45-L73) 实现连续物理页的首次适应查找与切分分配：

```c
static struct Page * default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}
```

**执行流与关键操作：**
1. **容量边界防御**：断言 $n > 0$；若请求页数 $n > nr\_free$，说明系统物理空闲总额不足，立即返回 `NULL`。
2. **First-Fit 线性检索**：从 `free_list` 头部开始沿 `list_next` 顺序遍历，通过 `le2page` 取出每个空闲块首页指针 `p`。**一旦发现首个满足 `p->property >= n` 的块，立即命中**，记录 `page = p` 并终止扫描循环。
3. **节点拆除与分裂（Splitting）**：
   - 记录命中节点的前驱指针 `prev = list_prev(&(page->page_link))`。
   - 调用 `list_del(&(page->page_link))` 将原空闲块首页从 `free_list` 中剔除。
   - 若 `page->property > n`，说明该空闲块大于请求容量，需要切分：新空闲块的起始页为 `p = page + n`，设置其 `p->property = page->property - n`，置位 `SetPageProperty(p)`，并将 `p` 插入到 `prev` 之后（`list_add(prev, &(p->page_link))`），从而保持链表物理地址升序排列不变。
4. **状态结算**：更新全局剩余空闲页 `nr_free -= n`；调用 `ClearPageProperty(page)` 清除被分配块首页的空闲标记，返回基页指针 `page`。

#### 2.4 default_free_pages

[`default_free_pages`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/default_pmm.c#L75-L123) 负责将一段连续物理内存块归还给分配器，并自动完成与前后相邻物理空闲块的双向合并：

```c
static void default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

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

    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}
```

**执行流与关键操作：**
1. **归还页状态检查与清空**：断言 $n > 0$；遍历 `[base, base + n)`，断言每个页面既不是保留页（`!PageReserved`）也不是已标记的空闲头（`!PageProperty`），将其 `flags` 与 `ref` 清零。
2. **初始化归还块元数据**：设置 `base->property = n`，`SetPageProperty(base)`，累加 `nr_free += n`。
3. **有序插入链表**：遍历 `free_list`，按照物理地址升序将 `base->page_link` 插入到合适的位置（与 `default_init_memmap` 插入逻辑一致）。
4. **双向相邻合并（Coalescing）**：
   - **向前合并**：检查 `base` 的前驱节点 `le = list_prev(&(base->page_link))`。若前驱非哨兵头，将其转为 `Page *p`。若物理地址满足连续性条件 `p + p->property == base`，说明前驱空闲块的尾部与当前块紧邻。执行合并：前驱块大小扩充 `p->property += base->property`，清除 `base` 的属性位 `ClearPageProperty(base)`，将 `base` 从链表删除，并令指针 `base = p`。
   - **向后合并**：检查合并后（或未向前合并）的 `base` 的后继节点 `le = list_next(&(base->page_link))`。若后继非哨兵头且满足连续性条件 `base + base->property == p`，说明当前块尾部与后继块首部紧邻。执行合并：`base->property += p->property`，清除后继块 `ClearPageProperty(p)`，并将后继块节点从链表中删除。

---

### 3. First-Fit 算法的局限性与改进空间分析

虽然 First-Fit 算法逻辑紧凑直观，但在生产级操作系统内核中存在明显的性能与空间瓶颈：

#### 3.1 现存瓶颈分析

1. **时间复杂度瓶颈**：
   - **分配开销**：分配时需从表头顺序遍历，时间复杂度为 $O(N)$（$N$ 为空闲块数量）。在频繁分配释放后，空闲块数量激增，分配开销线性上升。
   - **释放开销**：释放时将块按地址插入链表需要 $O(N)$ 遍历，导致释放操作同样耗时。
2. **低地址碎裂与聚集效应**：
   - First-Fit 始终优先利用低地址空间，导致低地址区域频繁切分，累积大量微小的物理碎片。
   - 后续的大块分配每次都必须遍历跳过链表前端的大量碎片，进一步加剧分配延迟。
3. **缺乏尾部边界标记导致的前向合并复杂度**：
   - 当前合并依赖将节点插入链表后，通过 `list_prev` 和 `list_next` 校验邻接性。若脱离有序链表，向前合并无法在 $O(1)$ 时间内知道前一个物理页是否空闲以及其所属块的大小。

#### 3.2 优化与改进方案

1. **数据结构树形化（红黑树 / AVL 树索引）**：
   - 维护两棵平衡二叉搜索树：一棵以“空闲块大小”为键（用于 $O(\log N)$ 快速检索满足大小的块），另一棵以“物理基地址”为键（用于释放时 $O(\log N)$ 查找前驱与后继进行合并）。
2. **循环首次适应（Next-Fit）**：
   - 维护一个全局游标指针指向最近一次分配命中的位置，下次查找从该游标继续向后扫描。
   - 优点：将小碎片均匀分散到整个物理地址空间，消除低地址热点；
   - 缺点：可能将原本连续的大空闲块在循环扫描中被无差别切碎，降低大块内存分配成功率。
3. **伙伴系统（Buddy System）**：
   - 将所有物理内存按 2 的幂次方大小分级组织（$2^0, 2^1, \dots, 2^K$ 页），每个阶维护独立的空闲双向链表。
   - 分配与释放均在对应阶链表头部操作，通过位运算 `buddy = base ^ (1 << order)` 在 $O(1)$ 时间内定位伙伴块完成拆分与合并，分配与释放时间复杂度均降至 $O(1)$ 或 $O(K)$。
4. **尾部边界标记法（Boundary Tag）**：
   - 在每个连续空闲块的尾部最后一个 Page 也记录 `property` 与 `PG_property`。
   - 释放基地址为 `base` 的块时，直接读取物理相邻的前一个页 `base - 1`：若其 `PG_property` 为 1（通过尾标记），可直接获知前驱块大小并计算前驱块首地址，无需遍历链表即可在 $O(1)$ 时间内完成相邻判定。

#### 3.3 算法复杂度对比总结

| 分配算法 / 改进方案 | 分配时间复杂度 | 释放与合并复杂度 | 空间额外开销 | 外部碎片控制能力 |
| :--- | :--- | :--- | :--- | :--- |
| **标准 First-Fit (链表实现)** | $O(N)$ 线性扫描 | $O(N)$ 插入查找 | 极低（复用 Page 结构体） | 差（低地址碎片堆积严重） |
| **Next-Fit (循环首次适应)** | $O(N)$（平均较低） | $O(N)$ | 极低（仅增一个游标指针） | 中等（碎片均匀分散） |
| **Best-Fit (最佳适应链表)** | $O(N)$ 遍历全链表 | $O(N)$ 插入查找 | 极低 | 差（产生极多不可用微碎片） |
| **红黑树索引分配器** | $O(\log N)$ | $O(\log N)$ | 中等（每个块需维护红黑树指针） | 较好 |
| **Buddy System (伙伴系统)** | $O(K) \approx O(1)$ | $O(K) \approx O(1)$ | 较低（按阶链表数组） | 极优（无外部碎片，但有内部碎片） |

---

## Challenge：物理内存范围获取思考题

**负责人：** 2413074-刘昀皓

### 1. 问题背景与技术挑战

在传统操作系统设计中，物理内存范围若由内核硬编码（如预设起始地址为 `0x80000000`，大小为 `128 MiB`），将导致系统丧失移植性与动态扩展能力：
1. **硬件异构性与动态插拔**：不同板卡搭载的物理 DRAM 容量各异；在现代服务器平台中，内存支持热插拔（Memory Hotplug）。
2. **虚拟化与模拟器环境**：在 QEMU 等虚拟化平台中，用户可通过启动参数（如 `-m 1G`、`-m 512M`）任意调整分配给虚拟机的内存大小。
3. **地址空间非连续性（Memory Holes & MMIO）**：物理地址空间并非全部映射为可用 RAM，中间通常包含 PCI/PCIe MMIO 映射区、APIC 控制器寄存器、固件保留区（如 OpenSBI、BIOS SMM 空间）等物理孔洞。

若操作系统无法提前获知可用物理内存范围，必须依赖由外至内、多层次的动态探测与感知手段。

---

### 2. 物理内存范围获取的四种主流技术方案

#### 2.1 方案一：Bootloader 与固件设备树传递机制（Device Tree / FDT）

在现代 RISC-V 和 ARM 嵌入式架构中，基于设备树（Device Tree）的动态配置已成为事实上的工业标准。

```
[ Bootloader / OpenSBI (M-Mode) ]
              |
              | 1. 初始化平台硬件，探测 RAM
              | 2. 将硬件拓扑编码为 DTB 二进制
              | 3. 将 DTB 物理地址写入 a1 寄存器
              v
[ ucore 内核入口 (S-Mode) ]
              |
              | 读取 a1 寄存器，获取 DTB 基地址
              v
[ DTB 解析模块 (dtb.c) ]
              |
              | 遍历 "/memory" 节点，读取 "reg" 属性
              v
得到可用物理内存区间：[mem_begin, mem_begin + mem_size]
```

**实现机制与调用链：**
1. **约定传参**：根据 RISC-V SBI 规范，在固件完成硬件初始化并跳转至内核入口时，将通用寄存器 `a0` 存入当前 CPU 的 Hart ID，将寄存器 `a1` 存入设备树二进制文件（Device Tree Blob, DTB）在物理内存中的基地址指针。
2. **内核解析**：内核在早期引导阶段（如 [`code/kern/mm/pmm.c:page_init`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/pmm.c#L67-L111)）调用 `get_memory_base()` 和 `get_memory_size()`。
3. **节点检索**：底层驱动解析 FDT 格式数据，遍历根路径下的 `/memory` 节点，提取其中的 `reg` 属性。该属性包含一组或多组 `(base, size)` 元组，精确声明了物理 RAM 的起始基地址和有效长度。
4. **范围截断与过滤**：内核读取后与虚拟映射范围上限（如 `PHYSICAL_MEMORY_END = 0x88000000`）取交集，并剔除自身镜像与固件占据的区间。

#### 2.2 方案二：固件标准化服务接口与内存映射表（BIOS E820 / UEFI）

在 x86 体系结构中，固件通过标准化的系统服务调用向内核提交详细的物理内存布局表：

1. **Legacy BIOS (INT 0x15, AX=0xE820)**：
   - 引导加载程序（Bootloader）在实模式下循环触发软中断 `INT 0x15`（输入 `EAX=0xE820`），由 BIOS 返回一系列 `AddressRangeDescriptor` 结构体。
   - 每个结构体包含 64 位基地址、64 位长度以及类型标识（如 `1: AddressRangeMemory` 代表可用 RAM，`2: AddressRangeReserved` 代表系统保留，`3: ACPI Reclaim Memory` 代表 ACPI 表区）。
   - Bootloader 将该内存映射表封装进 `multiboot_info` 数据结构后传递给内核。

2. **UEFI (Unified Extensible Firmware Interface)**：
   - 内核在尚未退出 UEFI 引导服务（Boot Services）之前，调用 `GetMemoryMap()` API。
   - 固件返回一组 `EFI_MEMORY_DESCRIPTOR` 数组，清晰划分物理地址、虚拟地址、页数以及内存属性（如 `EfiConventionalMemory` 代表操作系统可自由分配的物理内存，`EfiBootServicesCode` 在内核接管后可被回收释放）。

#### 2.3 方案三：硬件芯片组与总线直接探测（Hardware Register Probing）

在无固件支持或操作系统作为一级引导器的特殊裸机体系中，内核可直接与主板芯片组及内存控制器硬件通信：

1. **内存控制器寄存器读取**：
   - 北桥或 SoC 内部的内存控制器（Memory Controller Hub）通常包含专门的寄存器（如 DRAM Rank Configuration Register），记录了每个插槽（Channel/DIMM）中内存芯片的行地址、列地址宽度以及芯片排数（Rank）。
   - 内核直接读取这些映射在 MMIO 或配置空间的硬件寄存器，通过公式计算出实际连接的 DRAM 容量：
     $$\text{Capacity} = 2^{(\text{Row} + \text{Column})} \times \text{Bank} \times \text{DataWidth} \times \text{Ranks}$$
2. **I2C / SMBus 上的 SPD (Serial Presence Detect) 查询**：
   - 内存条上的 EEPROM 芯片通过 SMBus 串行总线与主板相连，固化了 JEDEC 标准定义的 SPD 数据。
   - 内核驱动可通过主板 SMBus 控制器直接发送 I2C 读指令，逐字节读取 SPD 内容，获取物理内存条的制造规范、容量与时序信息。

#### 2.4 方案四：裸机极限探测（Manual Probing）与 Trap 异常截断机制

在完全缺乏固件传参、无设备树、也无法直接读取内存控制器的极端裸机环境下，内核必须采用“主动内存探测算法”。该方案必须兼顾**破坏性规避**、**地址回卷防护**以及**总线硬件异常截断**：

```
                   [ 开始裸机探测循环 ]
                             |
                   probe_pa = DRAM_BASE (0x80000000)
                   step = 1 MiB (或 4 KiB)
                             |
             +-------------->+
             |               |
             |     [ 安装临时 Trap 向量表 ] (捕获 Load/Store Fault)
             |               |
             |     1. 保存原内存值: old_val = *probe_pa
             |     2. 写入测试魔数: *probe_pa = 0xAA55AA55
             |     3. 强制内存屏障: fence / mb()
             |     4. 回读并校验:   read_val = *probe_pa
             |               |
        发生硬件异常? -------+----是----> [ 触发 Trap: 达到物理内存边界 ]
             | 否                                        |
      read_val == 魔数?                                  |
        /          \                                     |
      否            是                                   |
      /              \                                   v
[ 探测终止 ]    5. 写入反转测试魔数: 0x55AA55AA        记录有效内存上界
(检测到非RAM)   6. 回读校验防地址回卷 (Aliasing)       max_pa = probe_pa
                     |                                   |
                7. 恢复原数据: *probe_pa = old_val       v
                     |                             [ 恢复正常系统异常表 ]
                probe_pa += step                         |
                     |                             [ 探测完成，进入分配器初始化 ]
                     +-----------------------------------+
```

##### 详细技术实现流程与关键陷阱防御

1. **写-读-恢复（Write-Read-Restore）测试**：
   - **操作步骤**：探测代码以物理页（`4 KiB`）或块（`1 MiB`）为步长递增探测指针 `probe_pa`。对每个地址，先读取并暂存原内容 `old_val`；写入特定特征魔数（如 `0xAA55AA55`）；执行内存屏障指令（RISC-V 的 `fence`，x86 的 `mfence`），确保写操作提交至物理总线；随后回读。若回读值等于魔数，再写入反转特征值（如 `0x55AA55AA`）二次检验。全部校验成功后，将 `old_val` 还原写回。
2. **总线地址回卷（Address Aliasing / Wrap-around）检测**：
   - **风险场景**：若硬件系统仅具备 32 MiB 物理内存（对应 25 根物理地址线 $A_0 \sim A_{24}$），当探测地址递增至 32 MiB 以上（$A_{25}$ 悬空或未连接）时，地址总线的高位被硬件忽略，对 32 MiB 处的写入实际上会篡改并回卷到 0 MiB 处的内存单元。
   - **防护机制**：在检测新地址 `probe_pa` 时，同时回读已知低地址基准点（如 `DRAM_BASE`），确保低地址处预置的内容未被意外覆盖；若发现低地址发生同步改变，说明地址线已发生回卷，立即终止探测。
3. **硬件 Trap / 异常捕获截断（Fault Interception）**：
   - **风险场景**：在具备现代系统总线（如 AXI / TileLink）的硬件平台上，向未挂载任何物理设备的空白地址区间发起读取或写入时，总线仲裁器无法获取从机应答（Slave Acknowledge），会向 CPU 核心上报硬件总线错误（Bus Error）。在 RISC-V 体系下将立即触发 **Store Access Fault（异常编号 7）** 或 **Load Access Fault（异常编号 5）**；若无保护将导致内核直接 Panic 崩溃。
   - **异常接管实现机制**：
     - 内核在进入探测循环前，临时重定向 `stvec` 寄存器至探测专用的异常处理入口（`probe_trap_entry`）。
     - 探测代码使用类似 C 语言 `setjmp/longjmp` 的跳转上下文机制。
     - 一旦触发 Access Fault，CPU 硬件自动打断执行流，保存现场并跳转至 `probe_trap_entry`。
     - 异常处理程序识别出当前处于物理内存探测阶段，主动修改 Trap 上下文中的 `sepc`，跳出探测循环，并将当前 `probe_pa` 认定为硬件物理内存的不可跨越边界。
     - 探测结束后，恢复内核常规的中断/异常向量表 `stvec`。

---

### 3. 四种探测机制的维度对比

| 探测方式 | 依赖环境 | 实现复杂度 | 健壮性与安全性 | 典型应用平台 |
| :--- | :--- | :--- | :--- | :--- |
| **设备树 (Device Tree)** | 依赖 Bootloader / 固件传入 DTB | 低（调用树解析库即可） | 极高（由固件保证准确性） | 现代 RISC-V、ARM64 Linux / ucore |
| **固件表 (E820 / UEFI)** | 依赖 x86 BIOS 或 UEFI 运行时 | 中等（需解析描述符数组） | 极高（工业界成熟规范） | 现代 PC、x86 服务器 |
| **硬件控制器探测** | 依赖特定 SoC / 芯片组驱动 | 高（需了解内存控制器寄存器规范） | 高（硬件精确数据） | 深度定制嵌入式系统、BIOS/固件早期阶段 |
| **裸机写-读-恢复探测** | 无任何外部依赖（纯裸机） | 高（需编写异常处理与防回卷算法） | 较低（易触碰不可预知的总线异常或破坏保留数据） | 教学微内核、古老硬件或极简引导系统 |
