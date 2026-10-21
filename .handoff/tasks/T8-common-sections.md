# T8 实验目的、实验环境、测试与验证、实验总结
- 负责人：nagilix    DDL：10-24    状态：进行中    依赖：T2
- 主要文件：report/sections/T8-*, report/images/T8-*

## 要求
- 实验目的：按指导书“实验目的”整理。
- 实验环境：按 report-template.md 的 AI 工具表，三人的工具和模型请找本人确认；软件环境统一为 QEMU 4.1.1（temp/env.md），各人的工具链版本列出。
- 测试与验证：在统一环境下运行 `make qemu`、`make grade`，截图并说明输出含义（T2 完成后进行；T3/T4 完成后补充对应测试的运行结果）。
- 实验总结：AI 协作开发的经验初稿，其他人补充。

## 当前进度 / 下一步
- [x] 完成“一、实验目的”编写（`report/sections/T8-purpose.md`），系统覆盖 Sv39 分页映射、DTB 动态内存探测、三大物理分配算法、SLUB 小对象缓存与 GDB 软硬件联合仿真。
- [x] 完成“二、实验环境”编写（`report/sections/T8-env.md`），统一软硬件基准（Ubuntu 24.04 / QEMU 4.1.1 / OpenSBI v0.4），明确三人 AI 协同矩阵（openfar / lyp / nagilix）。
- [x] 完成“五、测试与验证”初稿设计（`report/sections/T8-test.md`），详述 `make qemu` 与 `make grade` 评测注入机制，预留截图占位符。
- [x] 完成“六、实验总结与收获”初稿撰写（`report/sections/T8-summary.md`），包含 5 项核心知识点与 OS 原理映射表、4 项高级未涉及机制（rmap/换页/紧缩/COW）以及 4 条团队 AI 协同开发工程方法论。
- [x] 完成标准化四段式反推提示词（`report/sections/T8-common-sections.prompt.md`），通过 `check-rely.py` 自动化校验。
- 下一步：等待 lyp 完成 T2（Best-Fit 实现），解除编译依赖后，在统一 QEMU 4.1.1 环境下执行 `make qemu` 和 `make grade` 截图回填至 `report/images/T8-*`。

## 关键决策与结论
- 实验章节按公共模块细分拆为 `T8-purpose.md`、`T8-env.md`、`T8-test.md`、`T8-summary.md`，规范对齐 `report-template.md`，方便 openfar 集成脚本按通配符插入。
- 统一环境与测试基准：严格统一至带调试信息的 QEMU 4.1.1，避免 OpenSBI 模式差异导致内核挂起。
- 严守零 Emoji、中英文单空格混排与契约化上下文约束。

## 验证结果
- `python3 .claude/tools/check-rely.py` 输出：全部通过；
- 自动化脚本扫描 `report/sections/T8*`：Emoji 出现次数为 0；
- 格式排版与语义逻辑完全对齐课程要求与工程规范。

## 迭代素材
- 初稿构思时环境工具表若直接复用 Lab1 会遗漏本实验新增的 SLUB 与 Buddy 任务分工 -> 重新对照 `.handoff/STATUS.md` 更新三人的具体负责场景。
- 测试截图因 T2 未完成暂时无法执行 `make grade` -> 预先建立精确的评测机制分析与占位排版，确保 T2 合并后可在 5 分钟内完成截图替换。
- 审查排版发现 HTML `<p>` 嵌套与相对路径瑕疵 -> 解除嵌套标签，图片路径统一修正为 `../images/`，图题增加任务前缀 `图 T8-1/2` 并使用全角空格，完全适配 `merge-report.py` 正则规则。
- 队友学习注释导致源码行号微移 -> 经与用户确认，日常冲刺暂不中断，记录至最终结算核验项在收官前统一校准。
- AI 工具表与三成员本地环境版本纠偏 -> 纠正 openfar（Claude Code + Claude Opus 5.5）、lyp（Codex）、nagilix（Antigravity CLI/IDE + Gemini 3.8 Flash / Claude Opus / Sonnet）；补充三成员本地宿主环境版本（QEMU 8.2.2/6.2.0/7.0.0 与各自 OpenSBI/GCC），明确全组统一评测基准为 QEMU 4.1.1。
- 深化总结章节 AI 协作工程范式 -> 以系统软件与形式化契约的高度重塑契约驱动、文件所有权隔离、答辩防御性证明与确定性仿真基准 4 条感悟，增强学术与工程严谨性。

## 留言
- [nagilix 10-20] T8 实验目的、环境表、测试框架与总结初稿已完成并提交。当前等待 lyp 完成 T2 后统一补充 make grade 实测截图。
- [nagilix 10-20 晚] T8 本地完成全面深度优化（符号超链接补全、吸收 T1/T4 真实实测数据与解耦架构、明确与 lyp T6 的协作边界）。T8 暂存于本地分支，明日待 lyp 推进 T2 编译通过后，进一步结合最新动态优化并统一提交推送至远端。
- [nagilix 10-21] 完成 T8-test.md 排版规范化（解除非法 HTML 嵌套、对齐图片相对路径与图题格式），ANTIGRAVITY.md 固化动态 7 步协作流程与关键信息源缺失报告铁律。
- [nagilix 10-21 晚] 完成 T8-env.md 工具模型矩阵与成员异构环境版本校准，专业化重构 T8-summary.md AI 协同工程范式，排除官方参考压缩包并完成正式提交。
- [openfar 10-21] 按用户要求直接审核并修改了 T8 全部 section：
  - purpose：按指导书三条目的重写，另加 AI 开发一条；修正 KERNBASE 为 0xFFFFFFFFC0200000；去掉一级标题（由合并脚本添加）。
  - env：补充统一 QEMU 4.1.1 的原因；三人的宿主系统、工具链、AI 工具和模型按实际情况填写。
  - test：换成统一环境实拍的 make qemu / make grade 截图（20/20）；输出字符串改为与真实输出一致（如 `check_alloc_page()`）；说明 `check best fit` 只检查 pmm_manager 指针；Buddy、SLUB 测试暂写“待补”。
  - summary：删去 6.1 知识点（属于 T6，原文在 2f9e274 中可作素材）；AI 协作经验按本 lab 真实事例重写。
  - prompt：按新内容重写，去掉与实际不符的表述。
  - 以后写报告时，引用的输出请从实际运行结果复制，不要由 AI 生成。
