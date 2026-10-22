## T6：知识点与 OS 原理对应（lyp）

[PROMPT]
任务：撰写 `report/sections/T6-knowledge.md`，说明 Lab2 知识点与 OS 原理的关系，以及本实验未实现的重要机制。操作要求：核对当前代码和实验报告模板，保留他人章节，使用简体中文。输出要求：一张简明对照表和一份未覆盖知识点列表，不重复实现过程，不嵌入提示词。

[RELY]
可信上下文：`report-template.md` 的“对操作系统的理解”要求；`.handoff/tasks/T6-analysis.md` 的职责划分；当前 `code/kern/mm/pmm.c`、`pmm.h`、`memlayout.h`、`default_pmm.c`、`best_fit_pmm.c`、`buddy_pmm.c`、`slub.c` 和 `code/kern/init/entry.S`；已完成的 T5 调试章节。历史初稿只作素材，事实以当前代码和已验证记录为准。

[GUARANTEE]
必须回答：每项知识点在实验中怎样体现，与 OS 原理中的概念有何联系和差异；哪些重要机制尚未实现。交付独立 section，使用 humanizer 润色，执行既有 `merge-report.py --fill`，检查合并内容与章节来源一致，并更新任务状态。

[SPECIFICATION]
Pre-Condition：openfar 的整体逻辑章节已完成；当前任务只补知识点分析，不修改代码。
Post-Condition：正文覆盖页帧管理、连续分配与碎片、伙伴系统、小对象缓存、分页与 TLB；另列未实现的虚拟内存、地址空间及并发等机制。
Case 1：区分页分配与地址映射、4 KiB 管理粒度与 1 GiB 启动映射。
Case 2：说明 Buddy 的幂次取整和伙伴约束，不笼统声称常数复杂度或消除了碎片。
Case 3：区分简化 SLUB、QEMU 软件 TLB 与真实系统机制，不把 Bare 下的软件缓存等同于硬件分页。
Requirements：结论可由当前实现或已有验证支持；不虚构测试或迭代；合并后无待补占位，图号和图片路径有效。
