# T5 整体逻辑主线与集成
- 负责人：openfar    状态：完成    依赖：T1–T4, T6
- 可改文件：report/sections/T5-*.md, report/report.md, report/prompt.md

## 要求
- 对应报告要求：说明本章的整体逻辑主线，即按什么顺序、围绕哪些功能逐步实现（report-template.md 第三部分）。
- 集成工作：把各 section 合并进 report.md，并整理 prompt.md。

## 当前进度 / 下一步
- 已完成：整体逻辑主线 report/sections/T5-mainline.md 及其提示词 T5-mainline.prompt.md。
- 已生成 report.md / prompt.md **草稿**（合并 T0–T5），T6 的 5 个部分用“【待补】”占位。T6 提交后重新运行合并脚本，再做终审。
- 2026-10-07：T6 已用 merge-report.py --fill 填入，全文 12 张图编号连续，无【待补】。终审修正：去掉对已删除章节「对实验框架的修改」的 3 处引用（把 Makefile 改动并入练习 2 的“版本差异”说明），图题全部改为 <p align="center">。
- 待办：李云鹏的 AI 模型待确认（环境表中标为“待确认”）；report.pdf 只作本地预览，已移出 git。

## 关键决策与结论
- 报告结构按 report-template.md：一 目的(T6) / 二 环境(T6) / 三 整体逻辑(T5) / 四 实验内容(T1→T2→T3→T0→T6 拓展) / 五 测试(T6) / 六 总结(T4 + T6)。
- 合并时图片路径统一改为 ./images/，图号按出现顺序统一编为“图 1～N”，正文中的旧图号引用同步替换，指向 *.prompt.md 的相对链接改为纯文本。
- 代码加入学习注释后，各提示词的 [RELY] 引文统一注明“省略注释”；kern_init 的大括号换了行，T4/T5 的引文已更新；T2 的 Makefile 引文更新为当前版本（-bios $(OPENSBI)、$(GDB_OPENSBI)）。

## 验证结果
- 当前 report.md 草稿共 11 张图，编号连续；[RELY] 逐行比对全部通过（比较时忽略注释）。

## 迭代素材
- 提交用户的学习注释后，多份提示词的 [RELY] 引文与源码不再逐字一致（行尾注释、kern_init 大括号换行、Makefile 新增 OPENSBI）。改为“省略注释并注明”，更新过时引文，并用脚本逐行比对确认。
- 三人的图号格式不同（图 T1-1、图 2-x、图 T3-x），合并时统一按出现顺序重新编号。

## 留言

- [lyp 2026-10-07] lyp 的 Codex bot：T1/T3/T4 报告及最终提示词已完成并待审，T7 仅更新 lyp 小节的书面复习答案。Lab1 截止约 2026-10-13，以群通知为准。请 openfar 审核并合并三份 section，T3 已内嵌三段提示词，T4 对应总结中的 OS 理解。本机 GCC 10.2/QEMU 6.2/OpenSBI v0.9，固件地址勿与 T2 混用；edata=end，ENTRY 不自动保证排序。未改实验代码和最终汇总文件。Windows/WSL 未找到 feishu/lark/feishu-cli/lark-cli，也无飞书连接工具，故未能 send as user 通知，先在这里留交接记录。

- [lyp 2026-10-07] lyp 的 Codex bot：交付同步补充，053629d 已推送；最终报告提交 c90afcc 因 GitHub 443 连续连接失败暂存本地。网络恢复后先 pull --rebase 再 push，届时请审核集成。飞书通知也尚未发送。

- [lyp 2026-10-07] lyp 的 Codex bot：根据用户反馈重写 T1/T3/T4 的表达，T1 引用已有入口换栈图，T3 引用 ecall 和 UART 调用栈图，均保留 ../images/ 引用并标明 T2 来源。最终提示词已同步；集成时请处理与 T2 的图片复用和统一图号。本轮只修改 lyp 章节及任务记录，没有生成或冒充新截图。GitHub 连接仍失败，修订先提交本地，再重试同步。

- [lyp 2026-10-07] lyp 的 Codex bot：用户要求不复用 T2 图，上一条有关复用 T2 的集成说明作废。T1/T3 已改用独立运行得到的 T1-entry-step.jpg、T3-symbols.jpg、T3-elf-layout.jpg、T3-ecall-step.jpg，截图来自本机原始命令输出展示页，已标明 QEMU 6.2.0 / OpenSBI v0.9。T2 图片与正文均未修改。

- [lyp 2026-10-07] lyp 的 Codex bot：GitHub 已恢复，所有截至 d5582a8 的本地提交已同步到 origin/lab1；前面的待推送说明已解决。请按最新 T1/T3 的独立配图版本审核集成，T2 文件未变。

- [lyp（Codex bot） 2026-10-08] 已基于最新远端同步 T1/T3/T4 修订；T1/T3 使用真实 VSCode/tmux 终端 PNG（T1-entry-step.png、T3-symbols.png、T3-elf-layout.png、T3-ecall-step.png）。旧 JPG 暂保留，因 report.md 仍在引用；请审核并更新 report.md/prompt.md 的相关章节及配图。Lab1 截止约 2026-10-13，以群通知为准。
