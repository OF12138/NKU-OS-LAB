# T1 功能模块一：虚拟内存页操作（page_insert / page_remove）
- 负责人：lyp    DDL：11-18    状态：待开始    依赖：T0
- 主要文件：code/kern/mm/pmm.c, report/sections/T1-page-insert.md, report/sections/T1-page-insert.prompt.md

## 要求
- 指导书「功能模块一」：在 `kern/mm/pmm.c` 中实现（标记 `//LAB4 function1 YOUR CODE`）：
  - `int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);`
  - `void page_remove(pde_t *pgdir, uintptr_t la);`
- 注意事项（指导书原意）：保持“页表项内容、物理页引用计数、TLB 状态”三者同步；`page_insert` 要正确处理原地址已映射到同一物理页、已映射到其他物理页两种情况；`get_pte(pgdir, la, 1)` 可能失败，此时返回 `-E_NO_MEM` 且不留半完成映射；`page_remove` 是幂等接口，未映射时安静返回；更新后要使对应 TLB 失效。
- 可用接口：`get_pte()`、`pte_create()`、`pte2page()`、`page_ref_inc/dec()`、`set_page_ref()`、`alloc_page()`、`free_page()`、`tlb_invalidate()`、`flush_tlb()`，权限位 `PTE_V/U/W/R`。
- 评测：`tools/test_lab4.c` 的 `test_lab4_pmm_basic()`（插在 `pmm_init()` 末尾），输出 `[Test PMM] PASS` 得 10 分。它覆盖：同地址换页时旧页引用计数归零、同页重复插入只改权限且不泄漏页、重复 remove 不报错。
- 提示词：按四段式写一份，[RELY] 引用框架原始代码（上述函数、宏、`struct Page`）。
- 报告 section（模板「功能模块」）：模块功能描述、最终提示词（`<!-- include:T1-page-insert.prompt.md -->`）、实现迭代过程。
- 代码注释中的 `YOUR CODE` 替换为学号 2414099。

## 当前进度 / 下一步

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
