## Challenge：开关中断机制与多级页表设计剖析

**负责人：** 2413074-刘昀皓

---

### Challenge 1：`local_intr_save` 与 `local_intr_restore` 的开关中断机制及调度保护

**题目描述：**

> 说明语句 `local_intr_save(intr_flag); ... local_intr_restore(intr_flag);` 是如何实现开关中断的（结合 `kern/sync/sync.h`、`libs/riscv.h` 中的 `sstatus.SIE` 读写，以及本 lab 中 `schedule` / `proc_run` 为什么需要它）。

#### 1. 宏定义与底层 CSR 操作链路

在 `code/kern/sync/sync.h` 中，临界区中断保护由两个宏及两个静态内联函数协同完成：

1. **宏封装接口（`code/kern/sync/sync.h:32-36`）**：

   ```c
   #define local_intr_save(x) \
       do {                   \
           x = __intr_save(); \
       } while (0)
   #define local_intr_restore(x) __intr_restore(x);
   ```

   - `local_intr_save(intr_flag)` 使用 `do { ... } while (0)` 封装，确保宏在 `if-else` 等条件语句中展开时被视为单条复合语句，不会因为末尾分号引发语法错位或作用域泄漏。
   - 注意到参数 `intr_flag`（`bool` 类型）无需取地址即可在宏内部直接赋值，调用后保存进入临界区前的中断使能状态。

2. **状态读取与条件关中断：`__intr_save()`（`code/kern/sync/sync.h:10-19`）**：

   ```c
   static inline bool __intr_save(void)
   {
       if (read_csr(sstatus) & SSTATUS_SIE)
       {
           intr_disable();
           return 1;
       }
       return 0;
   }
   ```

   - `read_csr(sstatus)`（定义于 `code/libs/riscv.h:210-212`）展开为 RISC-V CSR 读取指令 `csrr %0, sstatus`，读取 Supervisor 模式状态寄存器 `sstatus`。
   - `SSTATUS_SIE`（定义于 `code/libs/riscv.h:46`，值 `0x00000002`）对应 `sstatus` 的第 1 位（SIE，Supervisor Interrupt Enable）：当运行在 S 态时，`SIE = 1` 表示允许响应 S 态全局异步中断，`SIE = 0` 表示禁止。
   - 若检测到当前 `sstatus.SIE == 1`（中断原本开启），则调用 `intr_disable()`（`code/kern/driver/intr.c:8`），其本质是 `clear_csr(sstatus, SSTATUS_SIE)`（即 `csrrc` 指令清除 `SIE` 位），关中断并返回 `1` 记入 `intr_flag`。
   - 若检测到当前 `sstatus.SIE == 0`（中断原本已关闭），则不做冗余 CSR 写操作，直接返回 `0`。

3. **按原状态恢复中断：`__intr_restore()`（`code/kern/sync/sync.h:21-25`）**：

   ```c
   static inline void __intr_restore(bool flag)
   {
       if (flag) intr_enable();
   }
   ```

   - 仅当 `intr_flag == 1`（即进入临界区前原本开着中断）时，调用 `intr_enable()`（`code/kern/driver/intr.c:5`，展开为 `set_csr(sstatus, SSTATUS_SIE)` 即 `csrrs` 指令置位 `SIE`）重新开启中断。
   - 若 `intr_flag == 0`，则保持 `sstatus.SIE = 0` 不变。

4. **“保存-屏蔽-条件恢复”带来的嵌套安全性**：
   若某段代码本身已在关中断条件下执行（例如处于外层 `local_intr_save` 临界区内，或处于硬件自动清零 `sstatus.SIE` 的 Trap 处理上下文中），内层再次调用 `local_intr_save` 时会测得 `intr_flag = 0`；内层退出调用 `local_intr_restore(0)` 时不会提前把中断打开，从而保证外层临界区的互斥契约不被破坏，也避免中断处理途中发生意外嵌套重入。

#### 2. 本 Lab 中 `schedule` 与 `proc_run` 为什么需要开关中断保护

在 `code/kern/init/init.c:46-49` 中，`proc_init()` 完成后依次调用 `clock_init()` 与 `intr_enable()` 打开时钟中断，随后进入 `cpu_idle()` 等待调度。此后每 10ms（`timebase = 1e7 / 100`，`code/kern/driver/clock.c:55`）都会产生一次异步 `IRQ_S_TIMER` 时钟中断。此外，在 `code/kern/process/proc.c:305-313` 的 `do_fork()` 中，分配 `pid`、挂入 `hash_list` 与 `proc_list` 并递增 `nr_process` 的步骤也同样使用 `local_intr_save/restore` 保护。单核环境下虽然没有多核并行竞争，但异步中断与当前内核执行流之间仍存在并发交错风险：

1. **保护 `schedule()`（`code/kern/schedule/sched.c:28-61`）中的共享调度数据结构一致性**：

   ```c
   local_intr_save(intr_flag);
   {
       current->need_resched = 0;
       last = (current == idleproc) ? &proc_list : &(current->list_link);
       le = last;
       do {
           if ((le = list_next(le)) != &proc_list) {
               next = le2proc(le, list_link);
               if (next->state == PROC_RUNNABLE) break;
           }
       } while (le != last);
       if (next == NULL || next->state != PROC_RUNNABLE) next = idleproc;
       next->runs++;
       if (next != current) proc_run(next);
   }
   local_intr_restore(intr_flag);
   ```

   - `schedule()` 需要修改全局共享的 `current->need_resched` 标志，并从 `current->list_link`（或 `proc_list` 表头）出发遍历全局双向链表 `proc_list`（`code/kern/process/proc.h:54`）寻找下一个 `state == PROC_RUNNABLE` 的线程。
   - 若不在遍历与决策期间关中断，一旦操作中途被异步中断打断，而中断处理路径（或后续实验引入的时间片到期置 `need_resched`、`wakeup_proc` 等）修改了 `proc_list` 节点链接或进程 `state` 字段，恢复回来后的 `schedule()` 将继续持有失效的链表指针或过时的状态判断，可能引发链表遍历越界、漏选就绪线程或选中非可运行进程。

2. **保证 `proc_run()`（`code/kern/process/proc.c:403-424`）切换过程的“进程身份-地址空间-寄存器现场”三元原子性**：

   ```c
   local_intr_save(intr_flag);
   {
       current = next;
       write_csr(satp, ((uintptr_t)SATP_MODE_SV39 << 60) | (next->pgdir >> RISCV_PGSHIFT));
       flush_tlb();
       switch_to(&(prev->context), &(next->context));
   }
   local_intr_restore(intr_flag);
   ```

   `proc_run(struct proc_struct *proc)` 需要在关中断临界区内依次完成三个不可分割的软硬件状态迁移：
   - **更新内核运行身份**：将全局指针 `current` 指向目标线程 `next`（第 411 行）；
   - **切换页表地址空间**：将目标线程的页目录物理基址 `next->pgdir` 按 Sv39 格式写入 `satp` 寄存器（第 413 行，`MODE=8` 左移 60 位或上物理页号）并调用 `flush_tlb()` 执行 `sfence.vma` 刷新 TLB（第 414 行）；
   - **切换 CPU 寄存器上下文**：调用 `switch_to(&(prev->context), &(next->context))`（第 419 行，`code/kern/process/switch.S:15-48`）保存原线程的 `ra`、`sp` 与 `s0`-`s11`（`code/kern/process/proc.h:32-48`），恢复目标线程上下文。

   如果不关中断保护这一序列，假设 CPU 刚执行完 `current = next`（或刚写入新的 `satp`），还未执行 `switch_to` 就被时钟中断打断，Trap 处理现场就会出现严重撕裂：
   - **内核控制流错乱**：中断处理函数读取的 `current` 已经是新线程 `next`，但当前 CPU 实际还在使用旧线程 `prev` 的内核栈 `sp` 与寄存器现场；若后续操作访问 `current` 或再次触发调度，会把 `prev` 的运行现场错绑定到 `next` 上。
   - **页表与指令流不匹配**：若切换的是拥有独立用户页表的进程，在旧页表与新现场或新页表与旧栈之间被异步中断插入，可能造成访存越权或异常。

3. **与新线程首次运行链路（`forkret`）及嵌套临界区的闭环衔接**：
   - 注意到 `schedule()` 在外层调用了 `local_intr_save`，内部调用 `proc_run(next)` 时会再次执行内层 `local_intr_save`——此时由于外层已将 `sstatus.SIE` 清零，内层 `intr_flag` 记录为 `0`，体现了第 1 节所述的**嵌套临界区安全性**。
   - 对于首次被调度执行的内核线程（如 `initproc`），`switch_to` 末尾的 `ret` 会跳转到 `forkret() -> forkrets(current->tf) -> __trapret`（`code/kern/process/proc.c:131-134`、`code/kern/trap/trapentry.S:157-167`）。在 `kernel_thread()`（`code/kern/process/proc.c:346`）构造初始 `tf.status` 时，特地设置了 `(read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE`（保持 `SIE=0`、`SPIE=1`），从而保证在 `__trapret` 恢复全部通用寄存器期间依然维持关中断，直到最后一条 `sret` 指令由硬件原子地将 `SPIE(1)` 赋给 `SIE` 才正式开启新线程的中断；而当旧线程 `prev` 日后再次被调度切回时，则从 `switch_to` 下一行返回，沿 `proc_run` -> `schedule` 依次执行 `local_intr_restore` 恢复原本的中断状态，形成严密的闭环。

---

### Challenge 2：深入理解不同分页模式与 `get_pte()` 设计思考

**题目描述：**

> - `get_pte()`（`kern/mm/pmm.c`）中有两段形式类似的代码，结合 sv32、sv39、sv48 的异同，解释这两段代码为什么如此相像。
> - 目前 `get_pte()` 将页表项查找和页表页分配合并在一个函数里，这种写法好吗？有没有必要把两个功能拆开？

#### 1. `get_pte()` 的两段同构代码与 Sv32 / Sv39 / Sv48 异同解析

在 `code/kern/mm/pmm.c:353-396` 中，面向 Sv39 三级页表的 `get_pte()` 实现如下结构：

- **第一段（第 356-374 行）**：用 `PDX1(la)`（`VPN[2]`）索引一级根页表 `pgdir`，得到目录项指针 `pdep1`。若 `!(*pdep1 & PTE_V)` 且 `create == 1`，则调用 `alloc_page()` 分配一页 4 KiB 物理页、置引用计数 `set_page_ref(page, 1)`、`memset(KADDR(pa), 0, PGSIZE)` 清零，并构造 `*pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V)`。
- **第二段（第 377-392 行）**：从 `*pdep1` 解出二级页表物理地址并转为内核虚拟地址 `KADDR(PDE_ADDR(*pdep1))`，用 `PDX0(la)`（`VPN[1]`）索引得到目录项指针 `pdep0`。若无效且允许创建，则执行完全相同的“分配 1 页 -> `set_page_ref(page, 1)` -> `memset` 清零 -> `pte_create(..., PTE_U | PTE_V)`”步骤。
- **最终返回（第 395 行）**：从 `*pdep0` 解出三级（叶子）页表基址，加上 `PTX(la)`（`VPN[0]`）偏移，返回目标 `pte_t *`。

##### （1）RISC-V 三种分页模式（Sv32 / Sv39 / Sv48）的异同对比

| 比较维度 | Sv32（RV32） | Sv39（RV64，本实验采用） | Sv48（RV64） |
| :--- | :--- | :--- | :--- |
| **适用架构与虚拟地址位数** | 32 位系统，32 位虚拟地址 | 64 位系统，有效低 39 位（高 25 位须等于 bit[38]） | 64 位系统，有效低 48 位（高 16 位须等于 bit[47]） |
| **物理地址与页内偏移** | 34 位 PA，12 位页内偏移（4 KiB 页） | 56 位 PA，12 位页内偏移（4 KiB 页） | 56 位 PA，12 位页内偏移（4 KiB 页） |
| **页表级数与索引切片** | **2 级**：`VPN[1](10) + VPN[0](10)` | **3 级**：`VPN[2](9) + VPN[1](9) + VPN[0](9)` | **4 级**：`VPN[3](9) + VPN[2](9) + VPN[1](9) + VPN[0](9)` |
| **PTE 宽度与每页表项数** | 4 字节（32 位），1024 项/页 | 8 字节（64 位），512 项/页（`NPTEENTRY = 512`） | 8 字节（64 位），512 项/页 |
| **中间目录（非叶子）个数** | $2 - 1 = 1$ 层 | $3 - 1 = 2$ 层 | $4 - 1 = 3$ 层 |
| **PTE 低 10 位标志位与物理页号布局** | `[31:10]` 为 PPN，`[9:0]` 为 RSW/D/A/G/U/X/W/R/V | `[53:10]` 为 PPN，`[9:0]` 为 RSW/D/A/G/U/X/W/R/V | `[53:10]` 为 PPN，`[9:0]` 为 RSW/D/A/G/U/X/W/R/V |

##### （2）为什么两段代码如此相像？

1. **多级页表是高度对称的基数树（Radix Tree）**：
   Sv32、Sv39、Sv48 尽管虚拟地址位宽和树高（2 级、3 级、4 级）不同，但它们解决核心问题（避免线性大页表浪费内存）采用的是同一种多级树形组织结构。在这棵树上，**所有中间级页表承担的职责在算法上是完全自相似的**：根据当前层级的 VPN 索引定位目录项 -> 判断 `PTE_V` 是否有效 -> 无效时按需申请一页 4 KiB 零化页表页并挂入父项 -> 解码 `PPN` 跳转到下一级页表基址。
2. **统一的 4 KiB 页粒度与 PTE 编码规范**：
   无论采用 Sv32、Sv39 还是 Sv48，基本页尺寸固定为 4 KiB（`PGSIZE = 4096`，页内偏移占 12 位），每个页表自身恰好占据 1 个 4 KiB 物理页；同时 PTE 低 10 位控制位（尤其 `PTE_V`、`PTE_R/W/X` 区分叶子项与中间指针）以及 `PTE_PPN_SHIFT = 10`（`code/kern/mm/mmu.h:73`）在三种模式下完全统一。
3. **两段重复正是 Sv39 三级结构的直接映射**：
   对任意 $L$ 级分页模式，在到达最终叶子 PTE 所在页表前，必须经过 $L - 1$ 次“中间目录查找与按需分配”。本实验采用 3 级 Sv39，恰好对应 $3 - 1 = 2$ 次结构相同的目录步进（分别由 `PDX1(la)` 和 `PDX0(la)` 索引），因此源码呈现出两段高度相似的代码；若换到 2 级的 Sv32 只需 1 段，若升级到 4 级的 Sv48 则会自然扩展为 3 段同构代码（或抽象为参数化循环遍历）。

#### 2. `get_pte()` 将“查找”与“页表页分配”合并的利弊及拆分必要性分析

目前 `code/kern/mm/pmm.c:353` 通过 `bool create` 参数把“只读定位 PTE”和“缺页表页时分配创建”合并在同一个 `get_pte(pgdir, la, create)` 函数中。这种设计具有明显的**教学简洁性与工程耦合性双重特征**：

##### （1）合并写法的优点（为何框架采用该设计）

1. **避免多级遍历逻辑重复**：无论是纯查找还是创建映射，都要沿着 `pgdir -> PDX1 -> PDX0 -> PTX` 走完全相同的索引计算与指针解引用路径。用 `bool create` 控制分支，避免了在教学代码中复制两份三级寻址逻辑。
2. **写路径单次遍历即可完成**：在 `boot_map_segment()`（`code/kern/mm/pmm.c:250`）和 `page_insert()` 等建立映射的场景中，调用一次 `get_pte(pgdir, la, 1)` 即可在单遍遍历中顺便创建缺失的中间页表并返回最终叶子项地址，调用接口紧凑。

##### （2）合并写法的局限性（工程实践中的隐患）

1. **违反单一职责原则（SRP），接口语义含混**：
   只读查找应是幂等、无副作用且永不产生内存分配的；而按需分配会修改页表树结构并消耗物理内存（甚至可能 OOM 失败）。二者共用同一函数名 `get_pte`，仅凭布尔参数区分，可读性较差，调用方一旦误传 `create = 1` 会在本应只读的探测场景里悄悄修改内核页表。
2. **错误语义与空映射无法区分**：
   `get_pte` 统一通过返回 `NULL` 表示异常，但在 `create = 0` 时 `NULL` 代表“该线性地址尚未建立映射（正常现象）”，在 `create = 1` 时 `NULL` 代表“物理页耗尽分配失败（`-E_NO_MEM` 错误）”，丢失了精确的错误码信息。
3. **半创建状态难以回滚（内存泄漏隐患）**：
   在 Sv39 三级结构下，若 `create = 1` 时第一级 `pdep1` 分配页表页成功，但第二级 `pdep0` 调用 `alloc_page()` 时物理内存耗尽返回 `NULL`，`get_pte()` 会直接返回 `NULL`。此时挂在 `pgdir[PDX1(la)]` 上的那页二级页表页已经清零挂入但未被任何有效叶子页使用，函数本身并未将其回滚释放。
4. **中间页表权限与属性缺乏精细控制**：
   当前 `get_pte` 在内部硬编码了目录项属性 `PTE_U | PTE_V`（`code/kern/mm/pmm.c:373,391`），无论映射的是内核地址空间还是用户地址空间，都无法按场景灵活定制中间目录标志位。

##### （3）结论：是否有必要拆开？

- **在当前 ucore Lab 4 教学实验范围下，没有必要强行改动原有接口**：现有内核测试函数（`check_pgdir`、`check_boot_pgdir`）和评测测试（`test_lab4_pmm_basic`）均直接依赖 `pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)` 签名；在当前固定 Sv39 单线程启动环境下，合并写法的简洁性大于其扩展成本。
- **在面向生产级内核或进一步功能演进时，应当拆开并分层重构**：
  1. **拆出纯只读查找接口** `pte_t *lookup_pte(pde_t *pgdir, uintptr_t la)`：无任何副作用，可在中断上下文、锁保护只读区安全调用；
  2. **拆出专用的创建/分配接口**（如 `int alloc_pte(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)`）：显式返回 `0` 或 `-E_NO_MEM`，支持在某一级 `alloc_page()` 失败时精确回滚此前刚分配的空中间页表页；
  3. **抽象多级遍历循环**：将 `PDX1` / `PDX0` 两段硬编码重构为基于页表级数 `level`（如 Sv39 为 3、Sv48 为 4）的统一迭代查找与分配逻辑，兼顾可维护性与多种分页模式的平滑扩展。
