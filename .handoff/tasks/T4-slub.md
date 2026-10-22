# T4 Challenge：任意大小内存单元的 SLUB 分配算法
- 负责人：openfar（实现）、lyp（测试）    DDL：实现 10-22，测试 10-23    状态：完成    依赖：T1
- 主要文件：code/kern/mm/slub.*, code/kern/mm/slub_test.c, code/tests/run_slub_tests.py, code/kern/init/init.c, report/sections/T4-*

## 要求
指导书原文要点：
- 两层架构：第一层基于页大小的分配，第二层在第一层基础上实现任意大小的分配。可简化实现，体现主体思想即可。
- 设计两份提示词：第一份让 AI 在没有现成代码框架的情况下从零生成 SLUB 模块并接入内核（openfar）；第二份让 AI 针对第一份生成的实现设计测试函数，检验不同大小对象的分配和释放（lyp）。
- 报告：提交两份提示词，说明测试覆盖了哪些核心行为和边界情况，以及这些测试为什么能有效检验实现。

## 当前进度 / 下一步
- [x] 实现（openfar）：code/kern/mm/slub.h、slub.c，kern_init 中 pmm_init() 之后调用 slub_init()
- [x] 提示词一 report/sections/T4-slub.prompt.md；section report/sections/T4-slub.md（设计、提示词一、迭代过程、冒烟验证、图 T4-slub-layout.png）
- [x] 测试（lyp，10-22）：`slub_check()` 接入正常启动；覆盖尺寸、数据完整性、partial 状态转换、自定义对齐、描述符缓存、大块页数和真实 OOM。`code/tests/run_slub_tests.py` 在临时副本运行 4 次正常启动、6 次预期 panic，全部通过。
- [x] `T4-slub-test.prompt.md` 已完成；T4 section 补齐测试设计、覆盖理由、实测结果、复现命令与真实迭代；提示词一同步补充溢出约束。[RELY] 引用检查通过。
- 下一步：openfar 在 T9 合并 section 和两份提示词；nagilix 可将 T4 日志引用到 T8 的公共验证章节。

## 关键决策与结论
- SLUB 不是 pmm_manager：第一层直接用 alloc_pages()/free_pages()（默认 Best-Fit），不影响 make grade。
- slab 固定一页，页首 48 字节 struct slab 头（magic/inuse/cache/freelist/npages/slab_link），对象地址 ROUNDDOWN 到页即得到头。
- 每个缓存：cpu_slab（活动）+ partial 链表；满 slab 不在链表上；非活动 slab 变空立即还页，活动 slab 变空保留。
- kmalloc：7 个通用缓存 16～1024；>1024 按页分配，页首 LARGE_MAGIC 头记页数，返回页首 +48。
- 给测试用的接口：kmalloc_cache(size) 返回所用缓存；struct kmem_cache 的 size/objs_per_slab/nr_slabs/nr_partial/nr_inuse；slub_print_stats()。
- 非法释放（不属于该缓存/不在对象边界/重复释放）和 kfree 非法指针都会 panic，测试这类情况时注意会停机。
- `cache_setup()` 在 ROUNDUP 前拒绝 `size > PGSIZE` 或归一化后 `align >= PGSIZE`，避免整数回绕；现有接口及合法输入语义不变。
- 正常启动连续测试两轮：第一轮只保留 7 个通用缓存活动页，第二轮不额外损失页数。批量描述符测试显式处理 cache_cache 的活动页保留，并最终恢复初始一页状态。
- 页数预期按运行时 PMM 名称选择，支持免 clean 切换；负向宏变体在临时目录 clean 后构建，不影响工作区默认内核。

## 验证结果
- 临时副本冒烟测试（不进仓库）：各大小 kmalloc 落入正确缓存；600 个 64B 对象跨 10 个 slab 数据校验正确；释放后只剩活动 slab，空闲页 31926→31919（差 7 个活动 slab）；自定义 40B/16 对齐缓存步长 48；重复释放 panic。
- 上述为 openfar 实现阶段的冒烟结果；以下为 lyp 于 2026-10-22 完成的正式验证。
- QEMU 4.1.1/OpenSBI v0.4，改动前后默认 `make grade` 均为 20/20；日志 `temp/T4-baseline-grade.log`、`temp/T4-grade.log`。
- `make`、`make qemu` 成功，输出 `[SLUB] ALL TESTS PASSED`；日志 `temp/T4-{make,qemu}.log`。
- Best-Fit/Buddy × 128/64 MiB 共 4 次启动，每次两轮完整测试通过；第一轮分别恢复至 31919/15695/31918/15694 页（保留 7 页），第二轮无额外页损失。
- 6 个隔离负向用例均匹配 `slub.c` 中预期原因；完整日志和 `summary.log` 在 `temp/T4-slub-tests/`。
- 不 clean 按 Best-Fit→Buddy→Best-Fit 切换，三次正常启动全部通过；日志 `temp/T4-switch-*-{make,qemu}.log`。
- `python3 .claude/tools/check-rely.py` 和 `git diff --check` 通过；`code/tools/`、Best-Fit 原始测试及 pmm.c 未改动。

## 迭代素材
- [lyp 2026-10-22] 自查发现测试按编译宏判断 Buddy 页数，但 Makefile 免 clean 切换只重编译 pmm.c → 测试可能仍使用旧模式 → 页数预期改为读取实际 `pmm_manager->name`，随后补做免 clean 双向切换验证。
- [lyp 2026-10-22] 批量销毁 40 个缓存后 `nr_free_pages() == before` 断言失败 → 测试漏算 cache_cache 的空活动页保留策略，原描述符页仍有 7 个内置缓存 → 先验证合法的两页状态，再填满空活动页、提升原 partial 页并释放临时描述符，确认多余页回收；不是实现泄漏。日志 `temp/T4-descriptor-before.log`。
- [lyp 2026-10-22] 直接 `wsl ... python3` 运行脚本报 `riscv64-unknown-elf-gcc: not found` → 未加载登录 shell 的工具链 PATH → 改从 `bash -lc` 运行，内核代码无需调整。
- [lyp 2026-10-22] 首次边界测试触发 `assertion failed: kmem_cache_create("overflow", (size_t)-1, 8) == NULL` → `ROUNDUP(size, align)` 回绕为 0，错误接受无法容纳的缓存 → 在对齐前限定 `size <= PGSIZE`、`align < PGSIZE`，保留该用例及加法溢出/极大对齐回归；原始日志 `temp/T4-overflow-before.log`。
- 前辈把 SLUB 注册成 pmm_manager → 会让 grade 的 Best-Fit 检查失败 → 改为页分配器之上的独立模块。
- struct Page 无空余字段、扩充会改变 page_init 布局 → 元数据放页首；kmalloc 上限因此取 1024（2048 每页只能放 1 个）。
- pmm.h 的 KADDR 被注释 → 自写 page2kva（pa + va_pa_offset）。
- 编写释放路径时注意到：单对象 slab 从满直接变空，不在 partial 上，不能 list_del。
- 画图时把 kmalloc-1024 页尾剩余算成 16B，核对后应为 976B（3×1024+48=3120）。

## 留言
- [lyp 2026-10-22] 已补齐独立测试与报告，并在 slub.c 做上述极大输入溢出修复、slub.h 增加测试入口、init.c 增加启动调用；openfar 的学习注释与整体架构保留。测试前后的失败证据、修复和页数策略说明见本文件及 T4 section。
