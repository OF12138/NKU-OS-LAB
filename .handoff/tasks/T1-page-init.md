# T1 功能模块一 任务一：物理内存探测与页描述符初始化（page_init）
- 负责人：openfar    DDL：10-18    状态：完成    依赖：T0
- 主要文件：code/kern/mm/pmm.c, report/sections/T1-*

## 要求
指导书原文要点：实现 `static void page_init(void); (kern/mm/pmm.c)`。
- 根据设备树提供的物理内存信息建立 Page 描述符布局，把排除 OpenSBI、内核镜像和描述符数组之后的完整空闲页交给当前 pmm_manager。
- 成功返回时正确设置 `va_pa_offset`、`npage`、`pages`，输出探测到的物理内存范围，完成初始空闲区注册。
- 区分虚拟地址、物理地址、物理页号和 Page 描述符地址；正确处理可管理上界和页面对齐；设备树信息无效时终止初始化；没有完整空闲页时不得调用 `init_memmap()`。
- 可用：`get_memory_base()`、`get_memory_size()`、`panic()`、`cprintf()`、`init_memmap()`、`pa2page()`、链接符号 `end`、`struct Page` 及相关全局变量和宏。
- 设计提示词借助 AI 完成；报告写「功能模块」：最终提示词 + 实现迭代过程。
- 评测：`make grade` 的 `[Test 1] Page Init: PASS`（见 code/tools/test_lab2.c）。

## 当前进度 / 下一步
- [x] `page_init()` 已实现（code/kern/mm/pmm.c），YOUR CODE 已替换为 2411264
- [x] 提示词 report/sections/T1-page-init.prompt.md，报告 section report/sections/T1-page-init.md（功能模块格式，最终提示词用 `<!-- include:... -->` 引入，合并时展开）

## 关键决策与结论
- 可管理上界用物理地址 `PHYSICAL_MEMORY_END`（= KERNTOP - va_pa_offset）比较；常见写法 `maxpa > KERNTOP` 把物理地址和虚拟地址比较，永远不成立。
- `pages[i]` 对应物理页号 `nbase + i`，数组长度 `npage - nbase`；设备树 base 低于 DRAM_BASE、size 为 0 或溢出时 panic。
- 供 T2/T3/T4 使用：默认 128M 下空闲区 [0x80347000, 0x88000000)，共 31929 页，一次 `init_memmap` 注册为一个连续块。

## 验证结果
- 框架未实现 Best-Fit 时编译不过，验证在 WSL 临时副本中进行：把 default_pmm.c 复制为 best_fit_pmm.c 并改名为 best_fit_pmm_manager。
- `make grade`：check page init OK；check best fit WRONG（等 T2）。
- `-m 64M / 128M / 256M`：npage = 0x84000 / 0x88000 / 0x88000，空闲页 15705 / 31929 / 31929，与手算一致；`check_alloc_page() succeeded!`。

## 迭代素材
- 参考实现的上界截断 `maxpa > KERNTOP` 虚实地址混用，从不生效 → 改为与 PHYSICAL_MEMORY_END 比较，-m 256M 验证截断生效。
- 框架原样编译失败 `error: 'nr_free' undeclared`（best_fit 未实现）→ 临时副本用 First-Fit 冒充 best_fit 单独验证 Test 1。
- 自查：panic 格式串末尾多写了 `
`，panic() 自己会换行 → 删除。

## 留言
