# T1 功能模块一：虚拟内存页操作（page_insert / page_remove）
- 负责人：lyp    DDL：11-18    状态：完成    依赖：T0
- 主要文件：code/kern/mm/pmm.c, report/sections/T1-page-insert.md, report/sections/T1-page-insert.prompt.md

## 要求
- 指导书「功能模块一」：在 `kern/mm/pmm.c` 中实现（标记 `//LAB4 function1 YOUR CODE`）：
  - `int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);`
  - `void page_remove(pde_t *pgdir, uintptr_t la);`
- 注意事项（指导书原意）：保持“页表项内容、物理页引用计数、TLB 状态”三者同步；`page_insert` 要正确处理原地址已映射到同一物理页、已映射到其他物理页两种情况；`get_pte(pgdir, la, 1)` 可能失败，此时返回 `-E_NO_MEM` 且不留半完成映射；`page_remove` 是幂等接口，未映射时安静返回；更新后要使对应 TLB 失效。
- ### 可用接口：`get_pte()`、`pte_create()`、`pte2page()`、`page_ref_inc/dec()`、`set_page_ref()`、`alloc_page()`、`free_page()`、`tlb_invalidate()`、`flush_tlb()`，权限位 `PTE_V/U/W/R`。
- 评测：`tools/test_lab4.c` 的 `test_lab4_pmm_basic()`（插在 `pmm_init()` 末尾），输出 `[Test PMM] PASS` 得 10 分。它覆盖：同地址换页时旧页引用计数归零、同页重复插入只改权限且不泄漏页、重复 remove 不报错。
- 提示词：按四段式写一份，[RELY] 引用框架原始代码（上述函数、宏、`struct Page`）。
- 报告 section（模板「功能模块」）：模块功能描述、最终提示词（`<!-- include:T1-page-insert.prompt.md -->`）、实现迭代过程。
- 代码注释中的 `YOUR CODE` 替换为学号 2414099。

## 当前进度 / 下一步
- 2026-11-17：按用户要求移除额外创建的辅助验证目录，清理报告及任务文件中的失效引用，保留此前测试结果摘要。
- 2026-11-17：按 `get_pte` 的格式为 `page_insert`、`page_remove` 补充中文功能、参数与返回值注释，未修改函数逻辑；`git diff --check` 通过。本轮启动拉取仍被已有未暂存改动阻止，沿用不提交、不推送、不发送消息的当前安排。
- 2026-11-17：完成 `page_insert`、`page_remove` 及 T1 报告、四段式技术提示词；保留原有学习注释。报告和提示词位于要求指定的 section 文件，等待 openfar 集成。

## 关键决策与结论
- 对外接口签名保持不变。先取得新页引用再处理旧映射；同页抵消新增引用，异页复用 `page_remove`；删除清零 PTE 并刷新 TLB 后才减引用、归零释放。
- `get_pte` 失败时不改变目标数据页引用，不留下有效末级映射；沿用其现有中间页表分配策略，不增加中间页表回滚或自动回收。
- T2/T3 当前缺失 `proc_init`、`cpu_idle`，在独立临时副本加启动桩验证 T1；工作区没有补写其他任务实现。该副本总分不作为最终集成成绩。

## 验证结果
- 2026-11-17：在 WSL Ubuntu-22.04-OS 的当前 `code/` 中实际运行 `timeout 25s make debug`，链接阶段即失败：`undefined reference to 'proc_init'`、`undefined reference to 'cpu_idle'`，`make: *** [Makefile:201: bin/kernel] Error 1`；未启动 QEMU 或 GDB 服务。T2/T3 实现前，当前整树的 `make debug` 不能通过。
- WSL Ubuntu-22.04-OS，QEMU 4.1.1 / OpenSBI v0.4：独立副本 `make -j4` 成功；`make qemu` 的 `check_pgdir()`、`check_boot_pgdir()` 等自检通过，补充 alias / permission / TLB remap / OOM / idempotence / reclaim 断言通过。
- 未修改官方测试：独立副本 `make grade` 的 PMM 项 OK（10/10），总分 10/30；Process Create / Schedule 尚未实现，grade 返回非零。辅助日志已按用户要求移除。
- 全新无桩副本仅剩 `proc_init`、`cpu_idle` 链接错误。当前整树不能声明编译或全量评测通过。
- `.claude/tools/check-rely.py` 全部通过；`git diff --check` 通过；评测串口确认同时出现 `[Test PMM] PASS` 和 `[T1 edge] ... PASS`。

## 迭代素材
- `No rule to make target 'kern/mm/best_fit_pmm.h', needed by 'obj/kern/mm/pmm.o'` → 本地残留旧 `.d` 依赖 → 改在排除 `obj/`、`bin/` 的全新副本验证，未修改当前头文件依赖。
- `undefined reference to 'proc_init'` / `undefined reference to 'cpu_idle'` → T2/T3 尚未实现 → 独立副本补最小启动桩，工作区不接管其他任务；报告明确模块测试与整体验收的区别。
- 验证夹具自查：官方 PMM 测试会保留中间页表，若补充测试无条件清理会使前后空闲页数比较失真 → 增加原有页表状态判断，仅回收本测试创建的页表（运行前修正）。

## 留言
- [openfar 11-19] 轻度审核通过：page_insert/page_remove 逻辑正确，PMM 10/10，check-rely 通过，未改动代码。openfar 提交学习注释时与 T1 的 get_pte 注释冲突，已合并保留双方注释（函数头保留你的中文参数说明，下方加了一段 Sv39 走表说明）。section 最后一段“工作区仍无法链接”在 T3 完成后会过时，集成时由 openfar 改为最终结果。
