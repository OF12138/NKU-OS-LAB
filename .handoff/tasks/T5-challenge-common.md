# T5 Challenge；公共章节初稿
- 负责人：nagilix    DDL：Challenge 11-19，公共章节 11-21    状态：进行中    依赖：测试截图依赖 T1～T3
- 主要文件：report/sections/T5-challenge.md, report/sections/T5-common.md, report/images/T5-*

## 要求
- 指导书「扩展练习 Challenge」（写成报告中的 Challenge 一节）：
  1. 说明语句 `local_intr_save(intr_flag); ... local_intr_restore(intr_flag);` 是如何实现开关中断的（结合 `kern/sync/sync.h`、`libs/riscv.h` 中的 sstatus.SIE 读写，以及本 lab 中 `schedule` / `proc_run` 为什么需要它）。
  2. 深入理解不同分页模式的工作原理（思考题）：
     - `get_pte()`（`kern/mm/pmm.c`）中有两段形式类似的代码，结合 sv32、sv39、sv48 的异同，解释这两段代码为什么如此相像。
     - 目前 `get_pte()` 将页表项查找和页表页分配合并在一个函数里，这种写法好吗？有没有必要把两个功能拆开？
- 公共章节初稿：实验目的（参考指导书「实验目的」）、实验环境（各成员的 AI 工具与模型，向组员确认）、测试与验证（`make qemu` 与 `make grade` 截图，等 T1～T3 完成后拍）、实验总结中「AI 协作开发的经验」。
- Challenge 附一份四段式提示词（`T5-challenge.prompt.md`）；截图按 `report/images/T5-*.png` 命名。

## 当前进度 / 下一步
- 11-16：已拉取 `origin/lab4` 并切换至 `lab4` 分支；从课程官网核对并补齐 Lab 4「实验目的」「扩展练习 Challenge」「实验报告要求」原文。
- 11-16：完成 `report/sections/T5-challenge.md` 初稿（Challenge 1 底层 CSR 机制及调度临界区预分析，Challenge 2 Sv32/Sv39/Sv48 对比与 `get_pte()` 拆分评价）、四段式提示词 `report/sections/T5-challenge.prompt.md`，以及 `report/sections/T5-common.md` 不依赖实测的前置骨架（实验目的、实验环境框架、评测原理、AI 协作经验初稿）。
- 下一步：
  1. 等待 T1-T3 合入后，对照 `schedule()` 与 `proc_run()` 最终源码核对并补充 Challenge 1 的实证行号与代码细节，定稿提交 Challenge（DDL 11-19）；
  2. 向组员确认 Lab 4 各自 AI 工具与底层模型配置，补齐 `T5-common.md` 实验环境表；
  3. T1-T3 全部通过后，在统一 QEMU 4.1.1 下运行 `make qemu` 与 `make grade` 验证 30/30 并截取 `report/images/T5-qemu-run.png`、`report/images/T5-make-grade.png`，完成公共章节终稿（DDL 11-21）。

## 关键决策与结论
- **Challenge 1 机理要点**：`local_intr_save(x)` 借 `do { ... } while (0)` 封装调用 `__intr_save()`，读取 `sstatus` 的 `SSTATUS_SIE` 位；原为 1 时调用 `intr_disable()`（`clear_csr(sstatus, SSTATUS_SIE)`）关中断并返回 1，原为 0 时直接返回 0；`local_intr_restore(x)` 仅在 `flag == 1` 时调用 `intr_enable()`，保证嵌套调用与 Trap 上下文安全。`schedule` / `proc_run` 需关中断以保护 `proc_list` 遍历及 `current`、`satp`、`switch_to` 状态切换的三元原子性。
- **Challenge 2 机理要点**：Sv32（2 级）/ Sv39（3 级）/ Sv48（4 级）均以 4 KiB 为页粒度且共享非叶子目录项递归查找与清零分配逻辑，$L$ 级页表对应 $L-1$ 次中间步进，故 Sv39 的 `get_pte()` 呈现 `PDX1` 与 `PDX0` 两段同构代码。合并设计在教学框架下简洁且免除外部调用改动，但在工业级设计中因违反单一职责、混用 `NULL` 语义、缺少跨级 OOM 回滚机制而宜拆分为只读查找与带错误码的分配接口。
- **评测架构要点**：依据 `code/tools/inject.py`、`code/tools/test_lab4.c` 与 `code/tools/grade.sh`，Lab 4 自动化评测在 `pmm_init()` 末、`proc_init()` 末、`init_main()` 首注入 3 个测例（每项 10 分，共 30 分）。

## 验证结果
- `python3 .claude/tools/check-rely.py`：全部通过（`T5-challenge.prompt.md` `[RELY]` 与当前 `code/` 一致）。
- `report/sections/T5-challenge.md`、`T5-challenge.prompt.md`、`T5-common.md` 均通过 Zero Emoji 扫描（0 个 Emoji）。

## 迭代素材
- `T5-challenge.md` 行号核对迭代：初稿凭印象写了 `init.c:51-57`、`clock.c:51`、`trapentry.S:105-120`，经 `nl -ba` 逐行核对发现受 Lab 0-3 注入的中文学习注释影响，实际行号分别为 `init.c:50-55`、`clock.c:55`、`trapentry.S:157-167`，已全部修正。
- 提示词 `[RELY]` 格式核对：确保所有 `[RELY]` 代码块逐行原样复制自当前 `code/`（保持原有 `{` 换行与单行风格），一次通过 `check-rely.py` 校验。

## 留言
