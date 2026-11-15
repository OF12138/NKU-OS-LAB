# T3 功能模块三：进程调度
- 负责人：openfar    DDL：11-19    状态：待开始    依赖：T2
- 主要文件：code/kern/process/proc.c, code/kern/schedule/sched.c, code/kern/process/switch.S, report/sections/T3-schedule.md, report/sections/T3-schedule.prompt.md

## 要求
- 指导书「功能模块三」（标记 `LAB4 function3 YOUR CODE`）：`cpu_idle`、`proc_run`（proc.c）、`schedule`（sched.c）、`switch_to`（switch.S）。
- 注意事项：`schedule` / `proc_run` 的关键区域要关中断（`local_intr_save/restore`）；`schedule` 找不到可运行线程时退回 `idleproc`；`proc_run` 更新 `current`、切换页表（Sv39 satp，不能用 Sv32 遗留的 `lsatp()`，参照 `enable_paging()`）并调用 `switch_to`；`switch_to` 只保存/恢复 `struct context`。
- 评测：`test_lab4_schedule()`（插在 `init_main()` 开头），两个测试线程都运行过并输出 `[Test Schedule] PASS`，另需 `this initproc, pid = 1, name = "init"` 和两行 `To U: ...`，得 10 分。
- 提示词：四段式一份；报告 section：模块功能描述、最终提示词、实现迭代过程。

## 当前进度 / 下一步

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
