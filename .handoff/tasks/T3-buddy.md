# T3 功能模块二 任务三：Buddy System 分配算法
- 负责人：lyp（实现）、openfar（测试）    DDL：实现 10-22，测试 10-23    状态：完成    依赖：T1
- 主要文件：code/kern/mm/buddy_pmm.*, report/sections/T3-*

## 要求
指导书原文要点：
- Buddy System 把可用空间划分为大小为 2 的幂的块进行管理。
- 设计两份提示词：第一份让 AI 参考现有 ucore 物理内存管理框架，从零生成 Buddy System 分配模块并接入内核（lyp）；第二份让 AI 针对第一份生成的实现设计测试函数，检验其正确性（openfar）。
- 不提供框架和测试函数；最终模块和测试能正常编译运行并通过。
- 报告：提交两份提示词，说明测试覆盖了哪些核心行为和边界情况，以及这些测试为什么能有效检验实现。
- 接入后默认分配器仍须是 best_fit（`make grade` 检查 Best-Fit），Buddy 通过编译开关启用，建议做法：Makefile 中加 `PMM ?= best_fit`，经 `-DPMM_xxx` 传给编译器，`init_pmm_manager()` 用条件编译选择；`make grade` 不带参数时仍是 best_fit。

## 当前进度 / 下一步
- [x] lyp：Buddy 实现、PMM 编译开关、启动自检、SLUB 兼容冒烟、默认评分验证已完成（2026-10-21）。
- [x] lyp：`report/sections/T3-buddy.md`、`T3-buddy.prompt.md` 已完成；引用检查通过。
- [x] lyp（10-22）：参照 T4 润色报告，补充初始化、分裂与合并的连贯说明及示例；按用户后续要求，将实现方验证压缩为编译启动、自检、默认功能回归三项，移除 SLUB 冒烟测试说明，T3 不再安排该项测试；提示词一同步更新。此前已完成的冒烟结果保留在下方作为历史记录。
- [x] openfar（10-22）：独立测试 `code/kern/mm/buddy_test.c`（`make PMM=buddy` 时由 kern_init 调用）、提示词二 `T3-buddy-test.prompt.md`、section 最后一节（覆盖说明、变异测试、实测结果）已完成。
- 已通过飞书 CLI 以 user 身份通知张远，开头标明“lyp 的 Codex 助手代发”，说明 10-26 总截止、10-23 测试交接和本地改动尚未推送；消息 ID `om_x100b62ba5f8fbca4cef91e89c634445`。
- 用户要求不 commit / push；保留现有 Best-Fit 学习注释。实现完成后交由 openfar 独立测试并撰写提示词二。
- 本次报告润色按用户要求不发送飞书；`git pull --rebase` 因已有未提交改动未执行成功，保留工作区现状。

## 关键决策与结论
- 按物理页号对齐，伙伴页号用 PFN XOR 块页数计算；0～15 阶覆盖当前不超过 128 MiB 的受管物理内存。
- `alloc_pages(n)` 实际占用向上取整后的 2 的幂页数；释放必须传入原分配起点和原请求 n（或同一块实际页数），不支持部分释放。分配首页 property 记录实际页数，空闲首页用 PG_property 标记。
- 默认 Best-Fit，`make PMM=buddy` 启用 Buddy；补充模式依赖以支持切换后不执行 clean 也能重新编译管理器选择代码。
- 交接接口为 `buddy_pmm_manager` 的标准六个回调；`BUDDY_MAX_ORDER=15`。启动的 `.check` 是实现方自检，openfar 可扩充它或增加独立测试，不要复用只允许 Best-Fit 的 T2 测试。

## 验证结果
- openfar 独立测试（10-22）：`make PMM=buddy qemu` 128 MiB / 64 MiB 均输出 `[BUDDY] all tests passed`，日志 `temp/T3-buddy-test.log`；变异测试 8/8 被发现，日志 `temp/T3-buddy-mutation.log`；默认 `make grade` 20/20。
- 改动前 QEMU 4.1.1 下 `make grade` 为 20/20，日志 `temp/T3-baseline-grade.log`。
- `make PMM=buddy` 编译、启动成功；`buddy: check passed (31929 free pages)`，随后 SLUB 正常初始化，日志 `temp/T3-buddy-{make,qemu}.log`。
- 不 clean 执行 `make` 会重编译 pmm.c，默认启动确认为 Best-Fit；日志 `temp/T3-switch-default-{make,qemu}.log`。
- 不 clean 再切回 Buddy，64 MiB 启动自检通过，恢复 15705 页；日志 `temp/T3-switch-buddy-make.log`、`temp/T3-buddy-64M.log`。
- `temp/T3-integration-smoke.py` 在临时副本插入 SLUB 9000/20000 字节分配、边界数据校验与释放：原请求 3/5 页实际占用 4/8 页，释放后空闲计数完全恢复，输出 `[T3 integration] SLUB large requests/reclaim: PASS`；日志 `temp/T3-integration.log`。
- 改动后默认 `make grade` 保持 20/20（`temp/T3-grade.log`）；不可修改的 Best-Fit 自检、code/tools 评测文件、Page 布局及 pmm.h 与 HEAD 一致。
- 10-22 文档润色：逐项核对 Buddy 实现及既有日志；`python .claude/tools/check-rely.py`、提示词 include 路径检查和 `git diff --check` 均通过。相关代码与 T4 文稿的文件哈希未变，本轮未重新执行内核测试。

## 迭代素材
- [openfar] 测试区的伙伴可能是区外空闲块，合并会越出测试区、地址无法预测 → 测试前先借出其余全部空闲页，借出总数顺便核对空闲计数。
- [openfar] 初版用 KADDR 写内存，框架导入时该宏被注释，[RELY] 无法只依赖框架 → 改用 page2pa + va_pa_offset。
- [openfar] 首轮变异测试中 M5 被 SLUB 自测先拦下、M7 在本测试前停住无输出 → 变异时同时关闭 slub_check 后重跑，8 种全部由 buddy_test 发现。
- 参考资料的树数组只覆盖最多 2048 页，当前空闲区约 31929 页 → 不能照搬固定规模限制 → 按阶链表登记所有完整页，不截断、不为树额外占页。
- 检查 SLUB 调用发现大块释放传原请求页数 → Buddy 实际分配页数会向上取整 → 首页保留实际块长，free 同样取整并验证整块。
- 原 Makefile 不跟踪 PMM 编译选项，存在切换后复用旧管理器对象的隐患 → 新增互斥模式标记，pmm.o 显式依赖当前标记；不 clean 切换实测通过。
- 本轮实现首次编译和启动自检通过，没有实际发生的代码失败轮次；报告按真实设计、自查与验证过程整理。
- 报告把分配与释放统一写成 O(log N) → 释放函数还会逐页清理实际占用的 S 页 → 修正为分配 O(log N)、释放 O(S + log N)，并同步到提示词的报告要求。

## 留言
- [openfar 10-22] 轻度审核 T3：实现逻辑正确，统一环境下复跑 `make grade` 20/20、`make PMM=buddy qemu` 输出 `buddy: check passed`。改动：temp/T3-*.log 原为 UTF-16 加乱码的 WSL 警告行，已转成纯文本（内容未改）。独立测试和提示词二按计划由 openfar 于 10-23 完成。
