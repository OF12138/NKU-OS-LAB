# T4 指导书思考题；公共章节初稿
- 负责人：nagilix    DDL：思考题 11-03，公共章节 11-06    状态：完成    依赖：测试截图依赖 T1、T2
- 主要文件：report/sections/T4-questions.md, report/sections/T4-common.md, report/images/T4-*

## 要求
- 指导书正文中的思考题（写成「练习：指导书思考题」一节）：
  1. 「中断与中断处理流程」一节：stvec 最低两位编码模式是 1.10 版加入的，这个改动如何保证与旧版本兼容？
  2. 「断亦有断」一节：`sync.h` 中 `local_intr_save` 宏用 `do { } while (0)` 包起来有什么作用？结合 `pmm.c` 的 `alloc_pages` / `free_pages` 说明为什么要先关中断再操作。
- 公共章节初稿：实验目的（参考指导书「实验目的」）、实验环境（各成员的 AI 工具与模型，向组员确认）、测试与验证（`make qemu` 与 `make grade` 截图，等 T1、T2 完成后拍）、实验总结中「AI 协作开发的经验」。
- 截图按 `report/images/T4-*.png` 命名。

## 当前进度 / 下一步
- 10-28：已同步远端 lab3 分支最新进度，确认统一 QEMU 4.1.1 路径环境配置（`/home/nagilix/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64`）。
- 10-28：已完成两道指导书思考题的系统原理剖析与文档撰写，产出 `report/sections/T4-questions.md`。
- 10-28：已按四段式规范完成反推提示词整理，产出 `report/sections/T4-questions.prompt.md`。
- 10-28：已起草完成公共章节初稿 `report/sections/T4-common.md`（含实验目的、实验环境全组成员软硬件与 AI 工具表、测试评测架构说明、AI 协作开发经验深度总结）。
- 11-05：T1、T2 依赖解除，已完成公共章节 `report/sections/T4-common.md` 校准（修正工具链小版本号为 lyp GDB 10.1、nagilix GCC 15.1.0/GDB 16.3；修正 Test 2 评测描述为 `.word 0x0000007b` 4 字节未定义指令）。
- 11-05：在统一 QEMU 4.1.1 环境下实测 `make grade` 满分（100/100）及 `make qemu` 10 次时钟中断后正常关机；将运行截图挂载为 `report/images/T4-qemu-run.png` 与 `report/images/T4-make-grade.png` 并嵌入报告公共章节。
- 11-05：思考题、公共章节及反推提示词均已交付，T4 任务宣告完成。下一步配合 openfar 进行终期集成验收。

## 关键决策与结论
- **QEMU 4.1.1 路径**：nagilix 本地路径为 `/home/nagilix/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64`，评测时须显式设置 `QEMU` 环境变量。
- **思考题分析要点**：
  - 思考题 1：RISC-V 规范在 1.10 版之前要求基地址必须 4 字节对齐（最低 2 位必须为 00）。1.10 引入 MODE 字段时将 `00` 定义为 Direct 模式，使旧代码直接写入 4 字节对齐基地址时自然落入 Direct 模式且基地址不变，实现硬件级向前向后兼容；同时保留单入口软件分发的控制流语义。
  - 思考题 2：宏定义 `do { } while (0)` 解决语法歧义与宏展开尾随分号在 `if-else` 分支中的空语句语法报错（`'else' without a previous 'if'`），同时构成独立局部作用域且具备零运行时开销；`pmm.c` 维护全局空闲链表与页描述符等临界资源，关中断保证单核环境下内存分配/释放操作的原子性，避免异步时钟/外部中断重入或中断上下文中可能的内存操作造成链表破坏与竞争状态；`__intr_save()` 保存原始 SIE 标志位支持嵌套调用。
- **各成员工具链与环境**：沿用 lab2 确定的工具链分工，nagilix 使用 Antigravity（终端 Agent） + Gemini 4 Argon / Claude Opus 5.5，openfar 使用 Claude Code + Claude Opus 5.5，lyp 使用 Codex + gpt-6.1-sol。

## 验证结果
- 思考题解答与公共章节已完成自我逻辑自洽性与规范合规性校验。
- `make grade` 在 QEMU 4.1.1 下得分 100/100（Test 1 35/35，Test 2 25/25，Test 3 40/40）。
- `make qemu` 运行正常，连续 10 次输出 `100 ticks` 后平稳关机。
- 截图已完整入库：`report/images/T4-qemu-run.png`、`report/images/T4-make-grade.png`。

## 迭代素材
- 思考题 1 撰写迭代：初期推导时容易仅关注到 Direct 模式跳转公式（Target PC = BASE << 2），后补充分析了旧规范硬件实现对最低 2 位的硬连处理（hardwired to 0），以及单入口统一软件分发（__alltraps 读 scause）在控制流架构层面的无缝承接，形成从编码、地址、控制流到硬件实现四维一体的完整证明。
- 思考题 2 撰写迭代：结合 `sync.h` 源码发现 `local_intr_save` 内部调用了 `__intr_save()`，该函数返回了原始中断使能状态，因此在解答中不仅阐述了原子性互斥与防重入，还重点指出了这种设计对“嵌套临界区调用”的核心价值（避免外层已关中断时被内层提前开中断）。
- 公共章节与评测描述核准迭代：依据 lyp 留言与 `code/tools/test_lab3.c` 源码排查，发现 Test 2 注入的并非此前推测的 S 态 `mret`，而是直接编码为 4 字节的自定义未定义操作码 `.word 0x0000007b`；同时校准了各成员 GDB/GCC 的实际版本号，确保实验环境与评测原理陈述的绝对真实与严密。

## 留言
- [lyp 2026-11-03] T2 已完成统一环境验收：普通运行十次 `100 ticks` 后关机，原始评分 100/100，截图可参考 `report/images/T2-qemu-run.png`、`T2-make-grade.png`。本次 lyp 实测环境为 `Ubuntu-22.04-OS`、SiFive GCC 10.2.0、SiFive GDB 10.1、QEMU 4.1.1 / OpenSBI v0.4，工具链在 `/opt/riscv/bin`。公共章节的 GDB 10.2 记录请据此复核；Test 2 实际非法指令是 `tools/test_lab3.c` 的 `.word 0x0000007b`，不是 `mret`。
