## Challenge：SLUB 分配器（提示词二：测试函数）

[PROMPT]

**任务**：为 openfar 已实现的 SLUB 编写简洁的启动自测。参考前辈报告中“各大小缓存分配与释放、填满后扩展、批量复用、大对象页级回退”的测试思路，针对本项目的真实接口独立设计断言，不复制前辈代码。测试负责人为 2414099-李云鹏。

**操作要求**：直接修改 `code/kern/mm/slub_test.c`，提供 `slub_check()`，由 `kern_init()` 在 `slub_init()` 之后调用。保留现有实现和学习注释，不修改评分脚本、Best-Fit 原始测试及 `struct Page`。全部测试在一次正常 `make qemu` 中完成，采用 `assert` 和简短输出，不引入额外运行脚本。

**输出要求**：交付测试函数、覆盖说明和实际编译/启动/评分结果。使用 QEMU 4.1.1，默认 Best-Fit 环境即可。

[RELY]

页管理接口来自导入框架的提交 `84603fb`；SLUB 接口是提示词一已生成的被测模块，省略注释。

```c
// code/kern/mm/pmm.h
size_t nr_free_pages(void);
// code/kern/mm/mmu.h
#define PGSIZE          4096

// code/kern/mm/slub.h
#define SLUB_MIN_ALIGN      sizeof(void *)
#define SLUB_MAX_SIZE       1024
#define KMALLOC_NR_CACHES   7
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t ksize(const void *ptr);
struct kmem_cache *kmalloc_cache(size_t size);
void slub_print_stats(void);
```

- 读取现有 `slub.h` 中 `struct kmem_cache` 的 `size`、`objs_per_slab`、`nr_slabs`、`nr_partial`、`nr_inuse`，不修改这些字段。
- 七种缓存为 16～1024 字节；每个 slab 占一页。活动 slab 不在 partial 链上，变空保留；非活动满 slab 释放首个对象后进入 partial，变空则归还整页。
- 请求超过 1024 字节由 `kmalloc` 直接申请连续页，`kfree` 根据头部归还，`ksize` 返回可用容量。
- 可使用 `assert`、`cprintf`、`memset`、`ROUNDDOWN`；panic 会停机。

[GUARANTEE]

```c
void slub_check(void);
static void check_cache(size_t request, size_t expected);
static void check_reuse(void);
static void check_large(void);
```

[SPECIFICATION]

### slub_check

**Pre-Condition**：`pmm_init()`、`slub_init()` 已完成；单核，无并发分配；七个通用缓存尚未分配对象页。

**Post-Condition**：依次执行参数、各大小缓存、批量复用和大对象测试，全部成功后打印统计和完成标记。所有对象已释放，底层只比测试前少七个通用缓存保留的活动页。

**Case 1**：`kmalloc(0)` 返回 NULL，`kfree(NULL)` 可安全返回。

**Case 2**：`kmem_cache_create("overflow", (size_t)-1, 8)` 返回 NULL，保留此前已修复溢出的回归。

### check_cache

**Pre-Condition**：请求与期望缓存对应：1/17/33/65/129/257/1024 字节分别选择 16/32/64/128/256/512/1024 字节缓存；该缓存无在用对象、最多保留一个空活动页。

**Post-Condition**：按 `objs_per_slab` 填满一页，核对地址对齐、同页关系、`ksize`、在用数；额外申请一个对象，必须位于另一页。向每个对象写入不同的字节值并逐字节校验。释放非活动页第一个对象时出现 partial，释放剩余对象后整页回收；最后释放额外对象，只保留一个空活动页。

### check_reuse

**Pre-Condition**：64 字节缓存已预热且无在用对象。

**Post-Condition**：分配 128 个对象，释放偶数号对象，再补齐；在用数依次为 128、64、128，补齐时不增加 slab 或消耗新页。重新校验所有对象内容，全部释放后页数恢复至本测试开始时。

**Requirements**：指针表放 BSS，避免批量对象地址占满内核栈。

### check_large

**Pre-Condition**：页分配器正常工作。

**Post-Condition**：分别申请 1025、4096、8192 字节，覆盖小对象上限之外、恰好一页的请求和多页请求；检查返回地址、可用容量，写满请求范围并读回，释放后空闲页数恢复。页首还有管理头，不把 4096 字节请求误认为只需一页。
