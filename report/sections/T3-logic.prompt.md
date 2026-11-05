## 实验整体逻辑分析

[PROMPT]

**任务**：为 lab3 实验报告撰写“实验整体逻辑分析”一章，写入 `report/sections/T3-logic.md`，包含“逻辑主线”“功能的逐步实现”“各功能的核心函数”三节，并画一张 Trap 处理全流程图（`report/images/T3-trap-flow.png`）。

**操作要求**：先通读本 lab 已经完成的代码（`trapentry.S`、`trap.c`、`clock.c`、`intr.c`、`sync.h`、`init.c`）和各任务的 section（功能模块一、功能模块二、思考题），以真实代码为准，不复述指导书原文。先用一句话概括主线，再用表格列出实现顺序，逐条说明“为什么排在这里”。流程图用 matplotlib 画成黑白、直角矩形的图片，不用字符画。

**输出要求**：Markdown，语言平实；章节标题由集成脚本编号，文件内从二级标题开始。每个结论都要能在代码或验证记录中找到依据；其他章节的细节只做概括，并写明在报告中的位置。

[RELY]

- `code/kern/init/init.c` 中 `kern_init` 的顺序：`idt_init(); pmm_init(); idt_init(); clock_init(); intr_enable(); while (1);`。
- `code/kern/trap/trap.c`：`idt_init` 执行 `write_csr(sscratch, 0); write_csr(stvec, &__alltraps);`；`trap_dispatch` 以 `(intptr_t)tf->cause < 0` 区分中断与异常。
- `code/kern/trap/trapentry.S`：`SAVE_ALL` 经 `sscratch` 中转保存原 `sp`，在栈上开辟 `36 * REGBYTES` 的 Trap frame；`__alltraps` 以 `.align(2)` 对齐，结尾落入 `__trapret`，执行 `RESTORE_ALL` 后 `sret`。
- `code/kern/trap/trap.h` 中的 `struct pushregs` 与 `struct trapframe`（`gpr`、`status`、`epc`、`badvaddr`、`cause`）。
- `code/kern/driver/clock.c`：`clock_init` 执行 `set_csr(sie, MIP_STIP)` 并调用 `clock_set_next_event()`，后者调用 `sbi_set_timer(get_cycles() + timebase)`；`intr.c` 的 `intr_enable` 置位 `sstatus.SIE`。
- `code/kern/sync/sync.h` 的 `local_intr_save`/`local_intr_restore`，以及 `pmm.c` 中 `alloc_pages`/`free_pages` 对它们的使用。
- `tools/inject.py`：测试分别插在第二个 `idt_init();`、`clock_init();`、`intr_enable();` 之后。
- T1、T2 任务文件中的验证结论（反例实验得分、GDB 观察到的 `sepc`/`pc`）。

[GUARANTEE]

需要回答的问题：

1. 本章围绕什么主题展开，要解决什么问题？用一句话概括主线。
2. 硬件、`trapentry.S`、`trap.c` 在一次 Trap 中各自负责什么？它们之间靠什么衔接？
3. 各功能按什么顺序实现，为什么是这个顺序？每一步依赖前面哪一步的结果？
4. 每个功能的核心函数是什么，哪些由框架给出、哪些由我们实现？

需要交付的产出：Trap 处理全流程图（初始化、硬件响应、`SAVE_ALL`、分派、两个处理函数、`RESTORE_ALL`、`sret` 及返回原执行流的回路，并标注每一步由谁完成）、实现顺序表、核心函数表。

[SPECIFICATION]

**Pre-Condition**：T0～T2 的代码已经完成并通过 `make grade`，各 section 已有初稿。

**Post-Condition**：

- 逻辑主线以“进入—处理—返回”的闭环组织，点明 Trap frame 是汇编与 C 之间的契约这条副线。
- 实现顺序覆盖补齐 lab2 代码、`idt_init`、现场保存与恢复、汇编调用 C、异常处理、中断处理和思考题，每一步说明排序的理由。
- 描述与代码一致，例如：硬件不保存通用寄存器；`sp` 最后恢复；中断返回时不修改 `epc`；时钟定时器每次都要重新设定；关机阈值要大于评测等待的 200 ticks。

**Requirements**：不重复各功能模块章节中的实现细节和迭代过程，只做汇总。
