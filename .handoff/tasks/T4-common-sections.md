# T4 指导书思考题；公共章节初稿
- 负责人：nagilix    DDL：思考题 11-03，公共章节 11-06    状态：进行中    依赖：测试截图依赖 T1、T2
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
- 下一步：
  1. 等待 11-02 功能模块二（T2，lyp）代码完成并合入 lab3 主干。
  2. 依赖就绪后，在统一 QEMU 4.1.1 环境下执行 `make qemu` 与 `make grade`，截取测试截图保存至 `report/images/T4-*.png`，补齐公共章节测试图表。
  3. 协同 openfar 进行最终 `report.md` 与 `prompt.md` 的汇总校验。

## 关键决策与结论
- **QEMU 4.1.1 路径**：nagilix 本地路径为 `/home/nagilix/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64`，评测时须显式设置 `QEMU` 环境变量。
- **思考题分析要点**：
  - 思考题 1：RISC-V 规范在 1.10 版之前要求基地址必须 4 字节对齐（最低 2 位必须为 00）。1.10 引入 MODE 字段时将 `00` 定义为 Direct 模式，使旧代码直接写入 4 字节对齐基地址时自然落入 Direct 模式且基地址不变，实现硬件级向前向后兼容；同时保留单入口软件分发的控制流语义。
  - 思考题 2：宏定义 `do { } while (0)` 解决语法歧义与宏展开尾随分号在 `if-else` 分支中的空语句语法报错（`'else' without a previous 'if'`），同时构成独立局部作用域且具备零运行时开销；`pmm.c` 维护全局空闲链表与页描述符等临界资源，关中断保证单核环境下内存分配/释放操作的原子性，避免异步时钟/外部中断重入或中断上下文中可能的内存操作造成链表破坏与竞争状态；`__intr_save()` 保存原始 SIE 标志位支持嵌套调用。
- **各成员工具链与环境**：沿用 lab2 确定的工具链分工，nagilix 使用 Antigravity（终端 Agent） + Gemini 4 Argon / Claude Opus 5.5，openfar 使用 Claude Code + Claude Opus 5.5，lyp 使用 Codex + gpt-6.1-sol。

## 验证结果
- 思考题解答与公共章节已完成自我逻辑自洽性与规范合规性校验。
- 测试运行截图（`make qemu` 与 `make grade`）暂未生成，严格依赖 T1、T2 完成。

## 迭代素材
- 思考题 1 撰写迭代：初期推导时容易仅关注到 Direct 模式跳转公式（Target PC = BASE << 2），后补充分析了旧规范硬件实现对最低 2 位的硬连处理（hardwired to 0），以及单入口统一软件分发（__alltraps 读 scause）在控制流架构层面的无缝承接，形成从编码、地址、控制流到硬件实现四维一体的完整证明。
- 思考题 2 撰写迭代：结合 `sync.h` 源码发现 `local_intr_save` 内部调用了 `__intr_save()`，该函数返回了原始中断使能状态，因此在解答中不仅阐述了原子性互斥与防重入，还重点指出了这种设计对“嵌套临界区调用”的核心价值（避免外层已关中断时被内层提前开中断）。

## 留言
