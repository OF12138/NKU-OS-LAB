## 功能模块三：进程调度

**负责人：** 2411264-张远（openfar）

### 模块功能描述

**需要实现的函数：**

```c
void cpu_idle(void);                                       // kern/process/proc.c
void schedule(void);                                       // kern/schedule/sched.c
void proc_run(struct proc_struct *proc);                   // kern/process/proc.c
void switch_to(struct context *from, struct context *to);  // kern/process/switch.S
```

**功能说明：**

功能模块二执行完以后，系统里已经有 `idleproc` 和 `initproc` 两个线程，但 CPU 仍在执行 `idleproc`，也就是 `kern_init` 的启动现场。本模块负责把 CPU 真正交给其他线程。四个函数从上到下逐层调用：`cpu_idle` 决定什么时候调度，`schedule` 决定调度给谁，`proc_run` 切换 `current` 和页表，`switch_to` 切换寄存器。

`cpu_idle` 是 `kern_init` 的最后一步，也是 `idleproc` 的函数体：一个死循环，每次检查 `current->need_resched`，为真就调用 `schedule()`。本实验中这是唯一的调度点。`proc_init` 已经把 `idleproc->need_resched` 置 1，所以进入 `cpu_idle` 后会立即调度到 `initproc`。

`schedule` 是一个简单的 FIFO/轮转调度器。它先把 `current->need_resched` 清 0，然后在 `proc_list` 中查找下一个状态为 `PROC_RUNNABLE` 的线程：

| 情况 | 查找方式 |
|---|---|
| `current` 不是 `idleproc` | 从 `current` 在链表中的下一个位置开始，绕一圈回到 `current` 为止，实现轮转；`current` 自己也是候选，只剩它可运行时会选回它 |
| `current` 是 `idleproc` | `idleproc` 不在 `proc_list` 中，从链表头开始查找 |
| 绕完一圈都没有可运行线程 | 选 `idleproc` 保底 |

遍历时要跳过链表头 `proc_list`：它是一个独立的哨兵节点，不嵌在任何 `proc_struct` 中，不能用 `le2proc` 转换。循环正常结束时，`next` 只是最后检查过的那个线程，未必可运行，所以循环后还要再检查一次它的状态。选出 `next` 后，把 `next->runs` 加 1，`next` 与 `current` 不同时调用 `proc_run(next)`。整个查找和切换过程都在关中断状态下进行，避免遍历链表时被时钟中断打断。

`proc_run` 在关中断的情况下完成三件事：

1. `current = proc`；
2. 把 `proc->pgdir` 写入 `satp`，再用 `flush_tlb()`（`sfence.vma`）刷新 TLB。`satp` 的 MODE 字段在最高 4 位，`riscv.h` 中的 `SATP_MODE_SV39` 值为 8，要左移 60 位再与根页表的物理页号 `pgdir >> 12` 合并，写法与 `pmm.c` 的 `enable_paging()` 相同。`riscv.h` 里的 `lsatp()` 是 Sv32 的写法，MODE 位在第 31 位，参数也只有 32 位，不能用。本实验中所有线程的 `pgdir` 都是 `boot_pgdir_pa`，所以这一步实际上不会改变映射；到 lab5 每个用户进程有自己的页表时，这一步才真正起作用；
3. 调用 `switch_to(&prev->context, &proc->context)` 切换寄存器。

如果 `current` 已经改了、页表也换了，但寄存器还是旧线程的，这时发生中断，中断处理程序看到的状态就不一致，所以这三步必须关中断。`proc_run` 中的 `switch_to` 调用要过很久才会“返回”：只有当 prev 以后再次被调度回来时，它才从 `switch_to` 返回，接着执行 `local_intr_restore`，恢复的是 prev 自己切走前的中断状态。新线程第一次运行时不会回到 `proc_run`，而是从 `forkret` 开始，中断由 `__trapret` 的 `sret` 根据 `SPIE` 打开。

`switch_to` 用汇编实现。`a0` 指向旧线程的 `context`，`a1` 指向新线程的 `context`。它按 `struct context` 的字段顺序，把当前的 `ra`、`sp`、`s0`～`s11` 存入 `a0` 指向的结构（偏移依次为 0～13 个 `REGBYTES`），再从 `a1` 的同样位置载入这 14 个寄存器，最后执行 `ret`。只保存“被调用者保存”的寄存器就够了：对编译器来说，`switch_to` 是一次普通的函数调用，`t0`～`t6`、`a0`～`a7` 等调用者保存的寄存器在调用前已由调用方自行处理。`ret` 跳到的是新线程的 `ra`：

- 新线程第一次运行：`ra` 是功能模块二的 `copy_thread` 设置的 `forkret`，之后经 `forkrets`、`__trapret` 进入 `kernel_thread_entry`，执行线程函数；
- 以前被切走的线程：`ra` 是它当初在 `proc_run` 中调用 `switch_to` 的返回地址，于是从 `proc_run` 继续执行，再逐层返回到 `schedule` 和调用 `schedule` 的地方。

以 `idleproc` 切换到 `initproc` 为例：`cpu_idle` → `schedule` 从链表头开始，找到唯一可运行的 `initproc` → `proc_run` 令 `current = initproc` → `switch_to` 把 `idleproc` 的寄存器存入 `idleproc->context`，载入 `initproc->context`，`ret` 到 `forkret` → `initproc` 开始执行 `init_main`。

评测在 `init_main` 开头插入 `test_lab4_schedule()`：它在 `initproc` 中再创建两个线程，三者都反复设置 `need_resched = 1` 并主动调用 `schedule()`，检查两个新线程都被轮转执行到，然后 `init_main` 打印三行字符串。`init_main` 返回后进入 `do_exit`，本实验的 `do_exit` 只有一句 `panic("process exit!!.\n")`，进程退出要到 lab5 才实现，所以最后出现这个 panic 是预期行为。

### 最终提示词

<!-- include:T3-schedule.prompt.md -->

### 实现迭代过程

本模块经过一轮生成前的核对和一轮完整验证，生成的代码一次通过，没有再修改。

#### 第一次迭代：生成前核对框架

**遇到的问题：**
- 指导书说明 `riscv.h` 中的 `lsatp()` 不可用。查看 `riscv.h` 后确认：它写入的是 Sv32 格式（`SATP32_MODE`，即第 31 位），参数类型是 `unsigned int`，64 位物理地址会被截断。同一文件中的 `SATP_MODE_SV39` 定义为 8，是 MODE 字段的取值，并没有移到字段所在的位置，直接与物理页号相或会得到错误的 `satp`。
- `switch.S` 在框架中只有一行标记，没有任何 `#include`，而保存寄存器要用的 `STORE`、`LOAD`、`REGBYTES` 宏定义在 `riscv.h` 中。

**问题解决策略：**
- 在提示词的 [RELY] 中给出 `lsatp()` 不可用的原因，以及 `enable_paging()` 的正确写法；在 [SPECIFICATION] 中明确 `satp` 的取值为 `((uintptr_t)SATP_MODE_SV39 << 60) | (pgdir >> RISCV_PGSHIFT)`。
- 在 [SPECIFICATION] 中写明 `switch.S` 需要 `#include <riscv.h>`，并给出 14 个寄存器的偏移。

#### 第二次迭代：完整工程验证

功能模块一、二已经完成，这是 lab4 第一次不加任何临时桩、完整链接整个工程。

**最终结果：**

- ✅ `make` 编译、链接通过；
- ✅ `make qemu` 依次输出 `this initproc, pid = 1, name = "init"`、`To U: "Hello world!!".`、`To U: "en.., Bye, Bye. :)"`，随后出现预期的 `process exit!!` panic；
- ✅ `make grade`：PMM、Process Create、Schedule 三项全部通过，30/30。

**关键改进点总结：**
1. [RELY] 中写明 `lsatp()` 为什么不能用，并给出 `enable_paging()` 作为正确写法，避免 AI 直接调用 `lsatp()` 或把 `SATP_MODE_SV39` 当成已经移好位的掩码。
2. [SPECIFICATION] 中写明 `schedule` 循环结束后要再检查一次 `next` 的状态，并跳过链表头哨兵节点。这两处如果漏掉，在没有可运行线程时会选中不可运行的线程，或把哨兵节点当成 `proc_struct` 访问。
