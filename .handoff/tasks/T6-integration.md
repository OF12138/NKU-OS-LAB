# T6 集成
- 负责人：openfar    DDL：11-22    状态：完成    依赖：T0–T5
- 主要文件：report/report.md, report/prompt.md

## 要求
- 审核各 section，按 lab4 的 section 名调整 `merge-report.py` 的 `init()`，合并 report.md 与 prompt.md（三个功能模块各一份提示词，另含分析类提示词）。
- 最终在统一环境下 `make grade` 30/30，`make qemu` 正常。
- 检查 `YOUR CODE` 已替换为学号、lab2/lab3/LAB4 标记注释保留，`check-rely.py` 通过。

## 当前进度 / 下一步
- 11-20 完成：report.md、prompt.md 已合并。之后成员再改 section，用 `merge-report.py --replace` 换入对应段落（先确认 report.md 没有未提交修改）。

## 关键决策与结论
- `merge-report.py` 的 `init()` 已按 lab4 调整：一/二/五/AI 经验取自 T5-common，三取 T4-logic（自带标题），四依次为 T1、T2、T3、T5-challenge，六为 T4-knowledge + AI 经验。全文 4 张图（图 1～4）。
- prompt.md 收录 T1、T2、T3 三个功能模块和 T5 Challenge 共 4 份提示词；分析类 section 不需要提示词。
- 集成时改动的 section：T1 迭代过程末句改为最终结果（30/30）；T5-common 删掉写给集成人的“如有变更可在集成前核对”。

## 验证结果
- 11-20，提交后的 HEAD 经 sync-wsl 导出：`make` 通过，`make qemu` 输出 initproc 三行（随后为预期的 process exit panic），`make grade` 30/30。
- 代码中无残留 `YOUR CODE`；lab2/lab3/LAB4 标记均保留并替换为学号；`check-rely.py` 全部通过。

## 迭代素材

## 留言
