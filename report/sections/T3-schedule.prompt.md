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
