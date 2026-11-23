# T1 功能模块一：用户态系统调用
- 负责人：lyp    DDL：11-28    状态：待开始    依赖：T0
- 主要文件：code/kern/trap/trap.c, code/kern/syscall/syscall.c, report/sections/T1-*

## 要求
- 指导书「功能模块一：用户态系统调用功能」。需要更新和完善：`exception_handler`（`kern/trap/trap.c`，只需关注 `CAUSE_USER_ECALL`）、`syscall`，以及 `sys_exit`、`sys_fork`、`sys_wait`、`sys_exec`、`sys_yield`、`sys_kill`、`sys_getpid`、`sys_putc`、`sys_pgdir`（`kern/syscall/syscall.c`）。
- 注意事项（指导书）：a0 为系统调用号，a1～a5 为参数；识别到用户态 ecall 后 `epc` 要前移；分发表要检查边界，非法编号不能当作有效请求；用户指针要结合 `user_mem_check` 检查。
- 评测 `test_lab5_syscall`：`SYS_getpid` 后 `epc` 从 0x1000 变成 0x1004、a0 为当前 pid；编号 0 返回 `-E_INVAL`。
- 交付：`report/sections/T1-syscall.md`（按模板「功能模块」写：功能描述、最终提示词、实现迭代过程）和四段式提示词 `T1-syscall.prompt.md`。
- 标记 `lab5 function1 YOUR CODE` 中的 `YOUR CODE` 替换为 2414099。

## 当前进度 / 下一步
- 待开始。

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
