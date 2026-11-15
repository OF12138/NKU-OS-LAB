# T0 环境与框架
- 负责人：openfar    DDL：11-16    状态：待开始    依赖：无
- 主要文件：code/kern/mm/default_pmm.c, code/kern/mm/pmm.c, code/kern/trap/trap.c, code/kern/trap/trapentry.S, code/Makefile

## 要求
- 指导书：“本实验依赖实验2/3。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有‘lab2’/‘lab3’注释的对应位置，确保代码能够正确编译。”
- 框架中的标记：`kern/mm/default_pmm.c:6`、`kern/mm/pmm.c:90`（lab2）；`kern/trap/trap.c:86`、`kern/trap/trapentry.S:56`、`:63`（lab3）。注意 lab4 的 lab2 标记在 `default_pmm.c`，不是 lab3 的 `best_fit_pmm.c`。
- lab2/lab3 代码中的学习注释尽量保留（参照 lab3 T0 的三方合并做法：旧框架原版 → 旧 lab 最终版的改动，叠加到 lab4 框架上），代码以 lab4 框架为准。
- 按 lab3 T0 的做法让 `make qemu` / `make grade` 都用 QEMU 4.1.1；检查 `sbadaddr` 等旧 CSR 名在新 binutils 下能否汇编。

## 当前进度 / 下一步
- 11-15：框架已导入（提交 `T0: 导入 lab4 框架代码`）。下一步填入 lab2/lab3 代码。

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
