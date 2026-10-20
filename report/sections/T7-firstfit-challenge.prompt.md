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
