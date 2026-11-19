# T3 功能模块三：进程调度
- 负责人：openfar    DDL：11-19    状态：完成    依赖：T2
- 主要文件：code/kern/process/proc.c, code/kern/schedule/sched.c, code/kern/process/switch.S, report/sections/T3-schedule.md, report/sections/T3-schedule.prompt.md

## 要求
- 指导书「功能模块三」（标记 `LAB4 function3 YOUR CODE`）：`cpu_idle`、`proc_run`（proc.c）、`schedule`（sched.c）、`switch_to`（switch.S）。
- 注意事项：`schedule` / `proc_run` 的关键区域要关中断（`local_intr_save/restore`）；`schedule` 找不到可运行线程时退回 `idleproc`；`proc_run` 更新 `current`、切换页表（Sv39 satp，不能用 Sv32 遗留的 `lsatp()`，参照 `enable_paging()`）并调用 `switch_to`；`switch_to` 只保存/恢复 `struct context`。
- 评测：`test_lab4_schedule()`（插在 `init_main()` 开头），两个测试线程都运行过并输出 `[Test Schedule] PASS`，另需 `this initproc, pid = 1, name = "init"` 和两行 `To U: ...`，得 10 分。
- 提示词：四段式一份；报告 section：模块功能描述、最终提示词、实现迭代过程。

## 当前进度 / 下一步
- 11-19 完成：四个函数已实现（`//LAB4 function3 2411264`），报告 section 与提示词已提交。下一步 T6 集成。

## 关键决策与结论
- `proc_run`：satp = `((uintptr_t)SATP_MODE_SV39 << 60) | (pgdir >> RISCV_PGSHIFT)`（`SATP_MODE_SV39` 值为 8，未移位），写完 `flush_tlb()`；关中断包住 current 更新、换页表和 `switch_to`。
- `schedule`：current 为 idleproc 时从 `proc_list` 头找，否则从 current 下一个开始绕一圈；跳过链表头；循环后再判一次状态，找不到用 idleproc。
- `switch_to`：`#include <riscv.h>`，用 `STORE`/`LOAD`/`REGBYTES` 保存/恢复 ra、sp、s0～s11。
- `init_main` 返回后进入 `do_exit` 的 `panic("process exit!!.
")` 是本实验的预期行为。

## 验证结果
- WSL（gcc 15.1，QEMU 4.1.1），完整工程无桩：`make` 通过；`make qemu` 输出 initproc 三行后出现预期的 process exit panic；`make grade` 30/30（PMM、Process Create、Schedule 均 OK）。
- `check-rely.py` 全部通过。

## 迭代素材
- 生成前核对 riscv.h：`lsatp()` 是 Sv32（`SATP32_MODE`、`unsigned int` 参数）；`SATP_MODE_SV39` 为 8 未移位 → [RELY] 给出 `enable_paging()` 写法，[SPECIFICATION] 写明左移 60 位。
- 框架 `switch.S` 只有标记、没有 include → 提示词要求 `#include <riscv.h>` 以使用 STORE/LOAD/REGBYTES。

## 留言
