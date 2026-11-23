# T4 功能模块四：do_exit、do_yield、do_kill
- 负责人：lyp    DDL：11-30    状态：待开始    依赖：T0；与 T2 的 do_wait 配合
- 主要文件：code/kern/process/proc.c, report/sections/T4-*

## 要求
- 指导书「功能模块四：其余系统调用功能」。要求实现：`do_exit`、`do_yield`、`do_kill`（`kern/process/proc.c`）。
- 注意事项（指导书）：idleproc 和 initproc 不能退出；先处理资源再变成僵尸；父子关系、等待状态和僵尸回收必须和 `do_wait` 完全匹配；do_kill 要考虑目标是否在等待、是否需要唤醒；状态转换放在临界区内。
- 评测 `test_lab5_process_control`：`do_yield() == 0` 且置位 `need_resched`，`do_kill(-1) == -E_INVAL`；spin 程序（`kill returns 0`、`wait returns 0`）。
- `do_wait` 属于 T2，双方约定（`wait_state`、`PF_EXITING`、`PROC_ZOMBIE`、唤醒父进程的时机）写在各自任务文件的「关键决策与结论」中。
- 交付：`report/sections/T4-exit-kill.md` 与 `T4-exit-kill.prompt.md`；`YOUR CODE` 替换为 2414099。

## 当前进度 / 下一步
- 待开始。

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
