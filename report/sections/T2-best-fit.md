## 功能模块二·任务二：Best-Fit 连续物理内存分配

**负责人：** 2414099-李云鹏

### 模块功能描述

**需要实现/修改的函数：**

```c
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
static void best_fit_insert_block(struct Page *base, size_t n); // 内部辅助
static void init_pmm_manager(void);                           // pmm.c
```

**功能说明：**

Best-Fit 从所有足够大的空闲块中选出最小者。实现复用 `free_area_t` 和 `Page`：空闲链表按地址递增，只有块首页设置 `PG_property` 并通过 `property` 记录页数，`nr_free` 统计空闲页总数。

| 操作 | 实现要点 |
| --- | --- |
| 初始化 | `best_fit_init()` 清空链表；`best_fit_init_memmap()` 将保留页转为空闲页并登记 |
| 分配 | 扫描最小适合块，同大小选低地址；精确匹配时提前结束，否则从块首切出 n 页，余块留在原链表位置 |
| 释放 | 清理页状态，按地址插入；先与前驱合并，再与后继合并，页数仅增加实际释放的 n 页 |
| 接入 | 定义 `best_fit_pmm_manager` 的六个接口，`.check` 使用原有 `best_fit_check()`；`pmm.c` 默认选择该管理器 |

例如，按地址依次有 8、4、6、4 页空闲块，申请 3 页时会选择第一个 4 页块，留下 1 页，而不选择前面的 8 页块。初始化和释放共用插入/合并函数，统一维护链表顺序、块首页标记和空闲页计数。

### 最终提示词

<!-- include:T2-best-fit.prompt.md -->

### 实现迭代过程

本模块共进行了 2 次迭代。

#### 第一次迭代

**遇到的问题：**

- 问题 1：原框架的 `free_list`、`nr_free` 未定义，编译报 `error: 'free_list' undeclared` 等错误。
- 问题 2：阅读参考 First-Fit 时发现，其尾插分支执行 `list_add` 后缺少 `break`，下一轮循环会把新节点再插入到自身之后，破坏链表（实测见练习“First-Fit 分析”中的说明）。

**问题解决策略：**

- 针对问题 1：补齐状态、分配/释放接口和注册结构后，编译通过。`basic_check()`、`best_fit_check()` 均保持原文，未修改测试。
- 针对问题 2：在最终提示词中明确“先定位后继，再在循环外插入一次”，避免沿用 First-Fit 尾插循环中的错误。

#### 第二次迭代

**遇到的问题：**

- 问题 1：官方 `[Test 2]` 只检查管理器指针，不能单独证明 Best-Fit 的选择和合并正确。

**问题解决策略：**

- 针对问题 1：在原有自检之外，增加独立回归内核，检查最小适合块选择、同大小块选择、分裂、碎片导致的分配失败、前后合并及分段初始化。补充检查仅在临时副本中接入。

**最终结果：**

编译通过，`make grade` 得分为 20/20，三组补充回归检查全部 PASS。

### 测试与验证

在 WSL Ubuntu 22.04、RISC-V GCC 10.2.0、QEMU 4.1.1（OpenSBI v0.4）下验证：

| 检查 | 结果 |
| --- | --- |
| `make` / `make qemu` | 编译通过；输出 `memory management: best_fit_pmm_manager` 和 `check_alloc_page() succeeded!`，随后 SLUB 正常初始化 |
| `make grade` | `check page init: OK`、`check best fit: OK`，总分 20/20 |
| `bash tests/run_best_fit.sh` | 三组补充检查全部 PASS，测试后恢复 31929 个空闲页 |

补充检查使用 `code/tests/best_fit_regression.c`。脚本将当前源码复制到临时目录，在临时内核中接入检查，退出时清理临时目录。运行前需将交叉工具链加入 `PATH`，并将 `QEMU` 设置为 4.1.1 的可执行文件。内核完成测试后进入空转，脚本通过超时结束运行。

原始输出保存在 `temp/T2-make.log`、`temp/T2-qemu.log`、`temp/T2-grade.log`、`temp/T2-regression.log`，供整体验证章节使用。

### 是否有进一步的改进空间？

当前分配和空闲块插入的最坏时间复杂度均为 O(B)，B 为空闲块数；定位完成后，相邻块合并为 O(1)。可增加按大小组织的平衡树，将最小适合块查询降到 O(log B)，同时保留地址索引用于合并，但需要增加元数据并维护两套索引。

Best-Fit 只保证本次分配后的剩余块最小，仍可能积累难以利用的小碎片。立即合并可回收相邻空闲区，但无法跨过已分配页。小对象可交由页分配器之上的 SLUB 管理。平衡树和 SLUB 分别改善搜索开销和小对象用页效率，都不能保证消除外部碎片。
