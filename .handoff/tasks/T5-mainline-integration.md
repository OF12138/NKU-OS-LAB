# T5 整体逻辑主线与集成
- 负责人：openfar    状态：待开始    依赖：T1–T4, T6
- 可改文件：report/sections/T5-*.md, report/report.md, report/prompt.md

## 要求
- 对应报告要求：说明本章的整体逻辑主线，即按什么顺序、围绕哪些功能逐步实现（report-template.md 第三部分）。
- 集成工作：把各 section 合并进 report.md，并整理 prompt.md。

## 当前进度 / 下一步

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言

- [lyp 2026-10-07] lyp 的 Codex bot：T1/T3/T4 报告及最终提示词已完成并待审，T7 仅更新 lyp 小节的书面复习答案。Lab1 截止约 2026-10-13，以群通知为准。请 openfar 审核并合并三份 section，T3 已内嵌三段提示词，T4 对应总结中的 OS 理解。本机 GCC 10.2/QEMU 6.2/OpenSBI v0.9，固件地址勿与 T2 混用；edata=end，ENTRY 不自动保证排序。未改实验代码和最终汇总文件。Windows/WSL 未找到 feishu/lark/feishu-cli/lark-cli，也无飞书连接工具，故未能 send as user 通知，先在这里留交接记录。

- [lyp 2026-10-07] lyp 的 Codex bot：交付同步补充，053629d 已推送；最终报告提交 c90afcc 因 GitHub 443 连续连接失败暂存本地。网络恢复后先 pull --rebase 再 push，届时请审核集成。飞书通知也尚未发送。

- [lyp 2026-10-07] lyp 的 Codex bot：根据用户反馈重写 T1/T3/T4 的表达，T1 引用已有入口换栈图，T3 引用 ecall 和 UART 调用栈图，均保留 ../images/ 引用并标明 T2 来源。最终提示词已同步；集成时请处理与 T2 的图片复用和统一图号。本轮只修改 lyp 章节及任务记录，没有生成或冒充新截图。GitHub 连接仍失败，修订先提交本地，再重试同步。

- [lyp 2026-10-07] lyp 的 Codex bot：用户要求不复用 T2 图，上一条有关复用 T2 的集成说明作废。T1/T3 已改用独立运行得到的 T1-entry-step.jpg、T3-symbols.jpg、T3-elf-layout.jpg、T3-ecall-step.jpg，截图来自本机原始命令输出展示页，已标明 QEMU 6.2.0 / OpenSBI v0.9。T2 图片与正文均未修改。

- [lyp 2026-10-07] lyp 的 Codex bot：GitHub 已恢复，所有截至 d5582a8 的本地提交已同步到 origin/lab1；前面的待推送说明已解决。请按最新 T1/T3 的独立配图版本审核集成，T2 文件未变。
