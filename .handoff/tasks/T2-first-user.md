# T2 功能模块二：创建加载第一个用户进程
- 负责人：openfar    DDL：11-29    状态：待开始    依赖：T0
- 主要文件：code/kern/process/proc.c, report/sections/T2-*

## 要求
- 指导书「功能模块二」。需要更新和完善：`init_main`、`alloc_proc`、`kernel_thread`、`do_wait`、`user_main`、`kernel_execve`、`do_execve`、`load_icode`（均在 `kern/process/proc.c`）。
- 注意事项（指导书）：`kernel_execve` 要真正进入 trap 返回路径；`load_icode` 设置好用户入口、用户栈指针和 `sstatus` 特权级；`do_execve` 先回收旧 mm 再装载；`do_wait` 的父子关系、僵尸回收与睡眠唤醒要和 `do_exit` 一致。
- 评测：`test_lab5_process_create`、`do_wait(-1, NULL) == -E_BAD_PROC`，以及用户程序输出 `all user-mode processes have quit.`、`init check memory pass.`。
- 交付：`report/sections/T2-first-user.md` 与 `T2-first-user.prompt.md`。

## 当前进度 / 下一步
- 待开始。

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
