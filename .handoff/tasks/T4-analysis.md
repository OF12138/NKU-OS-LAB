# T4 整体逻辑分析；知识点与 OS 原理对应
- 负责人：openfar（整体逻辑）、lyp（知识点）    DDL：11-20    状态：待开始    依赖：T1、T2、T3
- 主要文件：report/sections/T4-logic.md（openfar）, report/sections/T4-knowledge.md（lyp）

## 要求
- 报告要求：“说明自己对本章节整体逻辑线的理解，即本章节是按照什么顺序、围绕哪些功能逐步实现的。”
  - openfar：按模板「三、实验整体逻辑分析」写，主线为指导书给出的 `kern_init → pmm_init → pic_init/idt_init → vmm_init → proc_init → cpu_idle → schedule → proc_run → switch_to`，配流程图（画成图片）。
- 报告要求：“列出你认为本实验中重要的知识点，以及与对应 OS 原理中的知识点，并简要说明二者的含义、关系和差异。列出你认为 OS 原理中很重要、但在本实验中没有对应上的知识点。”
  - lyp：按模板「六、对操作系统的理解」两小点写（例如 PCB、进程状态、上下文切换、内核线程与用户进程、页表映射与 TLB）。
- 各自附一份四段式提示词（`*.prompt.md`）。

## 当前进度 / 下一步

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
