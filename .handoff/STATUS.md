# Lab3 状态    截止：2026-11-08    集成负责人：openfar

## 目标
中断与异常处理：补齐 lab2 代码，实现 `trapentry.S` 的 RESTORE_ALL / `__alltraps`（功能模块一）和 `trap.c` 的 `interrupt_handler` / `exception_handler`（功能模块二），通过 `make grade`（100/100）；交付 report.md 与 prompt.md。

## 任务看板
| ID | 任务 | 负责人 | DDL | 状态 | 主要文件 |
|----|------|--------|-----|------|----------|
| T0 | 环境与框架：填入 lab2 代码、grade 使用 QEMU 4.1.1 | openfar | 10-29 | 完成 | code/kern/mm/pmm.c, code/kern/mm/best_fit_pmm.c, code/Makefile, code/libs/riscv.h |
| T1 | 功能模块一：RESTORE_ALL 与 `__alltraps`（含 GDB 验证 Trap 进出） | openfar | 11-01 | 待开始 | code/kern/trap/trapentry.S, report/sections/T1-* |
| T2 | 功能模块二：interrupt_handler 与 exception_handler（含 GDB 验证 epc 更新） | lyp | 11-02 | 待开始 | code/kern/trap/trap.c, report/sections/T2-* |
| T3 | 整体逻辑分析（openfar）；知识点与 OS 原理对应（lyp） | openfar, lyp | 11-05 | 待开始 | report/sections/T3-* |
| T4 | 指导书思考题；实验目的、实验环境、测试与验证、实验总结初稿 | nagilix | 思考题 11-03，公共章节 11-06 | 待开始 | report/sections/T4-*, report/images/T4-* |
| T5 | 集成：report.md、prompt.md、最终 make grade | openfar | 11-07 | 待开始 | report/report.md, report/prompt.md |

## 集成状态
- make qemu ⬜   make grade ⬜   report.md 已合并 ⬜   prompt.md 已合并 ⬜

## 全组须知
- 环境（T0，10-28）：设置 `QEMU=<4.1.1 的完整路径>` 后，`make qemu` 和 `make grade` 都会用 4.1.1（Makefile 会把它的目录放到 grade.sh 的 PATH 最前面）；不设时 grade 用 PATH 里的 qemu，WSL 自带的 8.2.2 启动不了本框架。
- 框架原样编译不过：`trap.c` 调用的 `interrupt_handler` / `exception_handler` 要等 T2 实现；新版 binutils（2.45）不认识 SAVE_ALL 里的旧 CSR 名 `sbadaddr`，T0 已在 `libs/riscv.h` 里只对汇编 `#define sbadaddr stval`，SAVE_ALL 本身未改。
- `make grade` 会把 `tools/test_lab3.c` 的测试插到 `kern_init()` 的第二个 `idt_init()`、`clock_init()`、`intr_enable()` 之后：不要改 `init.c` 中这几句的写法和顺序，也不要改 `tools/` 下的评测文件。
- 评测按字符串匹配：`Breakpoint at`、`Illegal instruction at`、`Supervisor software interrupt`、`Unknown interrupt: cause 15`、`100 ticks`，未识别异常要走 `print_trapframe`。
- `YOUR CODE` 替换为实现者学号：trapentry.S → 2411264，trap.c → 2414099。
