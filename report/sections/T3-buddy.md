## 功能模块二·任务三：Buddy System 伙伴分配算法

**负责人：** 2414099-李云鹏（实现、提示词一），2411264-张远（独立测试、提示词二）

### 模块功能描述

```c
static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);
```

Buddy System 按 1、2、4、8……页管理连续物理内存。分配时逐级拆分大块，释放时逐级合并符合条件的伙伴块。它与 Best-Fit 同属页分配层，上层仍通过 `alloc_pages()` / `free_pages()` 使用。通过 `make PMM=buddy` 启用本模块，默认仍使用 Best-Fit，以满足评分要求。

### 设计思路

本实现用 16 条双向链表管理 0～15 阶空闲块。第 k 阶的块包含 2^k 页，起始物理页号必须是 2^k 的整数倍。每个空闲块仅将首页的 `page_link` 挂入对应链表，以首页的 `property` 记录块页数，以 `PG_property` 标记空闲块首页。已分配块的首页保留块页数，但清除该标记；块内其余页的 `property` 为 0。这些信息直接保存在 `struct Page` 中，无需额外分配元数据。

初始化、分配和释放的处理如下：

1. 初始化空闲区：从区间起点开始，每次选取既满足物理页号对齐、又不超过剩余页数的最大块，加入对应链表，再处理后续页面。区间长度不必是 2 的幂，尾部不足大块的页面按小块保留。插入时尝试与已有伙伴合并。
2. 分配与分裂：将请求 n 页向上取整为 S = 2^k 页，从第 k 阶向上查找第一个非空链表。若找到的块大于 S 页，则逐级二分：保留左半块继续拆分，将右半块放回对应链表，直到得到 S 页。空闲页数减 S；没有合适的块时返回 NULL。
3. 释放与合并：校验分配起点、块长和页状态，清理实际占用的 S 页并恢复计数，再用 `伙伴页号 = 本块物理页号 XOR S` 定位伙伴。只有伙伴在管理范围内、空闲且大小同为 S，才将其从链表中移除并合成 2S 页块，以较低页号为新起点继续尝试。最后将合并后的块放回链表；仅地址相邻不能触发合并。

例如，申请 3 页时若取到一个 8 页块，则将其拆成两个 4 页块，分配左半块，将右半块保留在空闲链表中。释放左半块时，若右半块仍空闲，两者可重新合成 8 页。调用 `free_pages(base, 3)` 会归还实际占用的 4 页，也可传入实际页数 4，但不能部分释放。释放时用首页记录的块长进行校验，兼容 SLUB 按原请求页数释放的方式。

按阶组织空闲块无需逐块扫描，但会产生内部碎片：3 页请求占用 4 页，9 页请求占用 16 页。设最大块为 N 页，查找、分裂和伙伴合并最多经过 O(log N) 个阶。本实现释放时还需逐页清理 S 页的状态，因此分配的时间复杂度为 O(log N)，释放为 O(S + log N)。

### 提示词一：实现并接入

<!-- include:T3-buddy.prompt.md -->

### 实现迭代过程

首次编译和 Buddy 启动自检均通过。实现前检查与代码自查中发现的问题及调整如下：

| 发现的问题 | 调整方式 |
| --- | --- |
| 参考资料的树数组最多管理 2048 页，小于当前约 31929 页的空闲区 | 提示词改为按阶链表方案，要求完整登记空闲区，不截断尾部页面 |
| SLUB 按原请求页数释放，与 Buddy 实际占用的页数可能不同 | 明确“分配向上取整、释放归还整块”的约定，首页保存实际块长用于校验 |
| Makefile 不跟踪编译选项变化，切换 `PMM` 可能复用旧管理器 | 增加模式标记依赖，切换时重新编译 `pmm.c`，无需手动 clean |

### 实现方验证

在 QEMU 4.1.1（OpenSBI v0.4）下完成以下验证：

- 编译与启动：`make PMM=buddy`、`make PMM=buddy qemu` 均通过，输出 `buddy: check passed (31929 free pages)`。
- 启动自检：1/3/9 页请求分别占用 1/4/16 页，读写正常，超大请求返回 NULL；释放后各阶块数与总页数恢复，链表、对齐和页状态检查通过。
- 默认功能回归：无需 clean 即可正常切换管理器，默认 `make grade` 仍为 20/20。该评分只验证默认功能未退化，不覆盖 Buddy 算法。

自检核对分配、释放后的状态，用于发现漏页、分裂错误或合并不完整等问题。原始输出见 `temp/T3-buddy-qemu.log`、`temp/T3-grade.log`。

### 提示词二与独立测试

<!-- include:T3-buddy-test.prompt.md -->

#### 测试设计

测试入口是 `buddy_test()`（`code/kern/mm/buddy_test.c`）。`kern_init()` 在 `slub_check()` 之后，仅当当前管理器是 Buddy 时调用它，所以 `make PMM=buddy qemu` 一次启动就能完成全部测试，默认的 Best-Fit 不受影响。这里按运行时的 `pmm_manager` 判断，没有用 `PMM_buddy` 宏，因为 Makefile 切换 PMM 时只会重编 `pmm.c`。

只看“分配返回了非空指针”判断不了伙伴系统是否正确：分裂时取左半还是右半、合并有没有找对伙伴，都不会反映在返回值是否为 NULL 上。因此测试先构造一个**可预测**的环境：

1. 申请一个 64 页、按 64 页对齐的块作为测试区 R；
2. 从第 15 阶到第 0 阶反复分配，借出系统中其余全部空闲页，用已分配页的 `page_link` 串起来暂存。

此后分配器里只剩测试区，每次分配应当返回哪一页都可以写成精确的断言；测试区的伙伴也不会和区外的空闲块意外合并。借出的总页数必须正好等于此前的空闲计数，这一步本身就检查了空闲计数与链表一致。测试只用 `pmm_manager` 的公开回调和 `struct Page` 的公开字段，不读 `buddy_pmm.c` 的私有链表。

每次分配和释放都经过两个辅助函数：`expect_alloc` 检查返回地址、首页 `property` 等于向上取整后的块长、首页不带空闲标记、起始页号按块长对齐、空闲计数正好减少块长；`expect_free` 检查空闲计数正好增加实际块长。

| 测试函数 | 覆盖的核心行为与边界 | 为什么能检出错误 |
| :--- | :--- | :--- |
| `check_split_merge` | 64 页整块注册；申请 1 页逐级分裂出 1+1+2+4+8+16+32；耗尽后分配失败；1 页伙伴先合并；以打乱的顺序释放后整块恢复 | 每个块的地址都事先确定，取错半块、漏挂右半块都会让地址断言失败；最后能拿回 64 页，说明合并逐级做完且与释放顺序无关 |
| `check_round_and_data` | 3/5/9 页请求取整为 4/8/16；5 页请求不能落在只有 4 页的块上；三块写满不同字节后互不覆盖；`alloc(65)`、`alloc(64)`（空闲 36 页但没有 64 页块）、`alloc(SIZE_MAX)` 返回 NULL；按原请求页数和实际块长两种方式释放 | 取整错误会使块长或计数断言失败；写入后读回能发现重叠分配；极大请求检验取整时不溢出 |
| `check_not_buddy` | R+16 与 R+32 相邻且同为 16 页，但不是伙伴，空闲 32 页时 `alloc(32)` 必须失败；真正的伙伴释放后才合并 | 直接针对“按相邻地址合并”这一常见错误；同时覆盖“总空闲页够、但没有足够大的块”的碎片情形 |
| `check_unaligned_init` | `init_memmap(R+3, 13)` 应拆成按页号对齐的 1+4+8；再分段注册 `[R, R+3)` 和 `[R+16, R+64)`，最终合并回 64 页块 | 验证非 2 的幂长度、未对齐起点的初始化，以及分段注册的块能跨段逐级合并 |

最后归还测试区和借出的全部块，检查空闲页数恢复为测试前的值。没有覆盖的部分：重复释放、释放未分配的块这类负向用例会触发实现中的 `assert` 使内核停机，本测试不包含；Buddy 只在单核下使用，也不涉及并发。

#### 测试的有效性：变异测试

为了确认这些断言确实能发现实现错误，在临时副本中向 `buddy_pmm.c` 逐个注入下列错误，并关闭实现方自检 `buddy_check()` 和 SLUB 自测（它们在本测试之前运行，会先发现部分错误），只看 `buddy_test()` 能否发现：

| 编号 | 注入的错误 | 结果 | 在哪里被发现 |
| :--- | :--- | :--- | :--- |
| M1 | 伙伴页号用 `本块 + 块长` 代替 `本块 XOR 块长`（按相邻地址合并） | 发现 | `check_split_merge` 的地址断言 |
| M2 | 释放时完全不合并 | 发现 | `check_split_merge` 的地址断言 |
| M3 | 阶数向下取整（不向上取整） | 发现 | `check_round_and_data` 的块长断言 |
| M4 | `init_memmap` 不考虑页号对齐 | 发现 | 测试开始时测试区 R 就没有按 64 页对齐 |
| M5 | 空闲计数按请求页数而不是实际块长扣减 | 发现 | `check_round_and_data` 的计数断言（前一组请求都是 2 的幂，所以没有暴露） |
| M6 | 合并只做一级，不继续向上 | 发现 | `check_split_merge` 的地址断言 |
| M7 | 分裂时丢掉右半块（不挂回链表） | 发现 | `drain` 的总数核对：借出的页数少于空闲计数 |
| M8 | 合并时不检查伙伴块长是否相同 | 发现 | `check_split_merge` 的地址断言 |

8 种错误全部被发现，原始输出见 `temp/T3-buddy-mutation.log`。

#### 实现迭代过程

| 发现的问题 | 调整方式 |
| --- | --- |
| 设计时发现测试区的伙伴可能是区外的空闲块，测试中合并出的块会越出测试区，地址也无法预测 | 测试开始时先借出系统中所有其余空闲页，使分配器只剩测试区；借出的总数顺便用于核对空闲计数 |
| 初版写内存用的是 `KADDR`，但框架导入时 `pmm.h` 中这个宏是注释掉的，提示词的 [RELY] 无法只依赖框架接口 | 改用框架提供的 `page2pa(p) + va_pa_offset` |
| 第一次变异测试中，M5 先被 SLUB 自测的断言拦下，M7 在进入本测试之前就停住了，没有任何 `[BUDDY]` 输出，看不出本测试能否发现它们 | 变异测试时同时关闭 SLUB 自测，重新运行后两者都被本测试发现 |

测试代码首次编译运行即全部通过，没有出现测试本身的失败轮次。

#### 实测结果

在 QEMU 4.1.1（OpenSBI v0.4）下：

- `make PMM=buddy qemu`（128 MiB）：四组测试依次输出 PASS，最后输出 `[BUDDY] all tests passed (31919 free pages restored)`。
- 64 MiB 启动：同样全部通过，最后一行为 `[BUDDY] all tests passed (15695 free pages restored)`。
- 默认 `make qemu` 使用 Best-Fit，不运行本测试；`make grade` 仍为 **20/20**。

完整串口输出见 `temp/T3-buddy-test.log`，128 MiB 下的关键输出如下：

```
memory management: buddy_pmm_manager
buddy: check passed (31927 free pages)
[SLUB] all checks passed (retained 7 active slab pages)
[BUDDY] split 64 -> 1 and merge back: PASS
[BUDDY] round up 3/5/9 -> 4/8/16, data isolation, oversize NULL: PASS
[BUDDY] adjacent non-buddies stay apart, fragmentation fails: PASS
[BUDDY] unaligned/odd-length and segmented init_memmap: PASS
[BUDDY] all tests passed (31919 free pages restored)
```
