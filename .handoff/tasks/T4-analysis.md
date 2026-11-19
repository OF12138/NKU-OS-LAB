# T4 整体逻辑分析；知识点与 OS 原理对应
- 负责人：lyp    DDL：11-20    状态：完成    依赖：T1、T2、T3
- 主要文件：report/sections/T4-logic.md, report/sections/T4-knowledge.md, report/images/T4-boot-schedule.svg

## 要求
- 报告要求：“说明自己对本章节整体逻辑线的理解，即本章节是按照什么顺序、围绕哪些功能逐步实现的。”
  - 按模板「三、实验整体逻辑分析」写（`T4-logic.md`），主线为指导书给出的 `kern_init → pmm_init → pic_init/idt_init → vmm_init → proc_init → cpu_idle → schedule → proc_run → switch_to`，配流程图（画成图片，放 `report/images/T4-*`，不要用代码块字符画）。
- 报告要求：“列出你认为本实验中重要的知识点，以及与对应 OS 原理中的知识点，并简要说明二者的含义、关系和差异。列出你认为 OS 原理中很重要、但在本实验中没有对应上的知识点。”
  - 按模板「六、对操作系统的理解」两小点写（`T4-knowledge.md`），例如 PCB、进程状态、上下文切换、内核线程与用户进程、页表映射与 TLB。
- 两部分各附一份四段式提示词（`T4-logic.prompt.md`、`T4-knowledge.prompt.md`）。
- 文中结论要对照最终代码核实（T1～T3 推送后）。

## 当前进度 / 下一步

- 两节正文、流程图和两份四段式提示词已完成。集成时将「整体逻辑分析」放入第三章，将「对操作系统的理解」放入第六章，并将图片路径由 `../images/` 改为 `./images/`。

## 关键决策与结论

- 以 T1～T3 已完成的 `3feb952` 为准：首次调度由 idle 的 `need_resched` 触发；首次启动用预构造的陷阱帧进入 S 态，新线程与 idle 共用启动页表。
- 将环形扫描的选取顺序、执行现场切换、时钟抢占分开说明，不把已有入口或状态定义写成已经实现的机制。

## 验证结果

- 对照 `init.c`、`pmm.c`、`vmm.c`、`proc.c`、`sched.c`、`switch.S` 和陷阱入口核对调用顺序及边界；检查 SVG 渲染、章节图片相对路径和提示词四段结构。
- 本任务仅涉及报告和协作状态，未修改内核或重跑 QEMU 评测。

## 迭代素材

- 看板要求的主线省略了开中断细节；对照 `kern_init()` 后在 `proc_init()` 与 `cpu_idle()` 之间补入 `clock_init()`、`intr_enable()`，并核对定时器分支没有发起调度。

## 留言
- [openfar 11-20] 审核：两节正文对照最终代码核实无误（pic_init 空实现、首次调度由 need_resched 触发、局部/全量 sfence.vma 等都正确）。直接改了两处：① `T4-boot-schedule.png` 按报告统一风格（黑白、直角、宋体/Consolas）重画，内容与你的原图一致，脚本在 `.claude/tools/lab4-diagrams.py`；② 图下补了图题。**待补：** 任务文件写的 `T4-logic.prompt.md`、`T4-knowledge.prompt.md` 没有提交到仓库（`report/sections/` 下没有），请补交；任务文件里的图名写的是 `.svg`，仓库中是 `.png`。
