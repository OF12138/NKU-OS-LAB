# T5 Challenge；公共章节初稿
- 负责人：nagilix    DDL：Challenge 11-19，公共章节 11-21    状态：完成    依赖：测试截图依赖 T1～T3
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
- 11-18：同步 `origin/lab4` 至 `826a448`（T1 已合入）；更新 `T5-common.md` 本人底层模型为 `Gemini 4 Argon / Gemini 4 Hydrogen`，并校准 `T5-challenge.md` 中因 T1 补充注释导致的 `get_pte()` 源码行号偏移。
- 11-19：同步 `origin/lab4` 至 `50b6977`（T1-T4 已合入），全部完成 T5 交付：
  1. 对照已合入的 `schedule()`（`sched.c:26-59`）、`proc_run()`（`proc.c:404-419`）、`do_fork()`（`proc.c:305-313`）、`switch_to`（`switch.S:15-48`）及 `get_pte()`（`pmm.c:353-396`）更新 `T5-challenge.md` 实证代码与全部源码行号，同步补充 `T5-challenge.prompt.md` `[RELY]` 并移除占位；
  2. 在统一 QEMU 4.1.1（OpenSBI v0.4）+ gcc 15.1.0 环境下实测 `make qemu`（输出 `initproc` 三行及预期 `process exit!!.` panic）与 `make grade`（30/30），渲染生成 `report/images/T5-qemu-run.png` 与 `report/images/T5-make-grade.png`；
  3. 完善 `T5-common.md` 全部公共章节（补齐 AI 工具与模型表、嵌入两张实测截图与图题、结合 Lab 4 一线实践完成「AI 协作开发的经验」终稿）。下一步交由 openfar 执行 T6 报告与提示词集成。

## 关键决策与结论
- **Challenge 1 机理要点**：`local_intr_save(x)` 借 `do { ... } while (0)` 封装调用 `__intr_save()`，读取 `sstatus` 的 `SSTATUS_SIE` 位；原为 1 时调用 `intr_disable()`（`clear_csr(sstatus, SSTATUS_SIE)`）关中断并返回 1，原为 0 时直接返回 0；`local_intr_restore(x)` 仅在 `flag == 1` 时调用 `intr_enable()`，保证嵌套调用与 Trap 上下文安全。结合最终源码：`schedule()` 与 `proc_run()` 形成两层嵌套临界区，保护 `proc_list` 遍历及 `current`、`satp`、`switch_to` 三元状态切换原子性；新线程首次运行经 `forkret -> __trapret` 由 `sret`（`SPIE=1`）原子开中断。
- **Challenge 2 机理要点**：Sv32（2 级）/ Sv39（3 级）/ Sv48（4 级）均以 4 KiB 为页粒度且共享非叶子目录项递归查找与清零分配逻辑，$L$ 级页表对应 $L-1$ 次中间步进，故 Sv39 的 `get_pte()` 呈现 `PDX1` 与 `PDX0` 两段同构代码。合并设计在教学框架下简洁且免除外部调用改动，但在工业级设计中因违反单一职责、混用 `NULL` 语义、缺少跨级 OOM 回滚机制而宜拆分为只读查找与带错误码的分配接口。
- **评测架构与实测结论**：依据 `code/tools/inject.py`、`code/tools/test_lab4.c` 与 `code/tools/grade.sh`，Lab 4 自动化评测在 `pmm_init()` 末、`proc_init()` 末、`init_main()` 首注入 3 个测例（每项 10 分，共 30 分）；统一 QEMU 4.1.1 下实测 30/30 满分通过。`T5-common.md` 中张远、李云鹏 AI 工具/模型沿用 Lab 3 配置，集成时如本次有变可直接微调。

## 验证结果
- `make -C code grade QEMU=.../qemu-4.1.1/.../qemu-system-riscv64`：`check PMM` OK、`check Process Create` OK、`check Schedule` OK，`Total Score: 30/30`。
- `make -C code qemu QEMU=.../qemu-4.1.1/.../qemu-system-riscv64`：全部内存/页表/SLOB/VMM 自检 succeeded，输出 `this initproc, pid = 1, name = "init"`、`To U: "Hello world!!".`、`To U: "en.., Bye, Bye. :)"`，末尾预期进入 `do_exit` panic 与 kernel debug monitor。
- 截图文件：`report/images/T5-qemu-run.png`（979x1736）、`report/images/T5-make-grade.png`（1304x476），已目视核对无乱码与空行异常。
- `python3 .claude/tools/check-rely.py`：全部通过（`T5-challenge.prompt.md` `[RELY]` 与当前 `code/` 一致）。
- Zero Emoji / 占位符 / `git diff --check` 全部通过。

## 迭代素材
- `T5-challenge.md` 行号核对迭代：初稿凭印象写了 `init.c:51-57`、`clock.c:51`、`trapentry.S:105-120`，经 `nl -ba` 逐行核对发现受 Lab 0-3 注入的中文学习注释影响，实际行号分别为 `init.c:50-55`、`clock.c:55`、`trapentry.S:157-167`，已全部修正。
- 提示词 `[RELY]` 格式核对：确保所有 `[RELY]` 代码块逐行原样复制自当前 `code/`（保持原有 `{` 换行与单行风格），一次通过 `check-rely.py` 校验。
- 11-18 T1 合入校准：T1 提交（`826a448`）在 `code/kern/mm/pmm.c` 的 `get_pte()` 内补充中文注释，导致 Challenge 2 引用的行号由 `305-346` 整体后移，后经 11-19 合并 T2/T3 学习注释后进一步后移至 `353-396`（`boot_map_segment` 位于 250 行、`mmu.h` `PTE_PPN_SHIFT` 位于 73 行、`switch.S` `switch_to` 位于 15-48 行），全部对照 `50b6977` 逐行核对修正。
- 11-19 `T5-qemu-run.png` 截图行距问题：初次通过 PTY 捕获 QEMU 输出并以文本模式读取时，OpenSBI `\r\n` 经 PTY `ONLCR` 转为 `\r\r\n`，被 Python 通用换行解析成双倍空行（高度 3356px）；改为二进制读取并归一化 `\r\r\n -> \n` 后重绘为 979x1736 紧凑清晰截图。

## 留言
- [openfar 11-20] 审核通过，Challenge 两题分析准确，截图真实。直接改了两处：① 11-19 openfar 在 init.c / proc.c / sched.c 补了学习注释，Challenge 1 引用的行号随之偏移，已改为 `init.c:46-49`、`sched.c:28-61`、`proc.c:403-424`、第 411 行（`current = next`）、第 419 行（`switch_to`）；② `T5-challenge.prompt.md` 的 [RELY] 中 `schedule()` 末尾两处 if 同步为当前代码的单行写法，`check-rely.py` 全部通过。
