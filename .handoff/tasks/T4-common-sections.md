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
- 下一步：
  1. 撰写两道思考题初稿，输出至 `report/sections/T4-questions.md`，并在节点反推对应 prompt（DDL: 11-03）。
  2. 起草公共章节骨架 `report/sections/T4-common.md`（实验目的、实验环境、AI 协作经验初稿，DDL: 11-06）。
  3. 待 T1、T2（11-02）全部合入通过测试后，执行 `make qemu` 与 `make grade` 截取验证图片存至 `report/images/T4-*`。

## 关键决策与结论
- **QEMU 4.1.1 路径**：nagilix 本地路径为 `/home/nagilix/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64`，评测时须显式设置 `QEMU` 环境变量。
- **思考题分析要点**：
  - 思考题 1：RISC-V 规范在 1.10 版之前要求基地址必须 4 字节对齐（最低 2 位必须为 00）。1.10 引入 MODE 字段时将 `00` 定义为 Direct 模式，使旧代码直接写入 4 字节对齐基地址时自然落入 Direct 模式且基地址不变，实现硬件级向前向后兼容。
  - 思考题 2：宏定义 `do { } while (0)` 解决语法歧义与宏展开尾随分号在 `if-else` 分支中的空语句语法报错（`'else' without a previous 'if'`），同时构成独立作用域；`pmm.c` 维护全局空闲链表与页描述符等临界资源，关中断保证单核环境下内存分配/释放操作的原子性，避免异步时钟/外部中断重入或中断上下文中可能的内存操作造成链表破坏与竞争状态。
- **各成员工具链与环境**：沿用 lab2 确定的工具链分工，nagilix 使用 Antigravity CLI / IDE + Gemini 3.8 Flash (High) / Claude，openfar 使用 Claude Code + Claude Opus 5.5，lyp 使用 Codex + gpt-6.1-sol。

## 验证结果
- 暂未开始实际代码测试截图（等待 T1、T2 依赖就绪）。

## 迭代素材
- （待记录）

## 留言
