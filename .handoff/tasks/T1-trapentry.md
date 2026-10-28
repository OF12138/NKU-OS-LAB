# T1 功能模块一：Trap 上下文恢复与 Trap 入口
- 负责人：openfar    DDL：11-01    状态：待开始    依赖：T0
- 主要文件：code/kern/trap/trapentry.S, report/sections/T1-trapentry.md, report/sections/T1-trapentry.prompt.md

## 要求
- 指导书「功能模块一」：在 `kern/trap/trapentry.S` 中实现 `RESTORE_ALL` 和 `__alltraps`（`#lab3 function1 YOUR CODE` 两处）。
  - RESTORE_ALL 必须与已给的 SAVE_ALL 对应；SAVE_ALL 对 x2(sp) 的处理与其他寄存器不同，不能照搬同样的恢复方式。
  - `__alltraps` 按 RISC-V 调用约定把 Trap frame 地址作为 `trap(struct trapframe *tf)` 的参数，`trap()` 返回后进入 `__trapret`，RESTORE_ALL 后 `sret`。
  - 不得修改 SAVE_ALL、`struct trapframe` 和 `trap()` 的签名。
- 提示词：按四段式写一份，[RELY] 引用框架原始代码（SAVE_ALL、trapframe/pushregs、trap()、sstatus/sepc 相关宏）。
- 报告 section：模块功能描述、最终提示词（`<!-- include:T1-trapentry.prompt.md -->`）、实现迭代过程；说明 sp 的特殊处理、哪些 CSR 需要恢复（sstatus/sepc）而 sbadaddr/scause 不必恢复的原因、`mv a0, sp` 的作用。
- GDB 验证：在 `__alltraps`、`trap`、`__trapret`、`sret` 处断点，截图展示 Trap frame 在栈上的布局和 sret 前后 pc/sp 的变化（截图放 `report/images/T1-*`）。

## 当前进度 / 下一步

## 关键决策与结论

## 验证结果
（`make grade` 中 Test 1“trap entry, register restore, and breakpoint EPC”需要 T2 的断点处理配合）

## 迭代素材

## 留言
