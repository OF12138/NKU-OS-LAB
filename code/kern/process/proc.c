#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>



// the process set's list
list_entry_t proc_list; //所有进程控制块的双向线性列表，proc_struct中的成员变量list_link将链接入这个链表中。

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE]; //所有进程控制块的哈希表，proc_struct中的成员变量hash_link将基于pid链接入这个哈希表中。

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL; //本实验中，指向一个内核线程。本实验以后，此指针将指向第一个用户态进程。
// current proc
/*
前占用CPU且处于“运行”状态进程控制块指针。
通常这个变量是只读的，只有在进程切换的时候才进行修改，
并且整个切换和修改过程需要保证操作的原子性，目前至少需要屏蔽中断。可以参考 switch_to 的实现。
*/
struct proc_struct *current = NULL;

// 当前系统中的进程/线程个数（do_fork 成功创建一个就 +1）。get_pid 里的 static_assert 保证 MAX_PID > MAX_PROCESS，所以总能找到空闲 pid。
static int nr_process = 0;

// 下面三个函数用汇编写，这里只做声明：
void kernel_thread_entry(void);                          // process/entry.S：内核线程的统一入口，调用 fn(arg)，fn 返回后调用 do_exit
void forkrets(struct trapframe *tf);                     // trap/trapentry.S：把 sp 设为 tf 后跳到 __trapret，按 tf 恢复寄存器并 sret
void switch_to(struct context *from, struct context *to); // process/switch.S（待实现）：保存 from 的被调用者保存寄存器、恢复 to 的，实现线程切换

// set_proc_name - set the name of proc
// 给进程起名字，只用于调试输出。先把 name 数组清零，再拷贝 PROC_NAME_LEN(15) 字节，所以 name[15] 恒为 '\0'。
// 注意 memcpy 固定拷 15 字节，不会在字符串结尾停下：传入比 15 字节短的字符串常量时会多读后面的内存（读到的内容不影响使用，一般无害）。
char * set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
// 把名字复制到一个 static 缓冲区后返回，方便直接传给 cprintf("%s")。
// 因为缓冲区是全局共享的，连续调用两次，后一次会覆盖前一次的结果；也不是线程安全的。
char * get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// get_pid - alloc a unique pid for process
/*
分配一个当前没有被任何进程使用的 pid。做法是“线性递增 + 偶尔扫描链表”，避免每次都遍历 proc_list：
  last_pid   上一次分配出去的 pid，每次先 +1 作为候选
  next_safe  比 last_pid 大的、已被占用的最小 pid；于是 (last_pid, next_safe) 之间的号码都保证空闲
所以：
  1) 候选 last_pid < next_safe：一定空闲，直接返回（快速路径，不扫描链表）；
  2) 候选 >= next_safe，或 pid 回绕（>= MAX_PID 时从 1 重新开始）：扫描一遍 proc_list ——
       碰到 pid 恰好等于候选就把候选 +1（冲突），并在越过 next_safe 时从头重扫（repeat）；
       碰到比候选大、又比 next_safe 小的 pid，就把 next_safe 降到它，缩小“已知空闲区间”的上界；
  3) 两个 static 变量初值都是 MAX_PID，所以第一次调用必然走 last_pid=1 + 扫描的路径。
pid 0 留给 idleproc（它不是通过 get_pid 分配的），所以这里从 1 开始。
*/
static int get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}


// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
/*
新线程“第一次被调度”时执行的第一个函数。它能被执行，是因为 copy_thread 把新线程 context.ra 设成了 forkret：
switch_to 恢复寄存器后执行 ret，就跳到这里（此时 proc_run 已把 current 改成了这个新线程）。
forkrets(current->tf) 把 sp 指到该线程的 trapframe，再走 __trapret 的“从中断返回”流程：按 tf 恢复所有寄存器，
然后 sret 到 tf->epc。对内核线程，epc 是 kernel_thread_entry，s0/s1 里放着 fn 和 arg，
于是线程就从 fn(arg) 开始运行。这个设计让“新建线程的第一次运行”和“线程从中断返回”共用同一套恢复代码。
*/
static void forkret(void)
{
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
// 以 pid 为键，把 proc 的 hash_link 头插到对应桶的链表中（桶下标 = hash32(pid, 10)，共 1024 个桶）。
// 目的是让 find_proc 按 pid 查找时不用遍历整个 proc_list。
static void hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
// 按 pid 查找进程控制块：只在该 pid 所在的哈希桶里线性查找，找不到（或 pid 不在 (0, MAX_PID) 范围内）返回 NULL。
// le2proc(le, hash_link) 是“从链表节点反推出所在 proc_struct”的 container_of 写法，与 lab2 的 le2page 同理。
struct proc_struct * find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}


// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
/*
给进程分配内核栈：向物理内存管理器要 KSTACKPAGE(=2) 个连续物理页，共 8KiB（KSTACKSIZE）。
alloc_pages 返回的是 Page 描述符，page2kva 把它换成内核虚拟地址，存进 proc->kstack。
这个地址是栈区间的“低端”：栈顶是 proc->kstack + KSTACKSIZE，栈向低地址增长；
copy_thread 会把该线程的 trapframe 放在栈顶附近。
这块内存在 entry.S 建立的 1GiB 大页映射范围内，所以不需要新建任何页表项。
成功返回 0，没有足够物理内存返回 -E_NO_MEM。
*/
static int setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
// setup_kstack 的逆操作：用 kva2page 把内核虚拟地址换回 Page 描述符，再把这 KSTACKPAGE 页还给物理内存管理器。
static void put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}


// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
// 【lab4 只是占位】本 lab 的内核线程都不会退出，真正的进程退出逻辑到 lab5 才实现。
// 若某个线程的 fn 返回，kernel_thread_entry 会调用到这里，直接 panic。
int do_exit(int error_code)
{
    panic("process exit!!.\n");
}

// init_main - the second kernel thread used to create user_main kernel threads
// 【lab4】第二个内核线程（pid=1，initproc）的函数体：打印自己的 pid、名字和传进来的参数后返回。
// 参数 arg 由 proc_init 里的 kernel_thread(init_main, "...", 0) 传入，经 trapframe 的 s1 寄存器交给 kernel_thread_entry。
// 它返回后会走到 do_exit（目前是 panic）。到 lab5 它会改为创建第一个用户进程。
static int init_main(void *arg)
{
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}

//LAB4 function2 2411264
// alloc_proc - 分配一个 proc_struct（kmalloc），并把它的字段设成“刚创建”的安全空白状态。
// state=PROC_UNINIT、pid=-1（身份还未分配）、pgdir=boot_pgdir_pa（内核线程共用内核页表），其余字段一律清零：
// 后续代码（do_fork、调度、测试）会直接读这些字段，不能留下 kmalloc 返回的旧数据。
// 分配失败返回 NULL。
static struct proc_struct *alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context)); // ra/sp 为 0：还没有可切换过去的现场
        proc->tf = NULL;
        proc->pgdir = boot_pgdir_pa;
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN + 1);
        list_init(&(proc->list_link)); // 两个链表节点先自指，表示还没挂进任何链表
        list_init(&(proc->hash_link));
    }
    return proc;
}

// copy_mm - 按 clone_flags 复制或共享 current 的用户地址空间（mm_struct）。
// lab4 只有内核线程，current->mm 恒为 NULL，没有可复制的东西，所以只保留函数形态，直接返回 0（成功）；
// lab5 引入用户进程后才真正实现。
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    assert(current->mm == NULL);
    /* do nothing in this project */
    return 0;
}

// copy_thread - 在 proc 的内核栈顶放一个 trapframe（复制传入的 tf），把 proc->tf 指向它，
// 并设置 context.ra = forkret、context.sp = proc->tf（即 forkret 将在这块栈上执行）。
// 首次运行链路：schedule -> proc_run -> switch_to 恢复 context，ret 跳到 forkret ->
// forkrets(current->tf) 把 sp 设为 tf -> __trapret 按 tf 恢复全部寄存器 -> sret 到 tf->epc。
// ![image](../../../.vscode/image-comment/Screenshot-2026-11-19-152214.png)
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
    // 内核栈区间是 [kstack, kstack + KSTACKSIZE)，trapframe 占用最顶端的 sizeof(struct trapframe) 字节
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
    *(proc->tf) = *tf;

    // a0 是返回值寄存器：子进程/线程从 fork 返回 0，以区别于父进程拿到的子 pid
    proc->tf->gpr.a0 = 0;
    // esp 为 0 表示内核线程：sret 之后就在 trapframe 下方继续用这块内核栈（栈向低地址增长）
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}

/*
do_fork - 创建新线程：alloc_proc -> setup_kstack -> copy_mm（lab4 为空操作）-> copy_thread ->
get_pid + hash_proc + 挂入 proc_list（关中断保护）-> wakeup_proc 置为 RUNNABLE -> 返回子 pid。
任何一步失败都要释放前面已分配的资源（goto 链按分配的逆序释放）：
  进程数已达 MAX_PROCESS -> -E_NO_FREE_PROC；PCB 或内核栈分配失败 -> -E_NO_MEM。
参数 stack 是子线程的栈指针（内核线程传 0），tf 是作为模板的 trapframe。
*/
//about clone_flags:
// ![image](../../../.vscode/image-comment/Screenshot-2026-11-19-144815.png)
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) goto fork_out;
    ret = -E_NO_MEM;

    // 1. 分配并初始化 PCB，父进程是调用者
    if ((proc = alloc_proc()) == NULL) goto fork_out;
    proc->parent = current;

    // 2. 分配 KSTACKPAGE 页内核栈
    if (setup_kstack(proc) != 0) goto bad_fork_cleanup_proc;
    
    // 3. 复制或共享地址空间（lab4 为空操作）
    if (copy_mm(clone_flags, proc) != 0) goto bad_fork_cleanup_kstack;
    
    // 4. 在内核栈上放 trapframe，设置 context
    copy_thread(proc, stack, tf);

    // 5. 分配 pid 并挂入哈希表和进程链表。get_pid 会扫描 proc_list，
    //    它和挂链表、nr_process++ 必须一起完成，中间不能被打断，所以关中断。
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        list_add(&proc_list, &(proc->list_link));
        nr_process++;
    }
    local_intr_restore(intr_flag);

    // 6. 资源都就绪后才设为 RUNNABLE，调度器才可能选中它
    wakeup_proc(proc);

    // 7. 父进程得到子 pid
    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

/*
kernel_thread - 造一个临时 trapframe：s0=fn、s1=arg、epc=kernel_thread_entry、
status 设为“返回 S 态且返回后开中断”（SSTATUS_SPP|SPIE，清掉 SIE），再调用 do_fork。
新线程第一次运行时 sret 到 kernel_thread_entry（process/entry.S），它执行 a0=s1、jalr s0，即 fn(arg)；
fn 返回后调用 do_exit。返回新线程的 pid（失败返回负错误码）。
  SPP=1   sret 之后仍处于 S 态（内核线程）；
  SPIE=1  sret 时 SIE <- SPIE，新线程开始执行后就是开中断的；
  SIE=0   __trapret 把 tf->status 写回 sstatus 到 sret 之间保持关中断，恢复寄存器不会被打断。
tf 是局部变量，do_fork 里的 copy_thread 会把它复制到新线程自己的内核栈上，所以函数返回后它失效也没关系。
*/
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    tf.gpr.s0 = (uintptr_t)fn;  //要执行的函数. 由函数指针提供, 就是这个线程要运行的函数体. 
    tf.gpr.s1 = (uintptr_t)arg;  //上面的函数的参数. 
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE; //status 设为“sret 后仍在 S 态（SPP）、返回后开中断（SPIE），且返回前关中断（清 SIE）”。
    tf.epc = (uintptr_t)kernel_thread_entry; //sret 之后跳转的地址. 
// ![image](../../../.vscode/image-comment/Screenshot-2026-11-19-150158.png)

    return do_fork(clone_flags, 0, &tf);
}

/*
proc_init - 初始化进程管理：
1) 初始化 proc_list 和 1024 个哈希桶；
2) 建 idleproc：它不是 fork 出来的，而是把“从 kern_init 一路执行到这里的当前现场”包装成第 0 个内核线程——
   pid=0，kstack 直接用 entry.S 里的启动栈 bootstack，need_resched=1（cpu_idle 一进去就调度），current 指向它。
   它的 context 保持全 0：第一次从它切走时，switch_to 会把真实寄存器存进去。
3) 用 kernel_thread(init_main, "Hello world!!", 0) 创建 initproc（pid=1）。
*/
void proc_init(void)
{
    //1) 初始化 proc_list 和 1024 个哈希桶；
    int i;
    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i++) list_init(hash_list + i);
    
    //2) 建 idleproc：
    if ((idleproc = alloc_proc()) == NULL) panic("cannot alloc idleproc.\n");
    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    nr_process++;
    current = idleproc;

    //3) 用 kernel_thread(init_main, "Hello world!!", 0) 创建 initproc（pid=1）。
    int pid = kernel_thread(init_main, "Hello world!!", 0); //note: 此时运行的是 idleproc 所以内核栈是内核镜像里的 bootstack. tf 在 bootstack 上. 
    //kernel_thread 里调用了 do_fork, 主要是 a. alloc_proc 分配新的PCB;  b. 开辟新的内核栈(kstack); c.copy_thread() 对内核栈进行填充, d.分配 pid. 挂入 proc_list, hash_list 等. 设置 runnable. 
    if (pid <= 0) panic("create init_main failed.\n");

    initproc = find_proc(pid); //在 hash_list 里找 proc_struct. 
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}


//LAB4 function3 2411264
/*
proc_run - 切换到 proc 运行：关中断 -> 更新 current -> 把 proc->pgdir 装入 satp -> switch_to(旧 context, 新 context) -> 恢复中断。
（switch.S 里的 switch_to 和 sched.c 里的 schedule 也属于功能模块三，各在自己的 function3 标记下。）
- proc 就是 current 时什么也不做。
- satp 的写法参照 pmm.c 的 enable_paging()：MODE 字段在最高 4 位，SATP_MODE_SV39 的值是 8，要左移 60 位；
  低 44 位是根页表的物理页号 pgdir >> 12。riscv.h 里的 lsatp() 是 Sv32 遗留（MODE 在第 31 位、参数只有 32 位），不能用。
  本实验所有线程的 pgdir 都是 boot_pgdir_pa，写 satp 实际不会改变映射；仍按“换页表后 sfence.vma”的规范刷新 TLB，
  lab5 各进程有自己的页表时这一步是必需的。
- 关中断保证“current 已改、页表已换、寄存器还没换”的中间状态不会被时钟中断打断。
- switch_to 返回时，已经是“很久以后 prev 再次被调度回来”，接着执行 local_intr_restore 恢复 prev 自己的中断状态。
  新线程第一次运行不会回到这里，而是从 forkret 开始（见 copy_thread），它的中断由 __trapret 的 sret（SPIE=1）打开。
*/
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

// cpu_idle - idle 线程（kern_init 的最后一步）的死循环：need_resched 为真就调用 schedule() 让出 CPU。
// 本实验中这是唯一的调度点。proc_init 已把 idleproc->need_resched 置 1，所以第一次进入就会切到 initproc；
// 以后只要没有其他可运行线程，schedule 就会回到 idleproc，它在这里继续空转。
void cpu_idle(void)
{
    while (1)
    {
        if (current->need_resched)
        {
            schedule();
        }
    }
}

















/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/