# Lab 4 提示词汇总

各任务按指导书的四段式模板整理的最终版提示词。


---

<!-- prompt:T1-page-insert.prompt.md -->
# T1 虚拟内存页操作：最终技术提示词

本文件按已完成的接口设计和验证结果整理，用于复现实验功能。

## 建立、更新与撤销页映射

[PROMPT]

任务：在 lab4 的 `code/kern/mm/pmm.c` 中实现 `page_insert` 与 `page_remove`，为现有 Sv39 页表提供物理页映射维护。直接修改真实文件，保留框架及现有学习注释，将本模块标记写为 `//LAB4 function1 2414099`。仿照 `get_pte` 的函数说明格式，补充中文功能描述、parameter 参数说明和 return value 返回值说明。复用现有接口，不改评测脚本及测试函数。输出实现、处理分支的说明和实际验证结果。

[RELY]

以下摘自框架导入提交 `a7043b3`，省略注释；不引用本任务实现。`get_pte` 定位末级页表项，`create` 为真时可分配中间页表，分配失败返回 `NULL`；`page_ref_inc/dec` 返回修改后的引用计数；`pte_create` 自动设置 `PTE_V`。使用框架已有的 `tlb_invalidate` 刷新对应地址的 TLB。

`code/kern/mm/memlayout.h`：

```c
typedef uintptr_t pte_t;
typedef uintptr_t pde_t;
struct Page {
    int ref;
    uint_t flags;
    unsigned int property;
    list_entry_t page_link;
    list_entry_t pra_page_link;
    uintptr_t pra_vaddr;
};
```

`code/kern/mm/pmm.h` 中相关声明与宏：

```c
extern struct Page *pages;
extern pde_t *boot_pgdir_va;
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);
void page_remove(pde_t *pgdir, uintptr_t la);
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void tlb_invalidate(pde_t *pgdir, uintptr_t la);
```

`code/kern/mm/pmm.h` 中相关内联接口（节选）：

```c
static inline ppn_t
page2ppn(struct Page *page)
{
    return page - pages + nbase;
}
static inline struct Page *
pte2page(pte_t pte)
{
    if (!(pte & PTE_V))
    {
        panic("pte2page called with invalid pte");
    }
    return pa2page(PTE_ADDR(pte));
}
static inline int
page_ref_inc(struct Page *page)
{
    page->ref += 1;
    return page->ref;
}
static inline int
page_ref_dec(struct Page *page)
{
    page->ref -= 1;
    return page->ref;
}
static inline pte_t pte_create(uintptr_t ppn, int type)
{
    return (ppn << PTE_PPN_SHIFT) | PTE_V | type;
}
```

`code/kern/mm/mmu.h` 和 `code/libs/error.h` 中权限位与错误码：

```c
#define PTE_V     0x001
#define PTE_R     0x002
#define PTE_W     0x004
#define PTE_U     0x010
#define E_NO_MEM            4
```

[GUARANTEE]

```c
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
```

[SPECIFICATION]

`page_insert`：

- Pre-Condition：`pgdir` 是有效根页表；`page` 是已分配、可由框架管理的物理页；已有映射引用计数一致；`perm` 是调用者给出的权限组合；沿用框架单核执行条件与普通页映射约定。
- Post-Condition：成功返回 0，目标虚拟页映射到 `page`，末级 PTE 使用 `pte_create(page2ppn(page), perm)` 构造，引用计数与有效映射一致，更新后使对应 TLB 失效；定位 PTE 失败返回 `-E_NO_MEM`，不增减目标页引用、不改旧数据映射。
- Case 1：目标未映射，新增一次引用并写入 PTE。
- Case 2：目标已映射同一物理页，只更新权限，最终引用计数和空闲页数不变，不能释放该页。
- Case 3：目标映射其他物理页，移除旧映射并减少旧页引用；旧页引用归零时释放，仍有其他引用则保留；目标页新增一次引用。
- Case 4：`get_pte` 分配失败，不产生目标地址的有效末级映射。已有辅助函数可能保留已建成的中间页表，不要求修改其分配策略或回滚其他级页表。
- Requirements：不得把 `perm=0` 擅自改成读写权限；保留框架测试的 PTE 表示约定。先获得新页引用再处理旧映射，防止同页重复插入时误释放。

`page_remove`：

- Pre-Condition：根页表有效，已有映射引用计数一致。
- Post-Condition：若存在有效末级 PTE，则清零、刷新对应 TLB、减少物理页引用，并仅在引用归零时释放；若不存在有效映射则直接返回。
- Case 1：缺中间页表，不分配任何页表或数据页。
- Case 2：PTE 存在但无效，重复调用无副作用。
- Case 3：同页有多地址映射，只撤销当前映射；最后一个引用撤销后释放该物理页。
- Requirements：复用 `get_pte(pgdir, la, 0)`；不回收中间页表，避免影响其他映射；在物理页交还分配器之前清除 PTE 并使 TLB 失效。

验证：运行框架 `check_pgdir`、`check_boot_pgdir` 和未修改的官方 `test_lab4_pmm_basic`；补充实际地址访问、同页权限更新、共享页替换、分配失败、重复删除与空闲页数恢复检查。若其他任务尚未实现，在独立副本使用明确标注的启动桩验证 T1，并将模块得分与最终集成得分分别说明。


---

<!-- prompt:T2-proc-create.prompt.md -->
# T2 进程创建及初始化：最终提示词

## 创建并登记内核线程（alloc_proc / proc_init / kernel_thread / do_fork / copy_thread / copy_mm）

[PROMPT]

任务：在 lab4 的 `code/kern/process/proc.c` 中实现功能模块二的六个函数，让内核把当前启动现场包装成第 0 个内核线程 `idleproc`，并通过 `kernel_thread` 创建第 1 个内核线程 `initproc`（执行 `init_main`）。代码写在 `//LAB4 function2 YOUR CODE` 标记下方，把 `YOUR CODE` 换成学号 2411264，不要动 `//LAB4 function3` 标记及其后的内容（`proc_run`、`cpu_idle`、`schedule`、`switch_to` 由功能模块三实现）。直接修改真实文件，保留现有代码和学习注释；不修改 `tools/` 下的评测文件，也不改 `init_main` 和 `proc_init` 的函数名。每个函数前写中文注释，说明它在“首次运行链路”或“失败清理”中的作用。输出：修改内容、各函数的处理流程说明和实际验证结果。

[RELY]

以下摘自框架导入提交 `a7043b3`，省略注释。

`code/kern/process/proc.h`：

```c
enum proc_state
{
    PROC_UNINIT = 0,
    PROC_SLEEPING,
    PROC_RUNNABLE,
    PROC_ZOMBIE,
};

struct context
{
    uintptr_t ra;
    uintptr_t sp;
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};

#define PROC_NAME_LEN 15
#define MAX_PROCESS 4096
#define MAX_PID (MAX_PROCESS * 2)

struct proc_struct
{
    enum proc_state state;
    int pid;
    int runs;
    uintptr_t kstack;
    volatile bool need_resched;
    struct proc_struct *parent;
    struct mm_struct *mm;
    struct context context;
    struct trapframe *tf;
    uintptr_t pgdir;
    uint32_t flags;
    char name[PROC_NAME_LEN + 1];
    list_entry_t list_link;
    list_entry_t hash_link;
};

extern struct proc_struct *idleproc, *initproc, *current;

void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
```

`code/kern/trap/trap.h`（`pushregs` 按 x0～x31 顺序排列，这里只列用到的成员）：

```c
struct trapframe
{
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};
```

`gpr` 中有 `sp`、`s0`、`s1`、`a0` 等成员，均为 `uintptr_t`。

`code/kern/process/proc.c` 中已有的全局变量和辅助函数：

```c
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

static list_entry_t hash_list[HASH_LIST_SIZE];

struct proc_struct *idleproc = NULL;
struct proc_struct *initproc = NULL;
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);

static void
forkret(void)
{
    forkrets(current->tf);
}

static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

static int
setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}
```

另有 `static int get_pid(void)`（扫描 `proc_list`，返回一个未被使用的 pid，从 1 开始分配）、`char *set_proc_name(struct proc_struct *proc, const char *name)`，以及 `init_main`（打印 pid、名字和参数）。

`code/kern/process/entry.S` 与 `code/kern/trap/trapentry.S`：

```asm
kernel_thread_entry:
	move a0, s1
	jalr s0

	jal do_exit
```

```asm
forkrets:
    move sp, a0
    j __trapret
```

`__trapret` 执行 `RESTORE_ALL`（按 trapframe 恢复 sstatus、sepc 和全部通用寄存器，最后恢复 sp）后 `sret`。

其他头文件：

```c
#define KSTACKPAGE          2
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)
extern uintptr_t boot_pgdir_pa;
extern char bootstack[], bootstacktop[];
#define SSTATUS_SIE 0x00000002
#define SSTATUS_SPIE 0x00000020
#define SSTATUS_SPP 0x00000100
#define E_NO_MEM            4
#define E_NO_FREE_PROC      5
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);
void *kmalloc(size_t n);
void kfree(void *objp);
void wakeup_proc(struct proc_struct *proc);
```

`wakeup_proc` 断言进程当前不是 `PROC_ZOMBIE` 或 `PROC_RUNNABLE`，然后把状态置为 `PROC_RUNNABLE`。框架中没有定义 `CLONE_VM`。

[GUARANTEE]

```c
static struct proc_struct *alloc_proc(void);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
void proc_init(void);
```

六个函数都定义在 function2 标记下，按“被调用者在前”的顺序排列（`alloc_proc`、`copy_mm`、`copy_thread`、`do_fork`、`kernel_thread`、`proc_init`），不需要另加前向声明。

[SPECIFICATION]

`alloc_proc`：

- Pre-Condition：`kmalloc` 已可用（`pmm_init` 中已调用 `kmalloc_init`）。
- Post-Condition：成功时返回一个 `kmalloc` 得到的 PCB，`state = PROC_UNINIT`、`pid = -1`、`pgdir = boot_pgdir_pa`，`runs`、`kstack`、`need_resched`、`parent`、`mm`、`tf`、`flags` 为 0 或 NULL，`context` 与 `name` 全部清零，两个链表节点初始化为自指；分配失败返回 NULL。
- Requirements：逐个字段显式赋值，不能依赖 `kmalloc` 返回的内存内容。

`copy_mm`：

- Post-Condition：本实验只有内核线程，断言 `current->mm == NULL` 后直接返回 0，保留函数形态供后续实验扩展。

`copy_thread`：

- Pre-Condition：`proc->kstack` 已由 `setup_kstack` 分配；`tf` 是调用者给出的 trapframe 模板。
- Post-Condition：`proc->tf` 指向内核栈最顶端的 `sizeof(struct trapframe)` 字节，内容为 `*tf` 的副本；副本中 `gpr.a0 = 0`，`gpr.sp` 在 `esp == 0` 时设为 `proc->tf`，否则设为 `esp`；`proc->context.ra = forkret`，`proc->context.sp = proc->tf`。
- Requirements：这样设置后，`switch_to` 恢复该 context 并 `ret` 时进入 `forkret`，再经 `forkrets`、`__trapret` 按 trapframe 恢复寄存器并 `sret` 到 `tf->epc`。注释中写明这条链路。

`do_fork`：

- Pre-Condition：`current` 非 NULL；`proc_list` 与 `hash_list` 已初始化。
- Post-Condition：成功时新线程的 PCB、内核栈和 trapframe 都已就绪，`parent = current`，pid 已分配，已挂入 `hash_list` 和 `proc_list`，`nr_process` 加 1，状态为 `PROC_RUNNABLE`，返回新 pid。
- Case 1：`nr_process >= MAX_PROCESS`，不分配任何资源，返回 `-E_NO_FREE_PROC`。
- Case 2：`alloc_proc` 失败，返回 `-E_NO_MEM`。
- Case 3：`setup_kstack` 失败，`kfree` 释放 PCB 后返回 `-E_NO_MEM`。
- Case 4：`copy_mm` 失败，依次释放内核栈和 PCB 后返回 `-E_NO_MEM`。
- Requirements：失败清理用 goto 标签按分配的逆序释放资源；`get_pid`、`hash_proc`、`list_add` 和 `nr_process++` 放在同一段 `local_intr_save` / `local_intr_restore` 之间；所有资源就绪、登记完成后才调用 `wakeup_proc`，否则调度器可能选中尚未准备好的线程。

`kernel_thread`：

- Post-Condition：在局部变量中构造一个清零的 trapframe，`gpr.s0 = fn`、`gpr.s1 = arg`、`epc = kernel_thread_entry`，`status` 为当前 `sstatus` 置 `SSTATUS_SPP` 和 `SSTATUS_SPIE`、清 `SSTATUS_SIE`；然后调用 `do_fork(clone_flags, 0, &tf)` 并返回其结果。
- Requirements：注释说明三个状态位的作用（SPP=1：`sret` 后仍在 S 态；SPIE=1：`sret` 后开中断；SIE=0：`__trapret` 恢复寄存器期间不被中断）。局部 trapframe 会被 `copy_thread` 复制到新线程自己的内核栈上，函数返回后失效也没有问题。

`proc_init`：

- Pre-Condition：在 `kern_init` 中调用，此时执行流使用 entry.S 的启动栈 `bootstack`，还没有任何 PCB。
- Post-Condition：`proc_list` 和 `HASH_LIST_SIZE` 个哈希桶已初始化；`idleproc` 由 `alloc_proc` 得到，`pid = 0`、`state = PROC_RUNNABLE`、`kstack = (uintptr_t)bootstack`、`need_resched = 1`、名字为 `"idle"`，`nr_process` 加 1，`current = idleproc`；随后调用 `kernel_thread(init_main, "Hello world!!", 0)` 创建 pid 为 1 的线程，用 `find_proc` 找到它赋给 `initproc` 并命名为 `"init"`。
- Case 1：`alloc_proc` 失败时 `panic("cannot alloc idleproc.\n")`。
- Case 2：`kernel_thread` 返回值不大于 0 时 `panic("create init_main failed.\n")`。
- Requirements：`idleproc` 不经过 `do_fork`，不挂入 `proc_list`，其 `parent`、`mm`、`tf`、`runs`、`flags`、`context.ra` 保持 `alloc_proc` 给出的 0 值（评测会逐一检查）。函数末尾断言 `idleproc->pid == 0`、`initproc->pid == 1`。评测脚本会在 `proc_init` 末尾插入 `test_lab4_process_create()`，检查 `current == idleproc`、`initproc->parent == idleproc`、`initproc->state == PROC_RUNNABLE`、`initproc->context.ra` 与 `context.sp` 非 0 等条件。

验证：`make` 编译通过（此时只剩功能模块三的 `cpu_idle` 链接错误）；功能模块三尚未完成时，在独立副本中为 `cpu_idle`、`proc_run`、`schedule` 加临时桩，`make grade` 中 PMM 与 Process Create 两项应通过（20/30）。


---

<!-- prompt:T3-schedule.prompt.md -->
# T3 进程调度：最终提示词

## 调度并切换内核线程（cpu_idle / schedule / proc_run / switch_to）

[PROMPT]

任务：在 lab4 中实现功能模块三，让 `idleproc` 通过 `cpu_idle` 把 CPU 交给功能模块二创建的 `initproc`，并支持多个内核线程之间的轮转切换。共四个函数，分别写在三个文件的 `//LAB4 function3 YOUR CODE` 标记下方，并把 `YOUR CODE` 换成学号 2411264：`cpu_idle` 和 `proc_run` 写在 `kern/process/proc.c`，`schedule` 写在 `kern/schedule/sched.c`，`switch_to` 写在 `kern/process/switch.S`（RISC-V 汇编）。直接修改真实文件，保留现有代码和学习注释，不修改 `tools/` 下的评测文件。每个函数前写中文注释，说明它在切换过程中的作用，以及为什么需要关中断。输出：修改内容、切换流程说明和实际验证结果。

[RELY]

以下摘自框架导入提交 `a7043b3`，省略注释。功能模块二已经实现：`idleproc`（pid 0）是当前现场，不在 `proc_list` 中，`need_resched = 1`；其他线程由 `do_fork` 挂入 `proc_list`，状态为 `PROC_RUNNABLE`，`context.ra = forkret`、`context.sp` 指向其内核栈顶的 trapframe。

`code/kern/process/proc.h`：

```c
struct context
{
    uintptr_t ra;
    uintptr_t sp;
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};

struct proc_struct
{
    enum proc_state state;
    int pid;
    int runs;
    uintptr_t kstack;
    volatile bool need_resched;
    struct proc_struct *parent;
    struct mm_struct *mm;
    struct context context;
    struct trapframe *tf;
    uintptr_t pgdir;
    uint32_t flags;
    char name[PROC_NAME_LEN + 1];
    list_entry_t list_link;
    list_entry_t hash_link;
};

#define le2proc(le, member) \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;

void proc_run(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));
```

`code/kern/process/proc.c`、`code/kern/schedule/sched.h`：

```c
list_entry_t proc_list;
void switch_to(struct context *from, struct context *to);
void schedule(void);
```

`code/libs/list.h`：

```c
static inline list_entry_t *
list_next(list_entry_t *listelm) {
    return listelm->next;
}
```

`code/libs/riscv.h`：

```c
#if __riscv_xlen == 64
#define STORE sd
#define LOAD ld
#define LOG_REGBYTES 3
#else
#define STORE sw
#define LOAD lw
#define LOG_REGBYTES 2
#endif
#define REGBYTES (1 << LOG_REGBYTES)
#define SATP_MODE_SV39 8
#define RISCV_PGSHIFT 12
#define write_csr(reg, val) ({ asm volatile("csrw " #reg ", %0" ::"rK"(val)); })
```

`riscv.h` 中还有 `lsatp(unsigned int pgdir)`，它写的是 Sv32 格式（`SATP32_MODE | (pgdir >> RISCV_PGSHIFT)`），本实验中不可用。正确写法参照 `code/kern/mm/pmm.c`：

```c
static void enable_paging(void)
{
    write_csr(satp, 0x8000000000000000 | (boot_pgdir_pa >> RISCV_PGSHIFT));
}
```

`code/kern/mm/pmm.h` 与 `code/kern/sync/sync.h`：

```c
static inline void flush_tlb()
{
    asm volatile("sfence.vma");
}
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);
```

`__intr_save` 在 `sstatus.SIE` 为 1 时关中断并返回 1，否则返回 0；`__intr_restore(flag)` 只在 flag 为 1 时开中断，所以两对调用可以嵌套。

[GUARANTEE]

```c
void cpu_idle(void);                                       // kern/process/proc.c
void proc_run(struct proc_struct *proc);                   // kern/process/proc.c
void schedule(void);                                       // kern/schedule/sched.c
void switch_to(struct context *from, struct context *to);  // kern/process/switch.S，.globl switch_to
```

[SPECIFICATION]

`cpu_idle`：

- Post-Condition：死循环，每次检查 `current->need_resched`，为真就调用 `schedule()`，永不返回。

`schedule`：

- Pre-Condition：`current` 非 NULL；`proc_list` 中的线程状态正确。
- Post-Condition：`current->need_resched` 清 0；选出下一个 `PROC_RUNNABLE` 的线程 `next`，`next->runs` 加 1，`next` 不是 `current` 时调用 `proc_run(next)`。
- Case 1：`current` 不是 `idleproc`：从 `current->list_link` 的下一个节点开始查找，绕一圈回到 `current` 为止（`current` 自己也是候选），实现轮转。
- Case 2：`current` 是 `idleproc`：它不在链表中，从链表头 `proc_list` 开始查找。
- Case 3：整圈都没有可运行线程，选 `idleproc`。
- Requirements：遍历时跳过链表头 `proc_list`（它不属于任何 `proc_struct`，不能用 `le2proc` 转换）；循环结束后要再次检查 `next->state`，因为 `next` 可能只是最后检查过的那个不可运行线程；查找和切换整体放在 `local_intr_save` / `local_intr_restore` 之间。

`proc_run`：

- Post-Condition：`proc == current` 时什么也不做；否则关中断，令 `current = proc`，把 `proc->pgdir` 写入 `satp`，刷新 TLB，调用 `switch_to(&prev->context, &proc->context)`，返回后恢复中断。
- Requirements：`satp` 的值为 `((uintptr_t)SATP_MODE_SV39 << 60) | (pgdir >> RISCV_PGSHIFT)`，注意 `SATP_MODE_SV39` 的值是 8，必须左移到最高 4 位；不能使用 `lsatp()`。注释说明：`switch_to` 返回时已经是 prev 以后再次被调度回来，此时恢复的是 prev 自己的中断状态；新线程第一次运行不会回到 `proc_run`，而是从 `forkret` 开始，由 `__trapret` 的 `sret` 打开中断。

`switch_to`：

- Pre-Condition：`a0` 为 from，`a1` 为 to，两者都指向有效的 `struct context`。
- Post-Condition：按 `struct context` 的字段顺序，把当前的 `ra`、`sp`、`s0`～`s11` 依次存入 from（偏移为 0～13 个 `REGBYTES`），再从 to 的同样位置载入这 14 个寄存器，最后 `ret`，跳到 to 的 `ra`。
- Requirements：使用 `riscv.h` 的 `STORE`、`LOAD`、`REGBYTES` 宏（需 `#include <riscv.h>`）；只保存“被调用者保存”的寄存器，调用者保存的寄存器已由编译器在调用方处理；不包含任何调度逻辑。

验证：`make` 编译通过；`make qemu` 输出 `this initproc, pid = 1, name = "init"`、`To U: "Hello world!!".`、`To U: "en.., Bye, Bye. :)"`，之后 `init_main` 返回、进入 `do_exit` 的 `panic("process exit!!.\n")`（本实验的预期行为）；`make grade` 三项全部通过（30/30）。


---

<!-- prompt:T5-challenge.prompt.md -->
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
