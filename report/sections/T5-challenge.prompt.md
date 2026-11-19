## Challenge：开关中断机制与多级页表设计剖析

[PROMPT]

**任务**：针对操作系统 Lab 4 指导书「扩展练习 Challenge」中的两道原理与架构分析题，结合当前仓库真实源码与 RISC-V 特权架构规范，撰写深入、严谨的 Markdown 解析报告，作为实验报告中 `## Challenge` 小节的交付内容。

**操作要求**：直接依据 `code/` 目录下的真实代码、函数签名与精确行号开展分析，不得虚构未在仓库中出现的符号或机制；结合已实现的 `schedule()`、`proc_run()`、`do_fork()`、`switch_to()` 与 `get_pte()` 源码展开实证剖析。交付文本严禁使用 Emoji。

**输出要求**：输出格式严格符合 `report-template.md` 中 Challenge 章节要求，分小题依次给出源代码链路、硬件/数据结构对比分析、场景推演以及客观工程评价。

[RELY]

```c
// code/kern/sync/sync.h
static inline bool __intr_save(void)
{
    if (read_csr(sstatus) & SSTATUS_SIE)
    {
        intr_disable();
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag)
{
    if (flag) intr_enable();
}

#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);
```

```c
// code/libs/riscv.h & code/kern/driver/intr.c
#define SSTATUS_SIE 0x00000002
#define read_csr(reg) ({ unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })
#define set_csr(reg, bit) ({ unsigned long __tmp; \
  asm volatile ("csrrs %0, " #reg ", %1" : "=r"(__tmp) : "rK"(bit)); \
  __tmp; })
#define clear_csr(reg, bit) ({ unsigned long __tmp; \
  asm volatile ("csrrc %0, " #reg ", %1" : "=r"(__tmp) : "rK"(bit)); \
  __tmp; })

void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
```

```c
// code/kern/mm/pmm.c
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    pde_t *pdep1 = &pgdir[PDX1(la)];
    if (!(*pdep1 & PTE_V))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    pde_t *pdep0 = &((pte_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
    if (!(*pdep0 & PTE_V))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}
```

```c
// code/kern/mm/mmu.h & code/kern/process/proc.h
#define PDX1(la) ((((uintptr_t)(la)) >> PDX1SHIFT) & 0x1FF)
#define PDX0(la) ((((uintptr_t)(la)) >> PDX0SHIFT) & 0x1FF)
#define PTX(la) ((((uintptr_t)(la)) >> PTXSHIFT) & 0x1FF)
#define PTE_ADDR(pte)   (((uintptr_t)(pte) & ~0x3FF) << (PTXSHIFT - PTE_PPN_SHIFT))
#define PDE_ADDR(pde)   PTE_ADDR(pde)
#define PTE_V     0x001
#define PTE_U     0x010

extern list_entry_t proc_list;
extern struct proc_struct *idleproc, *initproc, *current;
void proc_run(struct proc_struct *proc);
```

```c
// code/kern/schedule/sched.c & code/kern/process/proc.c
void schedule(void)
{
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;
        do
        {
            if ((le = list_next(le)) != &proc_list)
            {
                next = le2proc(le, list_link);
                if (next->state == PROC_RUNNABLE)
                {
                    break;
                }
            }
        } while (le != last);
        if (next == NULL || next->state != PROC_RUNNABLE) next = idleproc;

        next->runs++;
        if (next != current) proc_run(next);
    }
    local_intr_restore(intr_flag);
}

void proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag);
        {
            current = next;
            write_csr(satp, ((uintptr_t)SATP_MODE_SV39 << 60) | (next->pgdir >> RISCV_PGSHIFT));
            flush_tlb();
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}
```

[GUARANTEE]

必须完整、逐条回答 Lab 4 指导书「扩展练习 Challenge」的两部分全部问题：
1. Challenge 1：解释 `local_intr_save(intr_flag); ... local_intr_restore(intr_flag);` 实现开关中断的完整机制，并论证本实验中 `schedule` 与 `proc_run` 为什么需要该保护。
2. Challenge 2：结合 Sv32、Sv39、Sv48 的异同解释 `get_pte()` 中两段代码高度相似的原因，并辩证评估将页表项查找与页表页分配合并在 `get_pte()` 中的优缺点及拆分必要性。

[SPECIFICATION]

针对 Challenge 1（开关中断与调度保护）：
- Case 1（底层机制）：逐层说明 `local_intr_save/restore` 宏、`__intr_save/__intr_restore` 内联函数、`read_csr/set_csr/clear_csr(sstatus, SSTATUS_SIE)` 汇编映射；重点阐述为什么保存原 `SIE` 位并仅在 `flag == 1` 时恢复，能够保证嵌套临界区与中断上下文中的安全。
- Case 2（调度保护必要性）：结合 `proc_list`、`current`、`satp` 页表切换和 `switch_to` 上下文切换，推导异步时钟中断打断 `schedule` 或 `proc_run` 时导致链表状态错乱或“进程身份-页表-内核栈”三元撕裂的危害。

针对 Challenge 2（分页模式与 `get_pte()` 设计评估）：
- Case 1（Sv32 / Sv39 / Sv48 对比）：使用结构化表格对比三者在地址位宽、页表级数、VPN 切片位数、PTE 大小、共享的 4 KiB 页粒度及一致的 PTE 标志位编码上的异同；从基数树自相似性与 $L - 1$ 次中间目录降级步进解释为何 Sv39 的 `get_pte()` 出现两段同构代码。
- Case 2（合并 vs 拆分评价）：先肯定合并写法在消除重复索引遍历与单次建立映射上的紧凑优势；再指出违反单一职责、`NULL` 返回值掩盖“未映射 vs OOM”差异、三级创建中途 OOM 时的空目录页残留及硬编码权限位等问题；最后给出“当前教学框架保留兼容、工程级内核拆分并参数化重构”的明确结论。
