## 实验整体逻辑分析

[PROMPT]

**任务**：为 lab2 实验报告撰写“实验整体逻辑分析”一章，写入 `report/sections/T6-overall.md`，包含“逻辑主线”“功能的逐步实现”“各功能的核心函数”三节。

**操作要求**：先通读本 lab 已经完成的代码和各任务的 section（功能模块一、功能模块二的三个任务、SLUB、分支任务），以真实代码为准，不复述指导书原文。参考 lab1 报告中“整体分析”一章的写法：先用一句话概括主线，再用表格列出实现顺序，逐条说明“为什么排在这里”。

**输出要求**：Markdown，语言平实；章节标题由集成脚本编号，文件内从二级标题开始。每个结论都要能在代码中找到依据；涉及其他章节的细节只做概括，并写明在报告中的位置。

[RELY]

- 指导书给出的实现链：`pmm_init() -> init_pmm_manager() -> page_init() -> init_memmap() -> alloc_pages()/free_pages()`。
- `code/kern/init/entry.S`：用一个 1 GiB 大页建立 `boot_page_table_sv39`，写入 `satp` 后跳到高地址的 `kern_init`。
- `code/kern/mm/pmm.h` 中的 `struct pmm_manager`（`init`、`init_memmap`、`alloc_pages`、`free_pages`、`nr_free_pages`、`check`）和 `memlayout.h` 中的 `struct Page`。
- 各分配器的实现：`default_pmm.c`、`best_fit_pmm.c`、`buddy_pmm.c`、`slub.c`，以及 `page_init`。
- 分支任务中观察到的 `get_physical_address` 页表遍历过程（`temp/gdb-guide.md`）。

[GUARANTEE]

需要回答的问题：

1. 本章围绕什么主题展开，要解决什么问题？用一句话概括主线。
2. 各功能按什么顺序实现，为什么是这个顺序？每一步依赖前面哪一步的结果？
3. 每个功能的核心函数是什么，它的关键逻辑是什么？
4. 几个页分配器之间有什么共同点，为什么可以互相替换？

需要交付的产出：分层示意图、实现顺序表、`pmm_manager` 接口表、`struct Page` 字段在各分配器中的含义对照表。

[SPECIFICATION]

**Pre-Condition**：T1～T5 的代码已经完成，各 section 已有初稿。

**Post-Condition**：

- 逻辑主线按层次组织（地址 → 内存范围 → 页描述符 → 连续页 → 对象），并点明区分虚拟地址、物理地址、物理页号和描述符地址这条副线。
- 实现顺序覆盖框架已完成的启动页表、`page_init`、First-Fit、Best-Fit、Buddy、SLUB 和分支任务，每一步都说明排序的理由。
- 核心函数的描述与代码一致，例如 Best-Fit 在大小相同时选地址最低的块，First-Fit 分配后不清零 `property`，Buddy 已分配块的首页仍记录块长。

**Requirements**：不重复各功能模块章节中的实现细节和迭代过程，只做汇总。
