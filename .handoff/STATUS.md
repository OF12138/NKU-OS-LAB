# Lab2 状态    截止：2026-10-26    集成负责人：openfar

## 目标
物理内存管理：实现 page_init、Best-Fit、Buddy System、SLUB（Challenge）并通过 `make grade`；完成 First-Fit 分析、内存探测思考题和分支任务（GDB 调试 QEMU 页表查询）；交付 report.md 与 prompt.md。

## 任务看板
| ID | 任务 | 负责人 | DDL | 状态 | 主要文件 |
|----|------|--------|-----|------|----------|
| T0 | 环境与框架：导入框架、统一 QEMU 4.1.1、分配器切换开关 | openfar | 10-17 | 完成 | code/Makefile, temp/env.md |
| T1 | 功能模块一 任务一：page_init | openfar | 10-18 | 完成 | code/kern/mm/pmm.c, report/sections/T1-* |
| T2 | 功能模块二 任务二：Best-Fit | lyp | 10-20 | 完成 | code/kern/mm/best_fit_pmm.c, code/kern/mm/pmm.c, report/sections/T2-* |
| T3 | 功能模块二 任务三：Buddy System（实现 lyp / 测试 openfar） | lyp, openfar | 实现 10-22，测试 10-23 | 完成 | code/kern/mm/buddy_pmm.*, report/sections/T3-* |
| T4 | Challenge：SLUB（实现 openfar / 测试 lyp） | openfar, lyp | 实现 10-22，测试 10-23 | 完成 | code/kern/mm/slub.*, report/sections/T4-* |
| T5 | 分支任务：GDB 调试 QEMU 页表查询（openfar：调用路径与页表遍历 / lyp：TLB） | openfar, lyp | 10-24 | 进行中 | report/sections/T5-*, report/images/T5-* |
| T6 | 整体逻辑分析（openfar）；知识点与 OS 原理对应（lyp） | openfar, lyp | 10-24 | 待开始 | report/sections/T6-* |
| T7 | 功能模块二 任务一：First-Fit 分析；功能模块一 Challenge：内存范围获取 | nagilix | 10-21 | 完成 | report/sections/T7-* |
| T8 | 实验目的、实验环境、测试与验证、实验总结初稿 | nagilix | 10-24 | 进行中 | report/sections/T8-*, report/images/T8-* |
| T9 | 集成：report.md、prompt.md、最终 make grade | openfar | 10-25 | 待开始 | report/report.md, report/prompt.md |

## 集成状态
- make qemu ⬜   make grade ⬜   report.md 已合并 ⬜   prompt.md 已合并 ⬜

## 全组须知
- T4 lyp 测试已简化为单次启动自测：各大小缓存、跨页扩展、释放复用和大对象回退通过，默认评分 20/20；报告/提示词二同步重写，日志仅保留 `temp/T4-{make,qemu,grade}.log`。T9 请采用本地新版。
- T3 Buddy 完成（10-22）：`make PMM=buddy` 启用，默认仍为 best_fit；`make PMM=buddy qemu` 会依次运行 lyp 的自检和 openfar 的独立测试 `buddy_test()`。
- 分支任务的双 GDB 教程见 `temp/gdb-guide.md`（命令和输出均已在 2bc1801 的代码上实测；代码改动会改变地址，按教程 3.3 节重新查）。
- 先按 `temp/env.md` 装好 QEMU 4.1.1 并设置 `QEMU` 环境变量，再跑 `make qemu` / `make grade`。
- `make grade` 会把测试插到 `pmm_init()` 末尾并检查 `[Test 1] Page Init: PASS`、`[Test 2] Best-Fit: PASS`：不要改 `pmm_init` 的函数名、`pmm.c` 中的 `#include <pmm.h>`，默认管理器必须是 best_fit。
