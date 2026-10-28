# T0 环境与框架
- 负责人：openfar    DDL：10-29    状态：完成    依赖：无
- 主要文件：code/kern/mm/pmm.c, code/kern/mm/best_fit_pmm.c, code/Makefile, code/libs/riscv.h

## 要求
- 指导书：“本实验依赖实验2……将生成结果填写到本实验代码中标有‘lab2’注释的对应位置，确保代码能够正确编译。”
- 框架中的 lab2 标记：`kern/mm/pmm.c:84`（page_init）、`kern/mm/best_fit_pmm.c:7`（Best-Fit）。
- `code/Makefile` 写死了 `QEMU := qemu-system-riscv64`，`tools/grade.sh` 也直接调用 PATH 里的 qemu；按 lab2 T0 的做法让 `make qemu` / `make grade` 都用 QEMU 4.1.1。

## 当前进度 / 下一步
- 已完成（10-28）。下一步交给 T1 / T2。

## 关键决策与结论
- lab2 代码用三方合并带入（lab2 框架原版 → lab2 最终版的改动，叠加到 lab3 框架上）：page_init、Best-Fit 实现和各文件的学习注释都保留；**代码以 lab3 框架为准**，lab2 为 buddy/SLUB 加的 include、`PMM` 开关和 init.c 中的测试调用没有带入（lab3 框架里没有这些文件）。
- 两处 lab2 标记行（`//lab2 请补充你在lab2中的代码`）保留，下面接 lab2 的学号行和实现。
- 默认管理器保持 lab3 框架的 `default_pmm_manager`；把它换成 `best_fit_pmm_manager` 也能通过自检（见验证结果）。
- lab3 的 memlayout.h 删除了 `PHYSICAL_MEMORY_END`，page_init 中改用 `KERNTOP - va_pa_offset`（即原注释中写的等式，值同为 0x88000000），没有往框架头文件里补宏。
- Makefile：并入 lab2 的注释；grade 目标在 `QEMU` 是路径时，把它所在目录放到 grade.sh 的 PATH 最前面（grade.sh 本身未改）。
- `libs/riscv.h`：仅在 `__ASSEMBLER__` 下 `#define sbadaddr stval`，让 SAVE_ALL 能被 binutils 2.45 汇编；SAVE_ALL 未改，旧工具链同样可用。

## 验证结果
- 框架原样（HEAD 8cc0679）和 T0 版本都在 `trap.c` 处编译失败（`implicit declaration of function 'interrupt_handler'`），这是 T2 的工作，不是 T0 引入的。
- 在 WSL 副本里临时补两个空处理函数（未提交）后：`make` 通过；trapentry.o 反汇编为 `csrr s3,stval`；`make qemu`（OpenSBI v0.4）输出 `memory management: default_pmm_manager` … `check_alloc_page() succeeded!`；换成 best_fit 后同样 `check_alloc_page() succeeded!`。
- `make grade`：确认 grade.sh 调到的是 `QEMU emulator version 4.1.1`；得分 0/100（Trap 尚未实现，符合预期）。

## 迭代素材
- 汇编报错 `kern/trap/trapentry.S:47: Error: unknown CSR 'sbadaddr'` → binutils 2.45 删掉了 1.10 版规范以前的旧 CSR 名 → 在 riscv.h 中只对汇编把 `sbadaddr` 定义为 `stval`，SAVE_ALL 不动。
- 合并后 page_init 引用的 `PHYSICAL_MEMORY_END` 在 lab3 的 memlayout.h 中已不存在 → 改用 `KERNTOP - va_pa_offset`。

## 留言
