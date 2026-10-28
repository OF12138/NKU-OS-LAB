# T4 指导书思考题；公共章节初稿
- 负责人：nagilix    DDL：思考题 11-03，公共章节 11-06    状态：待开始    依赖：测试截图依赖 T1、T2
- 主要文件：report/sections/T4-questions.md, report/sections/T4-common.md, report/images/T4-*

## 要求
- 指导书正文中的思考题（写成「练习：指导书思考题」一节）：
  1. 「中断与中断处理流程」一节：stvec 最低两位编码模式是 1.10 版加入的，这个改动如何保证与旧版本兼容？
  2. 「断亦有断」一节：`sync.h` 中 `local_intr_save` 宏用 `do { } while (0)` 包起来有什么作用？结合 `pmm.c` 的 `alloc_pages` / `free_pages` 说明为什么要先关中断再操作。
- 公共章节初稿：实验目的（参考指导书「实验目的」）、实验环境（各成员的 AI 工具与模型，向组员确认）、测试与验证（`make qemu` 与 `make grade` 截图，等 T1、T2 完成后拍）、实验总结中「AI 协作开发的经验」。
- 截图按 `report/images/T4-*.png` 命名。

## 当前进度 / 下一步

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
