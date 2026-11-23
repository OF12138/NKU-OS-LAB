# T5 Challenge 1：Copy on Write 与 Dirty COW
- 负责人：openfar    DDL：12-03    状态：待开始    依赖：T3
- 主要文件：code/kern/mm/*, code/kern/trap/trap.c, code/user/*, report/sections/T5-*

## 要求
- 指导书「扩展练习 Challenge」：在 ucore 中实现 COW（fork 时父子共享只读页，写入时由 page fault 复制）；参考 https://dirtycow.ninja/ ，尝试在 ucore 的 COW 实现中模拟 Dirty COW 错误及其解决方案并解释；提供实现代码、测试用例和设计报告（含 COW 各种状态转换，类似有限状态自动机的说明）。
- COW 不能降低 `make grade` 的得分（必要时用编译开关隔离）。
- 交付：`report/sections/T5-cow.md`、`T5-cow.prompt.md`（实现与测试各一份）。

## 当前进度 / 下一步
- 待开始。

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
