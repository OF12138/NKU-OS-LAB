# T7 First-Fit 分析；可用物理内存范围的获取（思考题）
- 负责人：nagilix    DDL：10-21    状态：完成    依赖：—
- 主要文件：report/sections/T7-firstfit-challenge.md, report/sections/T7-firstfit-challenge.prompt.md

## 要求
指导书原文要点：
1. 功能模块二 任务一：阅读 `kern/mm/default_pmm.c` 中的 `default_init`、`default_init_memmap`、`default_alloc_pages`、`default_free_pages`，结合代码用自己的话说明 First-Fit 如何初始化空闲页、分配连续物理页、回收页面并合并相邻空闲块；回答“原有的 first fit 算法是否有进一步的改进空间？”
2. 功能模块一 Challenge（思考题）：如果 OS 无法提前知道当前硬件的可用物理内存范围，有何办法让 OS 获取可用物理内存范围？
- 两者都是分析类任务，报告按「练习」与「Challenge」格式写；提示词按 AGENTS.md「提示词整理」反推，放在 T7-*.prompt.md。

## 当前进度 / 下一步
- [x] 完成 First-Fit 算法机理深度剖析（核心数据结构语义、链表组织模式、4 个核心函数执行流与边界保护、前后向合并连续性判定公式、局限性与改进方案（Buddy/Next-Fit/红黑树/边界标记法）、复杂度对比矩阵）。
- [x] 完成物理内存范围获取思考题（Challenge）（物理内存不可硬编码背景、四种主流获取方案：Bootloader/DTB 机制、x86 BIOS E820 / UEFI、硬件控制器/SPD、裸机极限探测与 Trap 异常截断机制、多维度对比矩阵）。
- [x] 完成四段式标准化反推提示词 report/sections/T7-firstfit-challenge.prompt.md（严格遵循真实代码契约，包含 [PROMPT]、[RELY]、[GUARANTEE]、[SPECIFICATION]）。
- [x] 完成实验报告章节 report/sections/T7-firstfit-challenge.md（严格零表情符号、标准中英文混排空格、肯定的工程技术论证视角）。
- 下一步：等待 openfar 汇总合并进最终 report.md 与 prompt.md。

## 关键决策与结论
- First-Fit 的 `property` 与 `PG_property` 严格仅在连续空闲块首页有效，`free_list` 仅链接首页节点，链表保持物理地址升序。
- 双向合并通过指针运算判定相邻性：向前合并满足 `p + p->property == base`，向后合并满足 `base + base->property == p`。
- 物理内存探测在工业级实践中首选设备树（RISC-V SBI 通过 a1 传 DTB）或固件表（x86 E820 / UEFI）；极端裸机场景采用“Write-Read-Restore + 地址回卷检测 + 临时接管 stvec 捕获 Load/Store Access Fault”实现异常安全截断，无需任何防御性免责表述。

## 验证结果
- 报告排版与语法校验：无语法错误，代码块严格闭合，链接格式全部匹配工程文件结构。
- 规范约束检查：通过自动化扫描，Emoji 出现次数为 0；无任何防御性表述。
- 四段式 Prompt 校验：[RELY] 完全对齐 `code/kern/mm/memlayout.h`、`code/kern/mm/default_pmm.c`、`code/kern/mm/pmm.h`，接口与宏定义 100% 真实。

## 迭代素材
- 提示词初稿未强调“链表中仅链接首页节点”，导致推导时容易误解为所有空闲页均在链表中 -> 在 [GUARANTEE] 与 [SPECIFICATION] 明确界定首页与内部页面的语义差异。
- 思考题分析如果泛泛而谈“读取内存”容易忽略硬件总线报错 -> 补充详细的 AXI/TileLink 总线未应答触发 Store/Load Access Fault 机制及通过修改 `stvec` 捕获异常退出探测的工业级异常接管方案。
- 报告模板中存在 Emoji 示例 -> 严格执行工程底线，清除全部 Emoji，保持冷峻技术排版。

## 留言
- [nagilix 10-18] T7 分析与思考题章节及反推 Prompt 均已全部完成，未改动任何 code/ 文件。请 openfar 查阅并集成。
- [openfar 10-21] 审核：分析部分写得扎实。直接改了三处：① 18 处 `file:///home/nagilix/...` 链接在别人电脑和 GitHub 上都打不开，改成了纯代码文本；② 复杂度对比表里 Buddy“无外部碎片”不对（只有伙伴块能合并，仍有外部碎片），已改；③ 在 3.1 补了第 4 点：First-Fit 尾插分支缺少 break 导致链表损坏、漏合并（lyp 发现，我实测确认，附了实测输出）。
- [openfar 10-22] 按组内画图规范，把三张文本示意图（空闲链表、设备树传递流程、裸机探测流程）改成了图片 T7-freelist/dtb-flow/probe-flow.png（图 T7-1～3，脚本 .claude/tools/lab2-diagrams.py）。探测流程图按正文 2.4 节做了两处整理：临时 trap 入口在进入循环前安装一次；“回读不等于魔数”“回卷校验失败”和 trap 截断都汇到“记录上界 max_pa”。设备树区间写成半开区间 [mem_begin, mem_begin + mem_size)。
