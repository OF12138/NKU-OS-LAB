# T2 功能模块二 任务二：Best-Fit 连续物理内存分配
- 负责人：lyp    DDL：10-20    状态：完成    依赖：T1
- 主要文件：code/kern/mm/best_fit_pmm.c, code/kern/mm/pmm.c, report/sections/T2-*

## 要求
指导书原文要点：
- 参考 `kern/mm/default_pmm.c` 的 First-Fit 实现，设计提示词，让 AI 在 `kern/mm/best_fit_pmm.c` 中标有 lab2 和 YOUR CODE 的位置实现完整的 Best-Fit 物理内存管理模块。
- 文件只提供 `basic_check()` 和 `best_fit_check()`，不提供管理函数声明和 `best_fit_pmm_manager` 注册结构；需根据 `pmm.h` 中的 `struct pmm_manager` 自行设计并注册。实现必须符合测试函数使用的状态约定，**不得修改两个测试函数**。
- 在 `kern/mm/pmm.c` 的 `init_pmm_manager()` 中切换为 `best_fit_pmm_manager`。
- 报告：提交提示词（功能模块：最终提示词 + 实现迭代过程），回答“你的 Best-Fit 算法是否有进一步的改进空间？”
- 评测：`make grade` 的 `[Test 2] Best-Fit: PASS`。

## 当前进度 / 下一步
- 已核对 AGENTS.md、CLAUDE.md、指导书练习与报告要求；T1 已完成。
- 已实现地址有序空闲链表、最小适合块选择、分裂与双向合并；默认管理器切换为 Best-Fit。
- 已完成简洁报告 `report/sections/T2-best-fit.md`、四段式提示词 `T2-best-fit.prompt.md` 和独立回归 `code/tests/`；提示词引用通过 check-rely.py。
- T2 已完成，报告与提示词待 openfar 汇总；T3、T4 测试、T5 TLB、T6 知识点尚未在本轮开展。

## 关键决策与结论
- 初始化与回收共用插入/合并逻辑；等大小选低地址，精确匹配提前结束扫描；框架的两个测试函数保持原样。

## 验证结果
- 改动前 `make -j2` 失败：`free_list`、`nr_free` 未声明，无法启动或评分；不虚构基线分数。
- WSL `Ubuntu-22.04-OS`：GCC 10.2.0；QEMU `/home/lenovo/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64`，版本 4.1.1，自带 OpenSBI v0.4。非登录 shell 需 `PATH=/opt/riscv/bin:$PATH`；通过 QEMU 环境变量选版本。
- `make` 编译通过；`make qemu` 输出 `memory management: best_fit_pmm_manager`、`check_alloc_page() succeeded!` 与 SLUB 初始化信息，无 panic。内核空转后用 timeout 正常终止。
- `make grade`：check page init OK、check best fit OK，Total Score: 20/20。
- `bash code/tests/run_best_fit.sh`：selection/split/failure、predecessor/successor/bridge merge、memmap/state restoration 三组均 PASS，恢复 31929 页。脚本不修改真实内核或评测，需先设置 PATH、QEMU。
- 原始输出：`temp/T2-{make,qemu,grade,regression}.log`；两个不可修改的框架自检函数与 HEAD 一致，评分脚本未改。

## 迭代素材
- 原框架 `error: 'free_list' undeclared` / `'nr_free' undeclared` → Best-Fit 的空闲区状态与管理函数尚未实现 → 补齐 free_area、六个管理器接口并注册。
- 阅读 First-Fit 参考实现时发现尾插分支缺少 `break`，插入后会再次访问自身 → Best-Fit 改为先定位后继、循环外只插入一次，避免照搬该缺陷。
- 本机两个 WSL 的现有 QEMU 分别为 6.2.0、7.0.0 → 不符合统一环境 → 在实验 WSL 单独安装带调试信息的 QEMU 4.1.1。
- 补充验证第一次执行报 `riscv64-unknown-elf-gcc: not found` → 非登录 WSL 未加载工具链 PATH → 显式传入 `/opt/riscv/bin` 后三组回归通过；并发构建时 tar 曾报 `file changed as we read it`，调整为顺序执行验证。

## 留言
