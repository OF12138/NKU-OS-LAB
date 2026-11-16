# Lab4 状态    截止：2026-11-22    集成负责人：openfar

## 目标
进程管理：填入 lab2/lab3 代码，实现功能模块一（`page_insert` / `page_remove`）、功能模块二（进程创建与初始化）、功能模块三（进程调度与 `switch_to`），通过 `make grade`（30/30）；完成 Challenge 两题；交付 report.md 与 prompt.md。

## 任务看板
| ID | 任务 | 负责人 | DDL | 状态 | 主要文件 |
|----|------|--------|-----|------|----------|
| T0 | 环境与框架：填入 lab2/lab3 代码（保留学习注释）、grade 使用 QEMU 4.1.1 | openfar | 11-16 | 完成 | code/kern/mm/default_pmm.c, code/kern/mm/pmm.c, code/kern/trap/trap.c, code/kern/trap/trapentry.S, code/Makefile |
| T1 | 功能模块一：page_insert 与 page_remove | lyp | 11-18 | 待开始 | code/kern/mm/pmm.c, report/sections/T1-* |
| T2 | 功能模块二：alloc_proc、proc_init、kernel_thread、do_fork、copy_thread、copy_mm | openfar | 11-18 | 待开始 | code/kern/process/proc.c, report/sections/T2-* |
| T3 | 功能模块三：cpu_idle、schedule、proc_run、switch_to | openfar | 11-19 | 待开始 | code/kern/process/proc.c, code/kern/schedule/sched.c, code/kern/process/switch.S, report/sections/T3-* |
| T4 | 整体逻辑分析；知识点与 OS 原理对应 | lyp | 11-20 | 待开始 | report/sections/T4-* |
| T5 | Challenge 两题；实验目的、实验环境、测试与验证、实验总结初稿 | nagilix | Challenge 11-19，公共章节 11-21 | 进行中 | report/sections/T5-*, report/images/T5-* |
| T6 | 集成：report.md、prompt.md、最终 make grade | openfar | 11-22 | 待开始 | report/report.md, report/prompt.md |

## 集成状态
- make qemu ⬜   make grade ⬜   report.md 已合并 ⬜   prompt.md 已合并 ⬜

## 全组须知
- 环境（T0，11-15）：lab2/lab3 代码已填入，编译通过，只剩 T1～T3 待实现函数的链接错误。设置 `QEMU=<4.1.1 的完整路径>` 后 `make qemu` / `make grade` 都用 4.1.1。gcc 14 及以上会把 `int-conversion`、`incompatible-pointer-types` 当错误（框架 kmalloc.c 和评测注入的 test_lab4.c 都会触发），Makefile 已用 `-Wno-error=` 降为警告，不要删。
- 内存管理器：lab4 的 `default_pmm.c` 就是 Best-Fit 框架，已填入 lab2 的 Best-Fit 实现，变量名为 `default_pmm_manager`（启动时打印 `best_fit_pmm_manager`）。
- 评测（`tools/grade.sh` + `tools/inject.py`）共 30 分：`test_lab4_pmm_basic()` 插在 `pmm_init()` 末尾（PMM 10 分），`test_lab4_process_create()` 插在 `proc_init()` 末尾（Process Create 10 分），`test_lab4_schedule()` 插在 `init_main()` 开头（Schedule 10 分，还要求输出 `this initproc, pid = 1, name = "init"`、`To U: "Hello world!!".`、`To U: "en.., Bye, Bye. :)"`）。不要改这几个函数的名字，也不要改 `tools/` 下的评测文件。
- 标记注释（`LAB4 functionN YOUR CODE`、`lab2`、`lab3`）保留，`YOUR CODE` 替换为实现者学号；已有的学习注释尽量保留。
