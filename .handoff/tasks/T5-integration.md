# T5 集成
- 负责人：openfar    DDL：11-07    状态：完成    依赖：T0–T4
- 主要文件：report/report.md, report/prompt.md

## 要求
- 审核各 section，按 lab3 的 section 名调整 `merge-report.py` 的 `init()`，合并 report.md 与 prompt.md（2 份提示词）。
- 最终在统一环境下 `make grade` 100/100，`make qemu` 正常。
- 检查 `YOUR CODE` 已替换为学号、lab2/lab3 标记注释保留，`check-rely.py` 通过。

## 当前进度 / 下一步
- [11-05] 本地注释提交后 rebase 合入 lyp 的 trap.c（自动合并，无冲突）；`merge-report.py` 的 `init()` 已改为 lab3 结构，生成 report.md（图 11 张）与 prompt.md（5 份提示词：T1、T2、T3 知识点、T3 整体逻辑、T4 思考题）。
- 之后成员若再改 section，用 `merge-report.py --replace` 替换对应段落；report.md 由用户修改期间不要运行写它的脚本。

## 关键决策与结论
- `merge-report.py`：`tidy()` 新增把 `<p align="center">` 包裹的单张图片拆成独立行（否则 GitHub 不渲染其中的 Markdown 图片）；`part()` 去掉末尾 `---`；`include_prompts()` 同时去掉提示词文件开头的一级标题。

## 验证结果
- 合并后的 HEAD（WSL，gcc 15.1.0，QEMU 4.1.1）：`make` 通过；`make qemu` 十次 `100 ticks` 后关机；`make grade` 35/35、25/25、40/40，Total 100/100（截图 `T4-qemu-run.png`、`T4-make-grade.png`）。
- `YOUR CODE` 已替换：trapentry.S 两处 2411264，trap.c 2414099；`check-rely.py` 全部通过。

## 迭代素材

## 留言
