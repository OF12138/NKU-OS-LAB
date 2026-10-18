# T4 Challenge：任意大小内存单元的 SLUB 分配算法
- 负责人：openfar（实现）、lyp（测试）    DDL：实现 10-22，测试 10-23    状态：进行中（实现完成，等 lyp 测试）    依赖：T1
- 主要文件：code/kern/mm/slub.*, report/sections/T4-*

## 要求
指导书原文要点：
- 两层架构：第一层基于页大小的分配，第二层在第一层基础上实现任意大小的分配。可简化实现，体现主体思想即可。
- 设计两份提示词：第一份让 AI 在没有现成代码框架的情况下从零生成 SLUB 模块并接入内核（openfar）；第二份让 AI 针对第一份生成的实现设计测试函数，检验不同大小对象的分配和释放（lyp）。
- 报告：提交两份提示词，说明测试覆盖了哪些核心行为和边界情况，以及这些测试为什么能有效检验实现。

## 当前进度 / 下一步
- [x] 实现（openfar）：code/kern/mm/slub.h、slub.c，kern_init 中 pmm_init() 之后调用 slub_init()
- [x] 提示词一 report/sections/T4-slub.prompt.md；section report/sections/T4-slub.md（设计、提示词一、迭代过程、冒烟验证、图 T4-slub-layout.png）
- [ ] 测试（lyp，10-23）：按 section 末尾【待补】处的说明写测试函数和提示词二（T4-slub-test.prompt.md），并补全 section 的“提示词二”部分

## 关键决策与结论
- SLUB 不是 pmm_manager：第一层直接用 alloc_pages()/free_pages()（默认 Best-Fit），不影响 make grade。
- slab 固定一页，页首 48 字节 struct slab 头（magic/inuse/cache/freelist/npages/slab_link），对象地址 ROUNDDOWN 到页即得到头。
- 每个缓存：cpu_slab（活动）+ partial 链表；满 slab 不在链表上；非活动 slab 变空立即还页，活动 slab 变空保留。
- kmalloc：7 个通用缓存 16～1024；>1024 按页分配，页首 LARGE_MAGIC 头记页数，返回页首 +48。
- 给测试用的接口：kmalloc_cache(size) 返回所用缓存；struct kmem_cache 的 size/objs_per_slab/nr_slabs/nr_partial/nr_inuse；slub_print_stats()。
- 非法释放（不属于该缓存/不在对象边界/重复释放）和 kfree 非法指针都会 panic，测试这类情况时注意会停机。

## 验证结果
- 临时副本冒烟测试（不进仓库）：各大小 kmalloc 落入正确缓存；600 个 64B 对象跨 10 个 slab 数据校验正确；释放后只剩活动 slab，空闲页 31926→31919（差 7 个活动 slab）；自定义 40B/16 对齐缓存步长 48；重复释放 panic。
- make grade：check page init OK（best fit 等 T2）。

## 迭代素材
- 前辈把 SLUB 注册成 pmm_manager → 会让 grade 的 Best-Fit 检查失败 → 改为页分配器之上的独立模块。
- struct Page 无空余字段、扩充会改变 page_init 布局 → 元数据放页首；kmalloc 上限因此取 1024（2048 每页只能放 1 个）。
- pmm.h 的 KADDR 被注释 → 自写 page2kva（pa + va_pa_offset）。
- 编写释放路径时注意到：单对象 slab 从满直接变空，不在 partial 上，不能 list_del。
- 画图时把 kmalloc-1024 页尾剩余算成 16B，核对后应为 976B（3×1024+48=3120）。

## 留言
