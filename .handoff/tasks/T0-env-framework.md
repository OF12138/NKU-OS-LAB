# T0 环境与框架
- 负责人：openfar    DDL：11-25    状态：进行中    依赖：无
- 主要文件：code/kern/trap/trapentry.S, code/kern/trap/trap.c, code/kern/mm/pmm.c, code/kern/mm/default_pmm.c, code/kern/process/proc.c, code/kern/process/switch.S, code/kern/schedule/sched.c, code/Makefile

## 要求
- 指导书：“本实验依赖实验2/3/4。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有“lab2”/“lab3”/“lab4”注释的对应位置，确保代码能够正确编译。”
- 指导书：“原来lab3的 RESTORE_ALL 宏，它仅能用于纯内核、无用户态的系统。请在此基础上修改，扩充为支持用户态 / 内核态双向异常返回的完整版上下文恢复宏。”（`kern/trap/trapentry.S:70`、`:77`）
- 标记位置：`default_pmm.c:6`、`pmm.c:90`（lab2）；`trap.c:98`、`trapentry.S:70`、`:77`（lab3）；`pmm.c:324`、`sched.c:26`、`switch.S:3`、`proc.c:85`（lab4）。lab4 中 `alloc_proc`、`copy_thread`、`do_fork`、`kernel_thread`、`proc_init`、`proc_run`、`cpu_idle` 等在 lab5 框架里已被删成标记，需要按标记位置填回，再由 T2～T4 更新。
- 沿用 lab4 对 Makefile 的处理：`make qemu` / `make grade` 使用 QEMU 4.1.1；gcc 14+ 的 `-Wno-error=` 兼容项；评测目录单独关警告（Makefile 中不能出现评测目录的字面路径，否则 inject.py 拒绝注入）。
- 学习注释尽量保留（旧框架原版 → 旧 lab 最终版的改动，叠加到新框架上）。

## 当前进度 / 下一步
- 11-23：框架已导入（提交 `T0: 导入 lab5 框架代码`）。下一步填入 lab2/lab3/lab4 代码并扩充 RESTORE_ALL。

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
