# T2 功能模块二：进程创建及初始化
- 负责人：openfar    DDL：11-18    状态：待开始    依赖：T0
- 主要文件：code/kern/process/proc.c, report/sections/T2-proc-create.md, report/sections/T2-proc-create.prompt.md

## 要求
- 指导书「功能模块二」：在 `kern/process/proc.c` 中实现（标记 `//LAB4 function2 YOUR CODE`）：
  `alloc_proc`、`proc_init`、`kernel_thread`、`do_fork`、`copy_thread`、`copy_mm`。
- 注意事项：`alloc_proc` 把 PCB 置于安全空白状态；`idleproc` 复用当前执行现场（pid 0、`bootstack`、`current`）；`kernel_thread` 与 `copy_thread` 配合 `forkret → forkrets → __trapret` 形成首次运行链路；`do_fork` 正确处理失败清理、PID 分配、链表挂接和唤醒顺序；`copy_mm` 当前阶段只保持函数形态。
- 评测：`test_lab4_process_create()`（插在 `proc_init()` 末尾），检查 idleproc/initproc 的各字段，输出 `[Test Process Create] PASS` 得 10 分。
- 提示词：四段式一份，[RELY] 引用框架原始代码。
- 报告 section：模块功能描述、最终提示词、实现迭代过程。

## 当前进度 / 下一步

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
