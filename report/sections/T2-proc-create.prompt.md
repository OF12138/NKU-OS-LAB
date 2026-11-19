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
