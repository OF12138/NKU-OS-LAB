# T2 功能模块二 任务二：Best-Fit 连续物理内存分配
- 负责人：lyp    DDL：10-20    状态：待开始    依赖：T1
- 主要文件：code/kern/mm/best_fit_pmm.c, code/kern/mm/pmm.c, report/sections/T2-*

## 要求
指导书原文要点：
- 参考 `kern/mm/default_pmm.c` 的 First-Fit 实现，设计提示词，让 AI 在 `kern/mm/best_fit_pmm.c` 中标有 lab2 和 YOUR CODE 的位置实现完整的 Best-Fit 物理内存管理模块。
- 文件只提供 `basic_check()` 和 `best_fit_check()`，不提供管理函数声明和 `best_fit_pmm_manager` 注册结构；需根据 `pmm.h` 中的 `struct pmm_manager` 自行设计并注册。实现必须符合测试函数使用的状态约定，**不得修改两个测试函数**。
- 在 `kern/mm/pmm.c` 的 `init_pmm_manager()` 中切换为 `best_fit_pmm_manager`。
- 报告：提交提示词（功能模块：最终提示词 + 实现迭代过程），回答“你的 Best-Fit 算法是否有进一步的改进空间？”
- 评测：`make grade` 的 `[Test 2] Best-Fit: PASS`。

## 当前进度 / 下一步
- 等 T1（page_init，10-18）完成后开始；在此之前可以先读 default_pmm.c、best_fit_pmm.c 的测试函数和 pmm.h。

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
