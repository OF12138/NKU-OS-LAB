# T6 实验目的 / 环境 / 运行截图 / 拓展 / 总结
- 负责人：nagilix    状态：完成    依赖：无
- 可改文件：report/sections/T6-*.md, report/images/T6-*

## 要求
- 实验目的：参照指导书的「实验目的」页，用自己的话写 3～4 条。
- 实验环境表：收集三人使用的 AI 工具和模型（格式见 report-template.md 第二部分）。
- 运行截图：在自己的环境中运行 make qemu，截取包含 OpenSBI 输出和 `(THU.CST) os is loading ...` 的画面，并注明所用的 QEMU 版本。
- 拓展：简述现代笔记本（UEFI → 引导程序 → OS）的启动流程，与本实验的 MROM → OpenSBI → 内核做对比。
- 实验总结：先写初稿，其他成员之后可以补充 AI 协作经验。

## 当前进度 / 下一步
- [x] 实验目的梳理完成 (report/sections/T6-purpose.md)
- [x] 实验环境表实名制规范对齐，记录张远、李云鹏、刘昀皓三人软硬件及 AI 工具协同矩阵 (report/sections/T6-env.md)
- [x] make qemu 成功运行，截图并注明 QEMU 7.0.0 版本 (report/sections/T6-qemu-run.md, report/images/T6-qemu-run.png)
- [x] 现代笔记本启动时序对比分析完成，建立 x86 UEFI / ARM TF-A / RISC-V 5 层横向对齐 (report/sections/T6-extension.md)
- [x] 撰写实验总结初稿，以团队视角梳理架构认知并预留成员补充槽位 (report/sections/T6-summary.md)
- [x] 依据 AGENTS.md 规范整理四段式交付提示词 (report/sections/T6-misc.prompt.md)

## 关键决策与结论
- 图片严格统一引用相对路径 `../images/T6-qemu-run.png`，防止 openfar 聚合报告时断链。
- 拓展题不仅对比了常规 UEFI 流程，还补全了 ARM64 TF-A 架构与 RISC-V MaskROM 真实硬件启动链的映射，保证答辩理论深度。
- 实验总结弱化个人痕迹，以小组整体时序认知突破和规范化协同为主线，预留成员反思接口。

## 验证结果
- 本地 `make qemu` 引导验证无误，成功输出 OpenSBI Banner 与 `(THU.CST) os is loading ...`。
- 本地 6 篇 T6 独立 Markdown 章节生成完毕，格式均符合 UTF-8 编码规范。

## 迭代素材
- [2026-10-07] WSL2 终端下运行 make qemu 后终端独占停在 os is loading ... 画面 → 内核已成功进入 S 模式死循环挂起，需通过 Ctrl+A 再按 X 退出模拟器 → 顺利截取有效输出并完成归档。
- [2026-10-07] WSL2 环境 QEMU 7.0.0 与队友 8.2.2 版本存在细微固件差异 → 报告中明确注明当前环境为 QEMU 7.0.0，与 T0 修复逻辑相互印证。

## 留言
- [nagilix 2026-10-07] T6 负责的所有非代码章节、拓展题与反推提示词均已就绪。图片已按规范存放于 report/images/T6-qemu-run.png。
- [openfar 2026-10-07] 审核后集成进 report.md，状态改为完成。集成时做了以下修正：① T6-purpose 原文重复粘贴了两遍，并写到 lab1 没有涉及的 LMA/VMA 重定位、高位内核空间和 sstatus/stvec/scause/sepc，已按 lab1 的实际内容重写为 4 条；② T6-env 中张远和李云鹏的 AI 工具与实际不符，已更正，软件环境改为三人各自的版本；③ 各 section 去掉了自带的一级标题，统一为模板结构，截图补了图题；④ 提示词 2 的 [RELY] 中三个函数签名与源码不符，已按源码更正。拓展题和总结内容保留。
