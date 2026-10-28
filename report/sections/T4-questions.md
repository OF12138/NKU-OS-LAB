## 练习：指导书思考题

**负责人：** 2413074-刘昀皓

---

### 思考题 1：stvec 寄存器模式编码的向后兼容性

**题目描述：**

> 「中断与中断处理流程」一节：stvec 最低两位的模式编码是 1.10 版才加入的，这个改动如何保证与旧版本兼容？

#### 1. 背景与规范演进

在 RISC-V 特权级架构（Privileged Architecture）的发展历程中，陷阱向量基地址寄存器 `stvec`（Supervisor Trap-Vector Base-Address Register）的定义经历了一次重要演进：

1. **1.10 版之前的规范（如 v1.9 / v1.9.1）**：
   `stvec` 是一个 SXLEN 位的可读写寄存器，全部位段用于保存 Supervisor 态陷阱处理程序的基地址（Trap Handler Base Address）。规范明确规定：**写入 `stvec` 的基地址必须是 4 字节对齐的（4-byte aligned）**。在硬件实现中，其最低两位（bit[1:0]）被保留并固定绑定为零（hardwired to 0），软件向最低两位写入任何值都会被忽略。
2. **1.10 版及后续正式规范**：
   规范正式将 `stvec` 划分为两个独立字段：
   - **`BASE[XLEN-1:2]`（高 XLEN-2 位）**：保存 4 字节对齐的中断/异常处理程序向量基地址。
   - **`MODE[1:0]`（最低 2 位）**：向量模式选择字段，编码定义如下：
     - `00`（0 - **Direct 模式**）：所有进入 S 态的 Trap（包含异步中断和同步异常），PC 均统一跳转到 `BASE` 地址处。
     - `01`（1 - **Vectored 模式**）：同步异常跳转到 `BASE`；异步中断则根据原因码跳转到 `BASE + 4 × cause`（即基地址处存放一条条跳转指令构成的跳转向量表）。
     - `≥2`（2, 3 - **Reserved**）：保留供未来规范扩展使用。

#### 2. 向后兼容性保障机制分析

1.10 版特权级规范在引入 `MODE` 字段时，通过极具前瞻性的编码映射与硬件语义设计，保证了与旧版本软件和硬件实现的**完全双向兼容**，具体体现在以下四个维度：

##### （1）编码对齐与数值天然兼容（Encoding Compatibility）
在 1.10 版之前的旧版本操作系统中，由于规范硬性要求陷阱基地址必须 4 字节对齐，所有遵循旧规范编写的代码（例如旧版内核启动阶段的 `csrw stvec, t0`）向 `stvec` 写入的目标函数入口地址，其二进制最低两位必然恒为 `00`（因为地址满足 `addr % 4 == 0`）。
当旧版软件运行在支持 1.10 规范的新硬件上时，硬件将接收到的低两位 `00` 直接识别为 `MODE = 0`（Direct 模式）。旧软件无需修改一行代码、无需添加任何模式掩码，其写入的值天然且精确地匹配了 Direct 模式的编码。

##### （2）跳转目标地址的绝对恒等（Address Invariance）
在 Direct 模式下，硬件发生 Trap 时计算目标 PC 的规则为：将高位的 `BASE[XLEN-1:2]` 取出并在低位补零恢复为完整地址。
对于旧软件写入的 4 字节对齐地址 $A$：
$$A = \text{BASE} \ll 2$$
新硬件计算出的目标地址为：
$$\text{Target PC} = \text{BASE} \times 4 = A$$
跳转目标地址与旧软件预期的处理入口地址在二进制层面上分毫不差，不存在任何地址偏移或截断误差。

##### （3）控制流语义的无缝承接（Semantic Consistency）
在旧版规范中，处理器采用“单入口统一分发”模型：硬件在捕获中断或异常后，无差别地跳转到 `stvec` 指向的唯一起始点，由操作系统内核的通用汇编例程（如 `__alltraps`）读取 `scause` 寄存器后在软件层面进行二次分发。
1.10 规范将 `MODE = 0` 赋予 Direct 模式，保留的正是这一套经典的“单入口集中分发”控制流模型；而将 `BASE + 4 × cause` 的向量化跳转定义为可选的扩展模式（`MODE = 1`）。这使得依赖单入口软件分发的既有操作系统代码逻辑无需做出任何架构级重构。

##### （4）极简硬件实现的优雅回退（Hardware Fallback）
规范同时允许低成本、极简嵌入式核或者不需要向量中断特性的硬件实现，将 `stvec[1:0]` 在硬件电路层面直接硬连（Hardwired）为常量 `0`（只读 0）。这样既完全符合 1.10 规范中“仅支持 Direct 模式”的合规子集，又保持了硬件层面对旧软件行为的绝对兼容。

---

### 思考题 2：宏封装与内存分配关中断保护

**题目描述：**

> 「断亦有断」一节：`sync.h` 中 `local_intr_save` 宏用 `do { } while (0)` 包起来有什么作用？结合 `pmm.c` 的 `alloc_pages` / `free_pages` 说明为什么要先关中断再操作。

#### 1. `local_intr_save` 宏使用 `do { } while (0)` 封装的作用

在 `code/kern/sync/sync.h` 中，中断保存宏定义如下：

```c
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
```

使用 `do { ... } while (0)` 进行宏包装是 C/C++ 系统级编程中经典的防御性设计，其核心目的与工程收益如下：

##### （1）消除语法歧义，杜绝 `if-else` 分支中的“分号陷阱”（Semicolon Hazard）
在 C 语言中，宏调用通常以语句形式出现，调用者在书写时习惯在末尾添加分号，如 `local_intr_save(flag);`。
- **反例分析 1（单纯花括号封装）**：若定义为普通复合语句块：
  ```c
  #define local_intr_save(x) { x = __intr_save(); }
  ```
  当该宏置于无花括号的单行 `if-else` 结构中时：
  ```c
  if (condition)
      local_intr_save(flag);
  else
      do_other();
  ```
  经 C 预处理器展开后将变成：
  ```c
  if (condition)
      { flag = __intr_save(); }; // 注意此处的末尾分号！
  else
      do_other();
  ```
  复合语句块后紧跟的分号 `;` 被编译器解析为一条独立的**空语句（Null Statement）**。编译器会认为 `if` 语句已经闭合，随后的 `else` 关键字失去了与之配对的 `if`，进而抛出致命编译错误：
  `error: 'else' without a previous 'if'`。
- **反例分析 2（无花括号平铺）**：若宏内部包含多条语句且不使用代码块包裹，在 `if` 之后展开会导致只有第一条语句受条件保护，其余语句无条件执行，破坏逻辑语义。

- **`do { ... } while (0)` 的解法**：
  在 C 语言文法中，`do-while` 属于标准的**迭代语句（Iteration Statement）**，其语法规则严格要求末尾必须附带一个分号 `;`。
  因此展开后为：
  ```c
  if (condition)
      do { flag = __intr_save(); } while (0);
  else
      do_other();
  ```
  `do ... while (0);` 构成一条完整合法的单一复合语句，分号被语法正确消费，与 `else` 完美结合，无论调用者放置在 `if`、`while`、`for` 还是复杂的宏嵌套中，均能保持 100% 的语法一致性。

##### （2）构造独立的作用域（Scope Isolation）
若宏未来扩展需要定义内部临时变量、中转寄存器变量或进行多步计算，`do { ... }` 构成的块作用域能够有效隔离局部变量名，防止宏内部的变量与外部调用上下文中的变量发生重名冲突或变量遮蔽（Shadowing）。

##### （3）零运行时开销（Zero Runtime Overhead）
现代 C 编译器（如 GCC/Clang）的静态优化器能够敏锐识别恒假循环条件 `while (0)`。无论在任何优化级别下，编译器都会将循环跳转和条件测试完全剔除，直接将其内嵌折叠为顺序执行的直线汇编代码，引入的指令和运行开销严格为 0。

---

#### 2. 结合 `pmm.c` 分析内存分配与释放前关中断的必要性

在 `code/kern/mm/pmm.c` 中，物理内存分配与释放接口均被关中断逻辑严格包裹：

```c
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}
```

结合操作系统的底层机制与物理内存管理器的实现原理，先关中断再操作具有至关重要的系统安全性原因：

##### （1）临界资源保护与数据结构的非原子性（Critical Section & Non-Atomicity）
物理内存管理器 `pmm_manager` 维护着操作系统核心的全局共享状态，主要包括：
1. **全局空闲页双向链表 `free_list`**（用于跟踪当前所有未分配的物理内存块）；
2. **全局空闲页总计数 `nr_free`**；
3. **物理页描述符数组 `pages` 中各个 `struct Page` 的状态位**（如 `flags` 的 `PG_property`、`PG_reserved`，以及连续块大小 `property` 和引用计数 `ref`）。

无论在 First-Fit 还是 Best-Fit 分配算法中，分配页（`alloc_pages`）涉及：遍历链表查找满足条件的空闲块、将空闲块从链表中摘除（`list_del`）、切割（Split）剩余空闲页、重新插入链表（`list_add`）、重新标记页属性以及减少 `nr_free`。释放页（`free_pages`）同样涉及将页插回链表、扫描相邻物理页并进行前后向合并（Merge）、更新属性及增加 `nr_free`。
这些复杂的链表指针操作和状态更新需要执行几十甚至上百条 CPU 指令，**在硬件层面上绝非原子操作**。数据结构在操作中途必然会经历指针未闭合、计数未对齐的“中间不一致状态”（Inconsistent State）。

##### （2）异步中断打断与重入（Reentrancy）导致的毁灭性破坏
在单核 CPU 环境下，内核执行流被并发打断的唯一来源就是**异步硬件中断**（如周期性到来的 RISC-V 时钟中断、外部设备中断）：
- **重入破坏场景**：假定在未关中断的情况下，内核线程正在执行 `alloc_pages`，刚刚执行完 `list_del` 将一个节点摘出，正在准备切割剩余页面并修改双向链表指针；此时时钟中断或外部中断突然到来，CPU 立即保存现场并跳转执行 Trap Handler。
- **链表损坏与死锁/Panic**：如果中断服务程序（ISR）本身、或者中断触发的底半部、驱动程序、软中断也需要分配动态内存（例如为网络数据包或设备通信分配缓冲区），它将再次调用 `alloc_pages`。此时由于前一次操作被硬性打断，`free_list` 链表的前驱后继指针处于破损或自环状态。重入的分配函数在遍历链表时，将引发**死循环、解引用无效地址产生内核页异常（Page Fault in Kernel Mode），或者将同一块物理内存重复分配给两个不同模块（Double Allocation）**。一旦发生，将直接造成操作系统整体崩溃（Kernel Panic）或严重的数据破坏。
- **任务调度并发隐患**：即便当前的 ISR 不显式申请内存，时钟中断的处理例程最终会递增时间片并可能触发**进程上下文切换（Context Switch）**。如果被抢占的内核线程正处于分配内存的中间状态，被换入的新进程若也调用内存接口，同样会发生上述并发竞态。

因此，`alloc_pages` 与 `free_pages` 的内部逻辑构成了操作系统最典型的**临界区（Critical Section）**。在单核处理器上，**屏蔽中断（Disable Interrupts）是切断一切并发交错与上下文抢占的最高效且最彻底的互斥手段**，它从硬件根源上保证了临界区操作的原子性（Atomicity）与互斥性（Mutual Exclusion）。

##### （3）状态保存与嵌套安全性（Nesting Safety via State Preservation）
`local_intr_save` 并没有简单粗暴地执行 `intr_disable()`，而是设计为：
```c
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}
```
它先读取 `sstatus` 寄存器的全局中断使能位（`SIE`），记录下原本的中断状态（开启返回 1，原本即关闭返回 0），然后再关中断；退出时 `local_intr_restore(intr_flag)` 依据保存的原始状态恢复。
这种“检查-保存-恢复”机制确保了**临界区调用的嵌套安全性**：
- 如果调用 `alloc_pages` 前中断原本是开着的，操作结束后会正常恢复开中断；
- 如果调用 `alloc_pages` 的上下文原本就已经处于关中断状态（例如外层已经持有了更高级别的内核锁，或者直接运行在关中断的中断处理程序上下文中），退出时不会破坏外层的屏蔽状态，避免错误地提前开启中断引发竞态。
