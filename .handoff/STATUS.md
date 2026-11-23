# Lab5 状态    截止：2026-12-06    集成负责人：openfar

## 目标
用户进程管理：填入 lab2/lab3/lab4 代码并把 RESTORE_ALL 扩充为支持用户态/内核态双向返回；实现功能模块一～四（系统调用、第一个用户进程、fork 复制、exit/yield/kill），通过 `make grade`；完成分支任务（双 gdb 观察 ecall/sret）和 Challenge（COW 与 Dirty COW、用户程序加载时机）；交付 report.md 与 prompt.md。

## 任务看板
| ID | 任务 | 负责人 | DDL | 状态 | 主要文件 |
|----|------|--------|-----|------|----------|
| T0 | 环境与框架：填入 lab2/lab3/lab4 代码、扩充 RESTORE_ALL、grade 使用 QEMU 4.1.1 | openfar | 11-25 | 进行中 | code/kern/trap/trapentry.S, code/kern/trap/trap.c, code/kern/mm/pmm.c, code/kern/mm/default_pmm.c, code/kern/process/proc.c, code/kern/process/switch.S, code/kern/schedule/sched.c, code/Makefile |
| T1 | 功能模块一：用户态系统调用（exception_handler、syscall、sys_*） | lyp | 11-28 | 待开始 | code/kern/trap/trap.c, code/kern/syscall/syscall.c, report/sections/T1-* |
| T2 | 功能模块二：创建加载第一个用户进程（init_main、user_main、kernel_execve、do_execve、load_icode、do_wait 等） | openfar | 11-29 | 待开始 | code/kern/process/proc.c, report/sections/T2-* |
| T3 | 功能模块三：复制用户进程（do_fork、copy_mm、dup_mmap、copy_range） | openfar | 11-30 | 待开始 | code/kern/process/proc.c, code/kern/mm/vmm.c, code/kern/mm/pmm.c, report/sections/T3-* |
| T4 | 功能模块四：do_exit、do_yield、do_kill | lyp | 11-30 | 待开始 | code/kern/process/proc.c, report/sections/T4-* |
| T5 | Challenge 1：Copy on Write 与 Dirty COW | openfar | 12-03 | 待开始 | code/kern/mm/*, code/kern/trap/trap.c, code/user/*, report/sections/T5-* |
| T6 | 分支任务：双 gdb 观察 ecall 与 sret | lyp | 12-02 | 待开始 | report/sections/T6-*, report/images/T6-* |
| T7 | 整体逻辑分析；知识点与 OS 原理对应 | nagilix | 12-03 | 待开始 | report/sections/T7-* |
| T8 | 公共章节；Challenge 2：用户程序何时加载 | nagilix | 12-04 | 待开始 | report/sections/T8-*, report/images/T8-* |
| T9 | 集成：report.md、prompt.md、最终 make grade | openfar | 12-06 | 待开始 | report/report.md, report/prompt.md |

## 集成状态
- make qemu ⬜   make grade ⬜   report.md 已合并 ⬜   prompt.md 已合并 ⬜

## 全组须知
- 框架（11-23）：lab5 框架已导入 `code/`（labcode lab5.zip，11-17 版）。`proc.c` 中 lab4 的函数基本都被删成了标记，T0 会先把 lab4 实现填回去并跑通编译；T1～T4 等 T0 完成后再改代码，报告可以先动笔。
- 评测：`tools/grade.sh` + `tools/inject.py` 在 `proc_init()` 末尾注入 Process Create、在 `init_main()` 开头注入 Syscall 与 Process Control 三个测例，并用 exit、spin、forktest 等用户程序检查输出。不要改 `tools/` 下的评测文件和这几个函数的名字。
- 标记注释（`lab2`、`lab3`、`lab4`、`lab5 functionN YOUR CODE`）保留，`YOUR CODE` 替换为实现者学号；已有的学习注释尽量保留。
- 提示词：功能模块（写代码的练习）和 Challenge 的实现需要四段式提示词；分析类 section 不需要。
