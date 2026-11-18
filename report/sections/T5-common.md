# 一、实验目的

结合 Lab 4（进程管理）实验指导书与核心内核模块设计，本实验的主要目的是：

1. **理解并掌握虚拟内存管理的组织方式与多级页表维护机制**：
   - 理解在 Sv39 三级页表框架下，`page_insert()` 与 `page_remove()` 如何联动维护页表项内容、物理页引用计数（`struct Page.ref`）与 TLB 状态（`sfence.vma` / `tlb_invalidate()`）三者的一致性；
   - 理解虚拟内存描述结构（`mm_struct`、`vma_struct`）与多级页表按需建立、权限更新和幂等撤销的基本组织方式。

2. **掌握内核线程的创建、初始化与生命周期管理过程**：
   - 理解进程控制块 `struct proc_struct` 的关键字段（状态 `state`、`pid`、内核栈 `kstack`、上下文 `context`、中断帧 `tf`、页目录基址 `pgdir` 及链表/哈希表挂接关系）；
   - 掌握 0 号空闲线程 `idleproc` 复用启动现场的初始化过程，以及通过 `kernel_thread()`、`do_fork()`、`copy_thread()`、`setup_kstack()` 创建 1 号内核线程 `initproc` 并构造首次运行链路的完整机理。

3. **掌握内核线程的上下文切换与基本调度过程**：
   - 理解 `cpu_idle()` 主动让出 CPU、`schedule()` 在 `proc_list` 中按 FIFO/轮转策略选取 `PROC_RUNNABLE` 线程、`proc_run()` 在关中断临界区内切换 `current` 与 `satp` 页表的完整协调流程；
   - 深入理解 `switch_to()` 保存与恢复被调用者保存寄存器（`ra`、`sp`、`s0`-`s11`）以及经由 `forkret -> forkrets -> __trapret` 跳转至 `kernel_thread_entry` 启动新线程的底层控制流转换。

4. **持续深化契约化 AI 协同内核工程实践**：
   - 继续实践“四段式提示词 + `.handoff/` 异步看板 + 端到端评测注入”的团队协作范式，在功能拆分、边界约束与跨成员模块集成中提升代码正确率与文档可追溯性。

---

# 二、实验环境

## 软件与运行环境

本实验全组继续严格统一使用 **QEMU 4.1.1**（内置 OpenSBI v0.4），保证与 Lab 2/Lab 3 完全一致的 `-device loader` 内核加载行为；同时在 `code/Makefile` 中针对 gcc 14 及以上版本增加 `-Wno-error=int-conversion -Wno-error=incompatible-pointer-types` 编译选项以兼容框架与评测测试代码。

| 成员 | 宿主系统 | QEMU 版本 | 交叉编译工具链 | 调试器版本 |
| :--- | :--- | :--- | :--- | :--- |
| 2411264-张远 | WSL2 Ubuntu 24.04 | 4.1.1（OpenSBI v0.4） | riscv64-unknown-elf-gcc 15.1.0 | riscv64-unknown-elf-gdb 16.3 |
| 2414099-李云鹏 | WSL2 Ubuntu 22.04 | 4.1.1（OpenSBI v0.4） | SiFive riscv64-unknown-elf-gcc 10.2.0 | riscv64-unknown-elf-gdb 10.1 |
| 2413074-刘昀皓 | WSL2 Ubuntu 24.04 | 4.1.1（OpenSBI v0.4） | riscv64-unknown-elf-gcc 15.1.0 | riscv64-unknown-elf-gdb 16.3 |

全组成员本地仍通过 `export QEMU=$HOME/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64` 显式指定模拟器路径，使 `make qemu` 与 `make grade` 运行在同一 QEMU 4.1.1 版本上。

## AI 工具与底层模型

> **【待确认】**：以下表格待与小组成员最终确认本次 Lab 4 实际使用的工具及底层模型版本后补齐。

| 成员 | 负责任务 | AI 编程工具 | 底层模型 | 备注 / 特殊配置 |
| :--- | :--- | :--- | :--- | :--- |
| 2411264-张远 | T0, T2, T3, T6 | 【待向组员核对】 | 【待向组员核对】 | 环境集成、进程创建与调度实现、报告集成 |
| 2414099-李云鹏 | T1, T4 | 【待向组员核对】 | 【待向组员核对】 | 虚拟内存页操作、整体逻辑与 OS 原理对照 |
| 2413074-刘昀皓 | T5 | Antigravity（终端 Agent） | Gemini 4 Argon / Gemini 4 Hydrogen | Challenge 剖析、公共章节初稿与测试验证 |

---

# 五、测试与验证

### 1. 测试设计与评测原理

Lab 4 的自动化评测由 `code/tools/grade.sh` 与 `code/tools/inject.py` 协同驱动。执行 `make grade` 时，脚本会先将当前 `code/` 复制到临时工作目录，随后由 `tools/inject.py` 把 `tools/test_lab4.c` 和 `tools/test_lab4.h` 拷贝至临时目录的 `kern/test/` 下，向 `Makefile` 的 `KINCLUDE` 与 `KSRCDIR` 追加 `kern/test`，并向核心代码注入三处测试调用钩子：

1. 在 `code/kern/mm/pmm.c` 的 `pmm_init()` 函数末尾注入 `test_lab4_pmm_basic();`；
2. 在 `code/kern/process/proc.c` 的 `proc_init()` 函数末尾注入 `test_lab4_process_create();`；
3. 在 `code/kern/process/proc.c` 的 `init_main()` 函数开头注入 `test_lab4_schedule();`。

随后 `tools/grade.sh` 编译生成带注入测试的内核镜像并在 QEMU 中运行（默认在 `readline` 处拦截或超时截断输出），使用 `grep -F` 对串口输出进行精确子串匹配。评测共分 3 个部分，每部分 10 分，总分 **30 分**：

| 测试部分 | 分值 | 注入位置与核心验证逻辑 | 必须匹配的串口输出字符串 |
| :--- | :---: | :--- | :--- |
| `check PMM` | 10 | 调用 `test_lab4_pmm_basic()`：验证 `page_insert()` 建立映射及同页重复插入仅更新权限不泄漏物理页、同地址换页使旧页 `page_ref` 归零、以及 `page_remove()` 幂等删除与 `page_ref` 归零 | `[Test PMM] PASS` |
| `check Process Create` | 10 | 调用 `test_lab4_process_create()`：校验 `current == idleproc`、`idleproc`（`pid == 0`、`PROC_RUNNABLE`、`kstack == bootstack`、`parent == NULL`、`mm == NULL`、`tf == NULL`、`runs == 0`、`flags == 0`、`context.ra == 0`）与 `initproc`（`pid == 1`、`PROC_RUNNABLE`、`parent == idleproc`、`mm == NULL`、`context.ra != 0`、`context.sp != 0`） | `[Test Process Create] PASS` |
| `check Schedule` | 10 | 在 `init_main()` 中调用 `test_lab4_schedule()` 创建两个 `test_lab4_thread` 并循环主动让出 CPU 触发 `schedule()`，确认两线程均成功运行并结合 `init_main()` 原有输出检查 | `[Test Schedule] PASS`、`this initproc, pid = 1, name = "init"`、`To U: "Hello world!!".`、`To U: "en.., Bye, Bye. :)"` |

### 2. 测试执行截图

> **【待确认】**：当前 `T1`、`T2`、`T3` 功能模块尚待实现。待上游代码全部合入并通过编译后，将在统一 **QEMU 4.1.1** 环境下运行 `make qemu` 与 `make grade`，拍摄并补全以下两张实测截图：
> - `../images/T5-qemu-run.png`（`make qemu` 运行与 `initproc` 输出截图）
> - `../images/T5-make-grade.png`（`make grade` 30/30 满分评测截图）

---

# 六、实验总结与收获

## AI 协作开发的经验

> **【待补充】**：以下为基于 Lab 4 阶段工作流沉淀的经验初稿，待 `T1-T3` 实现与最终集成验收结束后补充完整的一线迭代体会。

1. **前后端解耦与“先原理推演、后实证校准”的并行推进策略**：
   在 Lab 4 多任务并行阶段，Challenge 分析与公共章节（实验目的、评测机制剖析）并非完全被动等待功能代码落地后才动笔。通过提前解析 `code/kern/sync/sync.h`、`code/kern/mm/pmm.c` 与 `code/tools/inject.py`/`test_lab4.c`，可以在上游功能尚在实现时先把不变的底层机制与测试架构写实，并对依赖下游代码的部分显式标记占位，待 `T1-T3` 合入后快速完成行号核对与真机截图，大幅降低了任务串行等待带来的集成尾期压力。

2. **机器校验与规范脚本兜底保障交付一致性**：
   继续使用 `.claude/tools/check-rely.py` 对提示词 `[RELY]` 代码块进行逐行核对，确保提示词中引用的框架数据结构与接口签名和仓库代码保持零误差；结合 `merge-report.py` 约定的章节标题格式编写各节草稿，避免了集成阶段的大规模格式返工。
