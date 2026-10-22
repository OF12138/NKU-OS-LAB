## Challenge：任意大小内存单元的 SLUB 分配算法

**负责人：** 2411264-张远（实现与接入，提示词一）、2414099-李云鹏（测试，提示词二）

### 设计思路

页分配器（First-Fit / Best-Fit / Buddy）的最小单位是 4 KB 的页。内核里大量的对象只有几十字节（链表节点、进程控制块、文件描述符……），如果每个都占一页，内部碎片极大。SLUB 在页分配器之上再加一层：向页分配器要一整页（称为一个 slab），把它切成等长的小对象，按对象大小分别管理。

本实现的两层结构：

| 层次 | 接口 | 实现 |
| :--- | :--- | :--- |
| 第一层：页级分配 | `alloc_pages()` / `free_pages()` | 直接使用当前 `pmm_manager`（默认 Best-Fit），SLUB 不关心底层用的是哪种算法 |
| 第二层：对象分配 | `kmem_cache_create/alloc/free/destroy`、`kmalloc/kfree/ksize` | slab 切分、freelist、partial 链表 |

SLUB **没有**注册成一个新的 `pmm_manager`，而是作为独立模块接在 `pmm_init()` 之后（`kern_init` 中调用 `slub_init()`）。原因有两点：一是两层本来就是上下级关系，不是并列的页分配算法；二是 `make grade` 要求当前 `pmm_manager` 必须是 `best_fit_pmm_manager`，把 SLUB 注册成管理器会让 Best-Fit 的检查失败。

![SLUB 结构示意图](../images/T4-slub-layout.png)

<p align="center">图 T4-1　slab 页的布局与 kmem_cache 的组织</p>

**slab 页的布局**（图 T4-1(a)）：页首是 48 字节的 `struct slab` 头，记录魔数、已分配对象数、所属缓存和 freelist；后面是等长的对象。空闲对象的前 8 字节存放下一个空闲对象的地址，freelist 就“嵌”在空闲对象里，不需要额外的空间。释放时，把对象地址向下对齐到 4 KB 就能找到 slab 头，从而知道它属于哪个缓存。

**缓存的组织**（图 T4-1(b)）：每种对象大小对应一个 `struct kmem_cache`，它持有三类 slab：

- **活动 slab**（`cpu_slab`）：分配时优先从它的 freelist 头部弹出对象，这是最快的路径。Linux 中每个 CPU 有一个活动 slab，这里是单核，所以只有一个。
- **部分空闲 slab**：挂在 `partial` 双向链表上。活动 slab 用完时，从这里取一个作为新的活动 slab；链表也为空时，才向页分配器申请新页。
- **满 slab**：不挂在任何链表上。满了就没有可分配的对象，SLUB 不需要找到它；等它的某个对象被释放时，再根据对象地址找回 slab，把它重新挂回 `partial`。

非活动 slab 的对象全部释放后，整页立即还给页分配器。活动 slab 变空时保留，避免在同一页上反复申请和释放。

**kmalloc**：`slub_init()` 先用一个静态的 `kmem_cache`（存放 `struct kmem_cache` 本身，称为 cache_cache）完成自举，再建立 16、32、64、128、256、512、1024 字节共 7 个通用缓存。`kmalloc(size)` 选用能容纳 `size` 的最小缓存；超过 1024 字节时直接按页分配，在首页页首写一个带不同魔数的头，记录页数。`kfree` 读取页首的魔数，就能区分这两类内存。

为什么上限取 1024 而不是 2048：slab 头放在页首，一页放下头部后，只能再容纳一个 2048 字节的对象，和直接按页分配没有区别。

**与 Linux SLUB 的对应和简化**：

| 方面 | Linux SLUB | 本实现 |
| :--- | :--- | :--- |
| freelist | 嵌在空闲对象中 | 相同 |
| 链表 | 只维护 partial，不跟踪满 slab | 相同 |
| 活动 slab | 每个 CPU 一个 | 单核，每个缓存一个 |
| slab 元数据 | 存放在 `struct page` 中 | ucore 的 `struct Page` 没有空余字段，改放在页首（代价是每页少放一些对象，例如 kmalloc-1024 每页只能放 3 个） |
| slab 大小 | 可以是多页（高阶） | 固定一页 |
| 其他 | per-node 链表、min_partial、调试毒化、对象合并 | 不实现；只保留重复释放和非法指针的检查 |

### 提示词一：实现并接入内核

<!-- include:T4-slub.prompt.md -->

### 实现迭代过程

本模块的实现经历了 2 次迭代，过程如下：

#### 第一次迭代

**遇到的问题：**

- 问题 1：前辈的实现把 SLUB 注册成一个 `pmm_manager`，内部再调用 Buddy。照这样做，当前管理器就不再是 `best_fit_pmm_manager`，`make grade` 的 `[Test 2] Best-Fit` 会失败。
- 问题 2：slab 元数据放在哪里。Linux 放在 `struct page` 里，但 ucore 的 `struct Page` 只有 `ref`、`flags`、`property`、`page_link` 四个字段，而且页分配器都在使用；如果扩充 `struct Page`，描述符数组会变大，page_init 的内存布局也会跟着改变。
- 问题 3：`pmm.h` 中的 `KADDR` 宏被注释掉了。`alloc_pages()` 返回的是描述符，不是可以直接读写的地址。

**问题解决策略：**

- 针对问题 1：在提示词中明确“不要注册成 `pmm_manager`”，第一层直接调用 `alloc_pages()` / `free_pages()`，并说明 `make grade` 的检查方式。
- 针对问题 2：在 [RELY] 中说明 `struct Page` 没有空余字段、不能修改，在 [Requirements] 中规定元数据放在页首。kmalloc 的上限相应定为 1024 字节。
- 针对问题 3：在 [RELY] 中注明 `KADDR` 不可用，物理地址转虚拟地址要写成 `pa + va_pa_offset`。

#### 第二次迭代

**遇到的问题：**

- 问题 1：编写释放路径时注意到一个边界情况：如果某个缓存每个 slab 只能放一个对象（例如对象大小接近一页的自定义缓存），slab 会在一次释放中从“满”直接变成“空”。满 slab 不在 partial 链表上，这时如果按“变空就从链表摘下”的逻辑执行 `list_del`，就会破坏链表。

**问题解决策略：**

- 针对问题 1：释放时先记下 slab 原来是否是满的，只有原来不满（在链表上）时才 `list_del`。这条作为 Case 3 写进提示词中 `kmem_cache_free` 的 [SPECIFICATION]。

**最终结果：**

经过 2 次迭代，该模块最终：

- ✅ 编译通过，`make qemu` 正常启动，输出 `slub: 7 kmalloc caches (16 ~ 1024 bytes), larger requests use whole pages`
- ✅ 当时 `make grade` 的 page init 检查通过；Best-Fit 尚待 T2 完成。本节末尾的正式回归已在当前 Best-Fit 下取得 20/20。
- ✅ 冒烟验证通过（见下）。正式测试由提示词二生成。

正式测试完成之前，我在临时副本中做了一次冒烟验证（First-Fit 冒充 Best-Fit，测试代码不进仓库）：

| 检查内容 | 结果 |
| :--- | :--- |
| `kmalloc` 1、8、16、17、33、100、255、500、1000、1024 字节 | 分别落在 16、16、16、32、64、128、256、512、1024、1024 字节缓存，地址 8 字节对齐 |
| `kmalloc` 1025、4000、4097、20000 字节 | 按页分配 1、1、2、5 页，`ksize` 分别为 4048、4048、8144、20432 |
| 连续分配 600 个 64 字节对象并写入编号 | 占用 10 个 slab（每个 63 个对象），逐字节校验全部正确 |
| 释放偶数号对象 | 9 个 slab 进入 partial，1 个为活动 slab |
| 全部释放 | 只剩活动 slab 1 个，另外 9 页还给页分配器 |
| 自定义缓存 40 字节、16 对齐 | 步长 48，对象 16 字节对齐；有对象未释放时拒绝销毁，释放后销毁成功 |
| `kmem_cache_create("too-big", 4096, 8)` | 返回 NULL |
| 重复释放同一个 32 字节对象 | `kernel panic at kern/mm/slub.c:200: slub: double free of 0xffffffffc034b030 in cache kmalloc-32` |

页数核对：冒烟测试开始时有 31926 个空闲页，结束时 31919 个，相差的 7 页正好是 7 个 kmalloc 缓存各自保留的活动 slab。

**关键改进点总结：**

1. 明确 SLUB 是页分配器之上的独立一层，不注册成 `pmm_manager`，与 `make grade` 的要求兼容。
2. 在 [RELY] 中写明 `struct Page` 不可扩充、`KADDR` 不可用这两个框架限制。
3. 在 [SPECIFICATION] 中补充单对象 slab 从满直接变空的边界情况。

### 提示词二：测试函数

<!-- include:T4-slub-test.prompt.md -->

### 独立测试的设计与覆盖

测试由李云鹏于 2026-10-22 完成。入口为 [`slub_check()`](../../code/kern/mm/slub_test.c)，在 `kern_init()` 的 `slub_init()` 之后运行。测试通过公开接口分配和释放对象，并读取公开的缓存统计；没有修改 Best-Fit 原始测试、评分脚本或页描述符布局。

单纯观察“返回了非空指针”无法判断分配器是否正确。例如，两个对象可能重叠，释放后也可能没有真正归还页面。因此测试同时核对四类证据：返回地址与容量、已写入内容、缓存状态，以及底层空闲页数。写入的字节模式同时包含对象编号和字节位置，填满 `ksize()` 声明的可用空间；再次分配或释放相邻对象后重读，可以发现重叠分配、越界覆盖和错误复用。

| 测试函数 | 核心行为与边界 | 为什么能够检错 |
| :--- | :--- | :--- |
| `check_invalid` | 零长度、NULL、12 字节非法对齐、过大对象、`SIZE_MAX`、接近整数上界的加法、最高位对齐值、禁止销毁通用缓存 | 既核对拒绝结果，也核对页数不变；极大输入能暴露对齐计算的整数回绕 |
| `check_sizes` | 遍历 1～1024 的每个请求大小；22 个大小类边界对象同时存活，交错释放和再分配，再逆序释放 | 核对最小适配缓存、对齐、`ksize` 和内容，区分“能分配”与“分配正确且互不干扰” |
| `check_partial` | 64 字节缓存填满两页；非活动满页释放两个对象，只进入 partial 一次；活动页满时提升 partial 并后进先出复用 | 地址、页数和 `nr_partial` 联合证明旧页被复用，没有额外申请第三页；继续释放区分非活动整页回收与活动页保留 |
| `check_custom` | 1/40/65/3000/4048 字节，默认/16/64/8 字节对齐；在用缓存拒绝销毁；单对象 slab 从满直接变空；40 个缓存描述符跨页 | 检查步长、对象完整性、销毁失败后状态不变，以及单对象页释放时 partial 计数不下溢；额外覆盖描述符缓存的生命周期 |
| `check_large` | 1025、4048、4049、4096、4097、9000、20000 字节同时分配 | 4048/4049 分别位于“头部后剩余一页容量”的两侧；逐次核对页数和全范围内容，释放后验证页面完全恢复 |
| `check_oom` | 通过真实 `alloc_page()` 耗尽物理页；满缓存扩容、空缓存首次分配和大块分配失败；无空闲页时复用已有对象；归还页后重新分配 | 不伪造 PMM 状态，验证真实资源不足时返回 NULL、旧状态和数据不变，以及资源恢复后仍能正常使用 |

`check_oom` 用已经分配出去的 `Page.page_link` 暂存借出的页，不接触任何分配器私有空闲链。所有临时页按逆序归还，随后销毁自定义缓存。通用缓存会各保留一个空活动页，所以首轮预热后空闲页减少 7 是实现策略；紧接着重复整套测试，第二轮页数必须不再减少。结束时 `cache_cache` 只有 7 个内置缓存描述符在用，其余 7 个缓存的 `nr_inuse` 都为 0。

大块的页数预期还需区分两种底层管理器。例如请求 9000 和 20000 字节时，SLUB 分别申请 3 和 5 页；Best-Fit 实际占用 3/5 页，Buddy 实际占用 4/8 页。`ksize` 按 SLUB 记录的请求页数返回可用容量，释放仍传原请求页数。测试读取当前 `pmm_manager->name` 判断实际占用预期，因此也支持不执行 clean 的管理器切换。

### 非法释放的隔离测试

panic 后内核会停止，不能将所有负向用例放进一次普通启动。[运行器](../../code/tests/run_slub_tests.py) 在临时源码副本中定义 `SLUB_PANIC_TEST=1..6`，为每个用例重新编译并启动独立 QEMU。除了用例开始标记，还必须匹配 `slub.c` 中的 panic 及具体原因；测试断言失败、调用意外返回所产生的 panic 均不能算通过。

| 编号 | 操作 | 预期诊断 | 实测 |
| :--- | :--- | :--- | :--- |
| 1 | 32 字节对象连续释放两次 | `slub: double free of ... in cache kmalloc-32` | 通过 |
| 2 | 把 32 字节缓存对象交给 64 字节缓存释放 | `slub: ... does not belong to cache kmalloc-64` | 通过 |
| 3 | 释放小对象首址加 1 | `slub: ... is not an object boundary in cache kmalloc-32` | 通过 |
| 4 | 释放 5000 字节大块首址加 1 | `kfree: invalid pointer ...` | 通过 |
| 5 | 释放已清零的普通已分配页中的地址 | `kfree: invalid pointer ...` | 通过 |
| 6 | 大块连续释放两次 | `kfree: invalid pointer ...` | 通过 |

这里检验的是有效映射内的错误地址和立即重复释放，不宣称能识别任意未映射地址或页面重新分配后的陈旧指针；当前实现没有代际标签，也没有并发分配接口，本测试不覆盖多核并发。

### 测试阶段的迭代过程

**第一次迭代：发现并修复极大尺寸溢出。** 首轮编译成功，但内核停在 `assertion failed: kmem_cache_create("overflow", (size_t)-1, 8) == NULL`。原实现先执行 `ROUNDUP(size, align)`，`SIZE_MAX` 会回绕成 0；`offset + stride` 的相加也可能回绕，导致无法容纳的缓存被接受。修复仅在 `cache_setup()` 对齐前增加单页范围检查：`size <= PGSIZE`、归一化后的 `align < PGSIZE`。这样后续运算的两个操作数都有界，原有布局和接口不变。提示词一补充该约束，提示词二保留三个回归输入。[失败日志](../../temp/T4-overflow-before.log)保存了修复前的断言。

**第二次迭代：纠正描述符缓存页数预期。** 批量创建并销毁 40 个缓存后，测试断言 `nr_free_pages() == before` 失败。原因是原描述符页仍保存 7 个通用缓存，而新活动页即使为空也按策略保留，并非实现泄漏。测试先验证合法的“两页、一个 partial、7 个在用描述符”状态；再填满空活动页，令原 partial 页成为活动页，最后释放临时描述符，验证多余页确实归还。[原始日志](../../temp/T4-descriptor-before.log)保留了这次测试设计错误。

**第三次迭代：补齐可重复运行条件。** 自查发现 Makefile 免 clean 切换 PMM 只重编译 `pmm.c`，若测试用编译宏计算 Buddy 页数，就可能沿用旧预期。测试改为读取运行时管理器，随后验证 Best-Fit→Buddy→Best-Fit 双向切换。另一次直接通过 WSL 启动 Python 时出现 `riscv64-unknown-elf-gcc: not found`，改用加载工具链 PATH 的 `bash -lc` 运行即可，未修改内核或工具链。

### 正式验证结果与复现

2026-10-22 使用 WSL Ubuntu 22.04、QEMU 4.1.1/OpenSBI v0.4 完成验证。4 次正常启动各执行两轮完整测试，另外 6 次启动检验预期 panic。完整串口日志及构建日志位于 [`temp/T4-slub-tests/`](../../temp/T4-slub-tests/)，汇总见 [`summary.log`](../../temp/T4-slub-tests/summary.log)。

| 底层分配器 / 内存 | 第一轮空闲页变化 | 第二轮空闲页变化 | 结果 |
| :--- | :--- | :--- | :--- |
| Best-Fit / 128 MiB | 31926 → 31919 | 31919 → 31919 | 全部通过 |
| Best-Fit / 64 MiB | 15702 → 15695 | 15695 → 15695 | 全部通过 |
| Buddy / 128 MiB | 31925 → 31918 | 31918 → 31918 | 全部通过 |
| Buddy / 64 MiB | 15701 → 15694 | 15694 → 15694 | 全部通过 |

各次构建的内核代码大小可能改变初始空闲页数，所以测试采用运行时快照，不硬编码表中的绝对数值。所有正常场景均输出 `[SLUB] ALL TESTS PASSED`；免 clean 切换的三次启动也全部通过。

在已配置交叉工具链的 Linux/WSL 登录 shell 中，从仓库根目录执行：

```bash
export QEMU="$HOME/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64"
python3 code/tests/run_slub_tests.py --qemu "$QEMU" --log-dir temp/T4-slub-tests
cd code
make
make qemu
# 看到 ALL TESTS PASSED 后，Ctrl+A 再按 X 退出。
make grade
```

默认 [`make`](../../temp/T4-make.log) 编译成功，[`make qemu`](../../temp/T4-qemu.log) 正常启动并完成两轮测试；[`make grade`](../../temp/T4-grade.log) 得分 **20/20**，与[修改前基线](../../temp/T4-baseline-grade.log)一致。评分只覆盖原实验要求，SLUB 的独立验证由上述正常与负向测试给出，不能用 20/20 代替 SLUB 测试结论。
