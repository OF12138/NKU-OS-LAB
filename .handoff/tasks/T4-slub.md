# T4 Challenge：任意大小内存单元的 SLUB 分配算法
- 负责人：openfar（实现）、lyp（测试）    DDL：实现 10-22，测试 10-23    状态：完成    依赖：T1
- 主要文件：code/kern/mm/slub.*, code/kern/mm/slub_test.c, code/kern/init/init.c, report/sections/T4-*

## 要求
- 两层架构：第一层基于页大小的分配，第二层在第一层基础上实现任意大小的分配；可简化，体现主体思想即可。
- 两份提示词：openfar 实现并接入模块，lyp 独立设计不同大小对象分配与释放的测试。
- 报告提交两份提示词，说明核心行为、边界情况及测试为什么有效。

## 当前进度 / 下一步
- [x] openfar：SLUB 模块、启动接入、设计图、提示词一与实现报告已完成。
- [x] lyp：重写为一次启动的简洁自测，删除 Python 运行器和负向启动宏。
- 测试源码由 333 行缩减至 108 行；T4 日志由 31 份收敛至 3 份。
- [x] 测试覆盖所有七种大小缓存、填满后新 slab、partial 转换与空页回收、128 个 64B 对象释放一半再复用，以及 1025/4096/8192B 页级回退。
- [x] 报告与提示词二已按现有测试重写；保留此前溢出修复及一条回归，清理旧测试日志，仅保留 `temp/T4-{make,qemu,grade}.log`。
- 下一步：openfar 在 T9 合并 T4 section 与两份提示词。

## 关键决策与结论
- SLUB 位于页分配器之上，第一层调用 alloc_pages/free_pages，不注册成 pmm_manager；默认仍为 Best-Fit。
- slab 固定一页，页首 48 字节头；小对象七种缓存 16～1024B，更大请求按页分配。
- 活动 slab 变空保留；非活动 slab 变空立即还页；满 slab 不在 partial 上。测试适配本实现，不套用前辈 full/partial 的计数。
- 每种缓存按 objs_per_slab 填满后额外申请一个对象，核对地址、容量、在用数、内容及释放结果；批量复用检查不申请新页。
- `cache_setup()` 保留先前极大尺寸溢出修复，测试保留 SIZE_MAX 输入断言；不改实现架构及学习注释。
- 自测由 slub_init 后调用 slub_check，一次正常 make qemu 即可。结束后七个通用缓存各留一个空活动页，其余测试页已归还。

## 验证结果
- 2026-10-22，WSL Ubuntu 22.04，QEMU 4.1.1/OpenSBI v0.4，默认 Best-Fit。
- `make` 编译通过；`make qemu` 各项检查通过，最终输出 `[SLUB] all checks passed (retained 7 active slab pages)`。
- 七个缓存的 nr_inuse/nr_partial 均为 0，nr_slabs 均为 1；缓存容量依次为 253/126/63/31/15/7/3。
- `make grade` page init/Best-Fit 均为 OK，总分 20/20，与前次基线一致。
- 日志仅保留 `temp/T4-make.log`、`temp/T4-qemu.log`、`temp/T4-grade.log`。
- 提示词 [RELY] 引用、报告链接与 include、`git diff --check` 均通过；被测实现、原启动接口与评测代码与本轮开始时一致。

## 迭代素材
- 前轮测试发现 `kmem_cache_create("overflow", (size_t)-1, 8)` 未返回 NULL → ROUNDUP 回绕为 0 → 在对齐前限定单页范围；本轮保留修复与回归，原失败日志随旧套件清理，Git 历史可查。
- 前轮批量描述符测试把空活动页保留误认为泄漏，且加入 OOM、多配置及六次 panic 启动 → 测试与说明超出本次实验所需 → 本轮按用户要求参照前辈报告，收敛到缓存分配释放、批量复用、大对象回退。
- 本轮首次编译、启动及评分均通过，没有新增代码失败或修复轮次。
- 前辈把 SLUB 注册为 pmm_manager → 不适合当前 Best-Fit 评分 → 保持页分配器之上的独立模块。
- struct Page 没有空余字段，扩充会改变布局 → 元数据放页首；2048B 每页仅一对象，因此通用缓存上限取 1024B。
- 编写释放路径时发现单对象 slab 可从满直接变空，不在 partial 上 → 释放时通过 was_full 避免错误摘链。

## 留言
- [lyp 2026-10-22] 已补齐独立测试与报告，并在 slub.c 做上述极大输入溢出修复、slub.h 增加测试入口、init.c 增加启动调用；openfar 的学习注释与整体架构保留。测试前后的失败证据、修复和页数策略说明见本文件及 T4 section。
- [lyp 2026-10-22] 按用户要求完成本地简化：测试改为单次启动，提示词二和测试章节重写，旧运行器及冗余日志删除。
- [openfar 10-22] 轻度审核 T4 测试（简化后的版本）：T4-slub.md 中 4 处 `../../code/...`、`../../temp/...` 相对链接改为纯文本路径（合并进 report.md 后相对路径会失效）。另外 `slub_check()` 每次启动都会运行，`make qemu` 的输出变长了，T8 的 make qemu 截图需要在集成时重拍。
