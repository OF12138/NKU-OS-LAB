# T5 Challenge；公共章节初稿
- 负责人：nagilix    DDL：Challenge 11-19，公共章节 11-21    状态：待开始    依赖：测试截图依赖 T1～T3
- 主要文件：report/sections/T5-challenge.md, report/sections/T5-common.md, report/images/T5-*

## 要求
- 指导书「扩展练习 Challenge」（写成报告中的 Challenge 一节）：
  1. 说明语句 `local_intr_save(intr_flag); ... local_intr_restore(intr_flag);` 是如何实现开关中断的（结合 `kern/sync/sync.h`、`libs/riscv.h` 中的 sstatus.SIE 读写，以及本 lab 中 `schedule` / `proc_run` 为什么需要它）。
  2. 深入理解不同分页模式的工作原理（思考题）：
     - `get_pte()`（`kern/mm/pmm.c`）中有两段形式类似的代码，结合 sv32、sv39、sv48 的异同，解释这两段代码为什么如此相像。
     - 目前 `get_pte()` 将页表项查找和页表页分配合并在一个函数里，这种写法好吗？有没有必要把两个功能拆开？
- 公共章节初稿：实验目的（参考指导书「实验目的」）、实验环境（各成员的 AI 工具与模型，向组员确认）、测试与验证（`make qemu` 与 `make grade` 截图，等 T1～T3 完成后拍）、实验总结中「AI 协作开发的经验」。
- Challenge 附一份四段式提示词（`T5-challenge.prompt.md`）；截图按 `report/images/T5-*.png` 命名。

## 当前进度 / 下一步

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
