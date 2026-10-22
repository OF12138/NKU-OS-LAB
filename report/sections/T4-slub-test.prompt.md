## Challenge：SLUB 分配器（提示词二：独立测试）

[PROMPT]

**任务**：针对提示词一生成并由 openfar 接入的 SLUB，实现独立测试函数，检验不同大小内存对象的分配和释放。测试负责人为 2414099-李云鹏。新建 `code/kern/mm/slub_test.c`，提供 `slub_check()`；在 `slub.h` 声明，并在 `kern_init()` 的 `slub_init()` 之后调用。另建 `code/tests/run_slub_tests.py`，在临时副本中编译、逐次启动 QEMU，验证正常场景和会停机的负向场景。

**操作要求**：直接编辑真实文件，保留现有学习注释和他人实现；不得修改 `code/tools/` 中的评分/注入脚本、Best-Fit 的原始测试函数及 `struct Page` 布局。通过公开接口调用分配器，可读取 `slub.h` 公开的状态，但不得修改分配器私有变量或伪造 PMM 的空闲页数。发现实际缺陷时先保存失败日志，再做最小修复，留下回归测试和交接记录。

**输出要求**：交付可重复运行的 C 测试、Python 标准库运行脚本、QEMU 原始日志；报告说明每组测试的行为、边界和检错依据，并如实记录遇到的问题。统一使用 QEMU 4.1.1/OpenSBI v0.4，分别测试 Best-Fit/Buddy、128/64 MiB；默认 `make`、`make qemu` 和 `make grade` 必须仍可用，评分不低于修改前。

[RELY]

下面的页管理接口来自导入框架的提交 `84603fb`，省略学习注释；SLUB 接口来自本任务测试开始前已有的提示词一产物，不是待生成测试的代码。

```c
// code/kern/mm/pmm.h：框架提供
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)
extern size_t npage;
extern uint64_t va_pa_offset;
static inline uintptr_t page2pa(struct Page *page) {
    return page2ppn(page) << PGSHIFT;
}

// code/kern/mm/mmu.h
#define PGSIZE          4096

// code/libs/error.h
#define E_INVAL             3
```

```c
// code/kern/mm/slub.h：openfar 提供的被测接口
#define SLUB_MIN_ALIGN      sizeof(void *)
#define SLUB_MAX_SIZE       1024
#define KMALLOC_NR_CACHES   7
void slub_init(void);
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align);
int kmem_cache_destroy(struct kmem_cache *cache);
void *kmem_cache_alloc(struct kmem_cache *cache);
void kmem_cache_free(struct kmem_cache *cache, void *obj);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t ksize(const void *ptr);
struct kmem_cache *kmalloc_cache(size_t size);
void slub_print_stats(void);
```

- 阅读现有 `slub.h` 的完整 `struct slab` 和 `struct kmem_cache`。缓存公开 `size`、`align`、`offset`、`objs_per_slab`、`cpu_slab`、`nr_slabs`、`nr_partial`、`nr_inuse`。slab 页首公开 `inuse` 和 `cache`，可用对象地址向下对齐到 `PGSIZE` 得到所属 slab。
- 每个 slab 占一页。活动 slab 变空保留；非活动 slab 变空归还。满 slab 不在 partial 链中，释放首个对象后才进入 partial；活动页满时优先提升 partial 页。freelist 按后进先出复用。
- `kmalloc` 的 7 个缓存大小为 16～1024；大块头部是 `ROUNDUP(sizeof(struct slab), 16)` 字节。大块 `ksize` 为请求页数乘 `PGSIZE` 再减头部。
- `cache_cache` 管理缓存描述符，7 个通用缓存描述符长期在用。测试批量创建/销毁描述符时，也要考虑它的活动 slab 保留策略。
- 已接入的 Buddy 通过 `make PMM=buddy` 选择，实际页占用向上取整到 2 的幂，释放传原请求页数；默认 Best-Fit 按请求页数占用。不能拿 Best-Fit 页数断言直接检查 Buddy。
- 使用框架的 `assert`、`panic`、`cprintf`、`memset`、`ROUNDUP`、`ROUNDDOWN` 和 `list.h`。panic 打印文件、行号和原因后无限循环，不能在同一次启动里继续后续测试。

[GUARANTEE]

```c
void slub_check(void);
static void check_invalid(void);
static void check_sizes(void);
static void check_partial(void);
static void check_custom(void);
static void check_large(void);
static void check_oom(void);
static void check_panic(void);
```

运行脚本入口为 `main()`，支持 `--qemu` 和 `--log-dir`；默认运行 4 次正常启动与 6 次隔离的负向启动，任一失败必须以非零退出码结束。

[SPECIFICATION]

### slub_check

**Pre-Condition**：`pmm_init()`、`slub_init()` 已完成，单核、没有并发分配者；各通用缓存尚无在用对象。

**Post-Condition**：运行两轮全部正常用例，每组通过后打印 PASS，最后打印统计及 `[SLUB] ALL TESTS PASSED`。自定义缓存、对象及直接申请的页全部释放。第一轮仅允许 7 个通用缓存各保留一页，第二轮空闲页数不得继续下降。

**Case 1**：`SLUB_PANIC_TEST` 为 1～6 时，仅执行对应负向用例。

**Requirements**：不能只验证返回指针非空；同时检查对象内容、地址不重叠、统计状态和 PMM 页数。批量指针表放 BSS，避免内核栈溢出。

### check_invalid

**Pre-Condition**：SLUB 初始化完成。

**Post-Condition**：验证零长度、NULL、非法对齐、一页容纳不下、`SIZE_MAX`、接近 `SIZE_MAX` 的加法回绕、极大对齐及超物理容量请求；拒绝销毁通用缓存。调用前后空闲页数一致，合法对象与缓存仍可使用。

**Case 1**：对齐小于指针大小按最小对齐处理；不要把 `align=0` 误判为非法。

### check_sizes

**Pre-Condition**：通用缓存可用。

**Post-Condition**：遍历请求 1～1024，并让 22 个边界请求同时存活，包括每个大小类的边界及相邻值。验证最小适配缓存、`ksize`、至少 8 字节对齐；写入包含对象编号及字节位置的模式，交错释放和重新分配后检查其余对象内容，再逆序释放。

**Requirements**：填充并校验整个 `ksize` 可用范围。结束后各缓存 `nr_inuse=0`、`nr_partial=0`、`nr_slabs=1`。

### check_partial

**Pre-Condition**：创建独立的 64 字节缓存，容量从 `objs_per_slab` 读取。

**Post-Condition**：填满两页，使第一页非活动且满；释放第一页两个对象，验证 partial 只增加一次；从满的活动页分配时必须提升第一页并后进先出复用，不能申请第三页。再次校验所有对象内容，再释放非活动页和活动页，分别验证立即回收与保留行为；销毁后恢复全部页数。

### check_custom

**Pre-Condition**：可创建、销毁自定义缓存。

**Post-Condition**：覆盖 1 字节/默认对齐、40/16 对齐、65/64 对齐、3000/64 对齐和最大单页可容纳对象；核对实际步长与地址，带在用对象时销毁返回 `-E_INVAL` 且状态、数据不变。单对象非活动 slab 必须能从满直接变空，回收后 partial 数不能下溢。

**Case 1**：同时创建 40 个缓存以跨越描述符 slab，销毁后允许“内置描述符页 + 空活动页”的两页状态；随后填满空活动页、提升内置描述符所在 partial 页并释放临时缓存，确认额外页回收且 7 个内置描述符仍存活。

### check_large

**Pre-Condition**：页分配器可用。

**Post-Condition**：同时分配 1025、一页可用容量、容量加一、4096、4097、9000、20000 字节；按头部计算请求页数与 `ksize`，核对 16 字节对齐和每次分配的真实页数，逐字节检查内容，释放后空闲页数完全恢复。

**Case 1**：Buddy 下计费按 2 的幂取整；`ksize` 仍按 SLUB 记录的原请求页数计算。根据运行时实际 `pmm_manager->name` 判断页数预期，支持免 clean 切换 PMM，不能依赖未触发重编译的测试对象中的旧宏。

### check_oom

**Pre-Condition**：先建立一个满的单对象缓存和一个尚未分配 slab 的缓存。

**Post-Condition**：通过 `alloc_page()` 真正耗尽 PMM，并用已分配 `Page.page_link` 暂存这些页；验证满缓存扩容、空缓存首分配以及大块请求均返回 NULL，原对象内容和缓存状态不变。无空闲物理页时释放并复用已有活动对象仍成功；归还所有临时页后，新分配恢复正常；最终页数回到初值。

### check_panic

**Pre-Condition**：每个用例从新启动的内核运行，先确保测试对象分配成功。

**Post-Condition**：分别触发小对象重复释放、错缓存释放、小对象内部地址、大块内部地址、无魔数的已分配普通页地址、大块重复释放；打印用例开始标记，要求在 `slub.c` 中产生对应诊断。若调用返回，触发明确的失败 panic，不能误记为通过。

### main（Python 运行器）

**Pre-Condition**：Linux/WSL，交叉工具链位于 PATH，已安装 QEMU 4.1.1。

**Post-Condition**：在临时目录复制源码、构建并运行；保存构建和串口输出。正常测试要求两轮 PASS、页数保留分别为 7/0、无 panic；负向测试要求对应开始标记、`slub.c` 文件位置及准确原因，而不是把任意 panic 当成功。

**Requirements**：每次改变条件编译宏前 clean；不污染工作区源码或现有构建产物。超时只是内核停机循环的预期现象，必须结合日志标记判断结果；QEMU 提前退出、版本不符或缺少标记都应失败。
