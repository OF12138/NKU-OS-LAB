# T2 功能模块二：中断与异常处理
- 负责人：lyp    DDL：11-02    状态：待开始    依赖：T0；完整联调依赖 T1
- 主要文件：code/kern/trap/trap.c, report/sections/T2-trap-handler.md, report/sections/T2-trap-handler.prompt.md

## 要求
- 指导书「功能模块二」：在 `kern/trap/trap.c` 的 `//lab3 function2 YOUR CODE` 处实现
  `void interrupt_handler(struct trapframe *tf);` 和 `void exception_handler(struct trapframe *tf);`，不改函数签名，不改其他函数。
  - 时钟中断（IRQ_S_TIMER）：设置下一次时钟事件、`ticks` 每次加一、达到 `TICK_NUM` 整数倍时调用 `print_ticks()`；累计输出达到 `SHUTDOWN_AFTER` 次后调用 `sbi_shutdown()`。
  - 其他中断：区分类型并输出提示；无法识别的走 `print_trapframe(tf)`。
  - 非法指令、断点异常：按规定格式输出，并按**实际指令长度**（2 字节压缩指令 / 4 字节）更新 `tf->epc`；其他异常用 `print_trapframe(tf)` 兜底。
- 已知要点（写提示词时注意）：
  - 框架中没有定义 `SHUTDOWN_AFTER`，需要自己定义（数值要与 `make grade` Test 3 等待 200 ticks 不冲突）。
  - `make grade` 匹配的字符串：`Breakpoint at`、`Illegal instruction at`、`Supervisor software interrupt`、`Unknown interrupt: cause 15`，未知异常要输出 `cause    0x00000014`（即 print_trapframe）。
  - 评测用的 `ebreak` 可能被汇编成 2 字节的 `c.ebreak`，非法指令是 4 字节的 `.word 0x7b`，所以 epc 偏移要从指令编码低两位判断，不能写死。
- 提示词：按四段式写一份，[RELY] 引用框架原始代码（trapframe、IRQ_*/CAUSE_* 宏、print_trapframe、clock_set_next_event、ticks、sbi_shutdown）。
- 报告 section：模块功能描述、最终提示词（`<!-- include:T2-trap-handler.prompt.md -->`）、实现迭代过程。
- GDB 验证：对断点和非法指令各截一张图，展示进入处理函数时的 `tf->epc`、指令编码和返回后的 pc（截图放 `report/images/T2-*`）。

## 当前进度 / 下一步

## 关键决策与结论

## 验证结果
（目标：`make grade` 100/100，`make qemu` 每秒打印 `100 ticks`，到次数后关机）

## 迭代素材

## 留言
