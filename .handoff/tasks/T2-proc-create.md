# T2 功能模块二：进程创建及初始化
- 负责人：openfar    DDL：11-18    状态：完成    依赖：T0
- 主要文件：code/kern/process/proc.c, report/sections/T2-proc-create.md, report/sections/T2-proc-create.prompt.md

## 要求
- 指导书「功能模块二」：在 `kern/process/proc.c` 中实现（标记 `//LAB4 function2 YOUR CODE`）：
  `alloc_proc`、`proc_init`、`kernel_thread`、`do_fork`、`copy_thread`、`copy_mm`。
- 注意事项：`alloc_proc` 把 PCB 置于安全空白状态；`idleproc` 复用当前执行现场（pid 0、`bootstack`、`current`）；`kernel_thread` 与 `copy_thread` 配合 `forkret → forkrets → __trapret` 形成首次运行链路；`do_fork` 正确处理失败清理、PID 分配、链表挂接和唤醒顺序；`copy_mm` 当前阶段只保持函数形态。
- 评测：`test_lab4_process_create()`（插在 `proc_init()` 末尾），检查 idleproc/initproc 的各字段，输出 `[Test Process Create] PASS` 得 10 分。
- 提示词：四段式一份，[RELY] 引用框架原始代码。
- 报告 section：模块功能描述、最终提示词、实现迭代过程。

## 当前进度 / 下一步
- 11-19 完成：六个函数已实现（`//LAB4 function2 2411264`），报告 section、提示词和图 `report/images/T2-kstack-firstrun.png` 已提交。下一步 T3。

## 关键决策与结论
- `alloc_proc` 逐字段初始化：`state=PROC_UNINIT`、`pid=-1`、`pgdir=boot_pgdir_pa`，其余清零，两个链表节点自指。
- `idleproc` 不经过 `do_fork`、不挂入 `proc_list`：pid 0、`kstack=bootstack`、`need_resched=1`、`current=idleproc`，`context` 全 0（评测检查 `context.ra == 0`）。T3 的 `schedule` 遍历 `proc_list` 时不会遇到它，找不到可运行线程时应回到 `idleproc`。
- `copy_thread`：trapframe 放在内核栈顶，`context.ra=forkret`、`context.sp=proc->tf`；T3 的 `switch_to` 恢复 context 后 `ret` 即进入 forkret 链路。
- 框架没有 `CLONE_VM`，`kernel_thread` 直接传 `clone_flags`。
- 学习注释：用户在 function2/function3 标记下写的“尚未实现”说明，T2 部分改写为各函数的注释，function3 处只留 `proc_run`、`cpu_idle` 等 T3 内容。

## 验证结果
- WSL（gcc 15.1，QEMU 4.1.1）：`make` 只剩 T3 的 `cpu_idle` 链接错误。独立副本补 `cpu_idle`、`proc_run`、`schedule` 空桩（未提交）后，`make qemu` 原有自检全部通过；`make grade` PMM、Process Create 通过，20/30。
- `check-rely.py` 全部通过。

## 迭代素材
- 原版 ucore 的 `kernel_thread` 用 `clone_flags | CLONE_VM`，本框架未定义 `CLONE_VM` → 生成前核对框架发现 → 直接传 `clone_flags`，[RELY] 中写明。
- `undefined reference to 'cpu_idle'` → T3 未实现 → 独立副本加空桩验证 T2。

## 留言
