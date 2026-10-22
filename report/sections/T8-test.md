以下结果均在统一环境（QEMU 4.1.1 / OpenSBI v0.4）下，对 lab2 分支已提交的代码运行得到。

## make qemu

![make qemu 运行截图](../images/T8-make-qemu.png)

<p align="center">图 T8-1　make qemu 的运行结果</p>

输出依次对应启动过程的各个阶段：

- `OpenSBI v0.4`：QEMU 4.1.1 自带的固件，完成 M 态初始化后跳转到 0x80200000 的内核入口。
- `DTB Init` … `DTB init completed`：`dtb_init()` 从 OpenSBI 传来的设备树（地址 0x82200000）中读出物理内存：起始 0x80000000，大小 128 MB。
- `Special kernel symbols`：内核各段的结束地址都是 `0xffffffffc02…` 开头的高地址，说明 entry.S 已经开启 Sv39，内核运行在虚拟地址上；`end = 0xffffffffc0209a60` 对应物理地址 0x80209a60。
- `memory management: best_fit_pmm_manager`：当前的页分配器是 Best-Fit（练习“Best-Fit”）。
- `physical memory map` 一行由 `page_init()` 打印，范围与设备树一致（功能模块一）。
- `check_alloc_page() succeeded!`：Best-Fit 自带的 `best_fit_check()` 通过，包括分配、释放、分裂与合并的基本检查。
- `satp virtual/physical address`：启动页表 `boot_page_table_sv39` 的虚拟地址与物理地址，两者正好相差 `va_pa_offset = 0xFFFFFFFF40000000`。
- `slub: 7 kmalloc caches ...` 由 `slub_init()` 打印，SLUB 的 7 个通用缓存建立完成；其后的 `[SLUB] ...` 各行是 `slub_check()` 的启动自测：逐个缓存填满一个 slab 并触发新 slab 分配、检查 partial 状态转换和空 slab 回收、批量分配后释放一半再复用，以及超过 1024 字节时退回整页分配。最后打印各缓存的统计并以 `[SLUB] all checks passed` 结束（Challenge：SLUB）。

## make grade

![make grade 运行截图](../images/T8-make-grade.png)

<p align="center">图 T8-2　make grade 的运行结果</p>

`make grade` 把代码复制到 `/tmp/lab2_grade`，由 `tools/inject.py` 在 `pmm_init()` 末尾插入 `test_page_init()` 和 `test_best_fit()` 两个测试函数，再运行内核、检查输出。两项都通过，总分 **20/20**。两项分别检查：

| 检查项 | 实际检查的内容 |
| :--- | :--- |
| check page init | `va_pa_offset` 等于 `PHYSICAL_MEMORY_OFFSET`；`npage` 等于设备树内存上界的页号；`pages` 页对齐且位于内核 `end` 之后；空闲页数大于 0 且不超过总页数；内核末页和描述符数组所在页是保留页；`pa2page(page2pa(p)) == p` |
| check best fit | 只检查当前 `pmm_manager` 是否为 `best_fit_pmm_manager` |

可以看出，`check best fit` 并不检查 Best-Fit 的选择和合并是否正确，这部分由 `best_fit_check()`（`make qemu` 中的 `check_alloc_page() succeeded!`）和各模块自己的测试负责。

## 各模块的补充测试

| 模块 | 测试方式 | 结果 |
| :--- | :--- | :--- |
| page_init | 用 `-m 64M`、默认 128M、`-m 256M` 启动，打印 `npage` 和空闲页数 | 空闲页 15705、31929、31929，与手算一致；256M 时上界被截断到 0x88000000 |
| Best-Fit | `code/tests/run_best_fit.sh`：在临时副本中接入 `best_fit_regression.c`，检查最小块选择、同大小选低地址、分裂、碎片导致分配失败、前后合并 | 三组检查全部 PASS，测试后空闲页恢复为 31929 |
| Buddy System | 由提示词二生成的测试函数 | 待补（T3 完成后填写） |
| SLUB | 由提示词二生成的测试函数 | 待补（T4 测试部分完成后填写） |
