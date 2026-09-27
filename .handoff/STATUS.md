# Lab1 状态    截止：约 2026-10-13（Q&A 答复，以群通知为准）    集成负责人：openfar

## 目标
回答练习 1（entry.S）和练习 2（GDB 跟踪启动流程），写出本章的逻辑主线、核心模块理解和知识点对照，并附上 make qemu 的运行截图。lab1 不需要写代码，也没有 make grade（框架里没有 tools/grade.sh）。

## 任务看板
| ID | 任务 | 负责人 | 状态 | 可改文件 |
|----|------|--------|------|----------|
| T0 | 环境兼容：让 make qemu / make debug 在各版本 QEMU 下都能启动内核 | openfar | 待开始 | code/Makefile |
| T1 | 练习1：entry.S 中 la sp / tail kern_init 的分析 | lyp | 待开始 | report/sections/T1-*.md |
| T2 | 练习2：用 GDB 跟踪 0x1000 → OpenSBI → 0x80200000 | openfar | 待开始 | report/sections/T2-*.md, report/images/T2-* |
| T3 | 核心模块理解：链接脚本与内存布局、SBI→cprintf 输出链、构建流程 | lyp | 待开始 | report/sections/T3-*.md |
| T4 | 知识点对照：实验与 OS 原理的对应，以及原理中本实验未覆盖的知识点 | lyp | 待开始 | report/sections/T4-*.md |
| T5 | 整体逻辑主线 + 集成（合并 report.md 和 prompt.md） | openfar | 待开始 | report/sections/T5-*.md, report/report.md, report/prompt.md |
| T6 | 实验目的、实验环境表、make qemu 运行截图、拓展（现代笔记本启动流程）、实验总结 | nagilix | 待开始 | report/sections/T6-*.md, report/images/T6-* |

## 集成状态
- make qemu ⬜   make grade 不适用   report.md 已合并 ⬜   prompt.md 已合并 ⬜

## 全组须知
1. **QEMU 版本差异**：每人的环境不同，先运行 `qemu-system-riscv64 --version` 查看版本。课程推荐的 4.1.x 用原 Makefile 就能跑。较新的版本（在 8.2 上确认过）自带的 OpenSBI 会显示 `Domain0 Next Address : 0x0`，内核不会运行，不输出 `(THU.CST) os is loading ...`。T0 完成之前，遇到这种情况可以在 code/ 下手动运行：`qemu-system-riscv64 -machine virt -nographic -bios default -kernel bin/kernel`。
2. **指导书不可全信**：练习 2 的提示说“OpenSBI 把内核加载到 0x80200000，可用 watch 观察加载瞬间”，至少在新版 QEMU 上是错的：GDB 停在 0x1000 时，0x80200000 处已经是 kern_entry 的指令。另外，lab1 页面里的文件树与实际代码不符，示例输出的 OpenSBI 版本也因环境而异。凡是引用指导书的结论，都要先对照代码或实测确认。
3. 2026 版 lab1 框架代码与 2025 版完全相同，往届资料可以参考，但报告必须自己写。
4. **Q&A 答复**：lab1 只要求 make qemu 能运行；报告由小组共同完成，放在 git 仓库里；提示词按模板写，只提交迭代优化后的最终版，不要交聊天记录导出。
5. **prompt.md 的具体写法还没定**。在此之前，所有提示词都如实、完整地记在各自任务文件的「提示词记录」里。
