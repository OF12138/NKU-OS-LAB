# T9 集成
- 负责人：openfar    DDL：10-25    状态：待开始    依赖：T1–T8
- 主要文件：report/report.md, report/prompt.md

## 要求
- 把各 section 合并进 report.md，各提示词合并进 prompt.md；功能模块的最终提示词放进报告。
- 统一环境下最终 `make grade`；检查所有 YOUR CODE 已替换为学号、lab2 注释保留。

## 当前进度 / 下一步
- 10-25 合并，10-26 截止前留一天给全组审阅。

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
- [lyp 2026-10-22] T5 的 TLB 部分已完成：`report/sections/T5-tlb.md`、`T5-tlb.prompt.md` 和 5 张真实 tmux/GDB 输出图，四组实验实测，默认评分 20/20。可用既有 `merge-report.py --fill` 合并；复现见 `temp/T5-reproduce.md`。本机 GCC 10.2 的 kern_init/sd 地址比 T5-walk 多 2 字节，正文已说明。另请审核 T5-walk 中“未 fence 可沿用旧翻译”的适用范围：当前规范要求 Bare 与其他模式切换立即生效，不能把 QEMU 4.1.1 的该现象直接视为硬件规范允许的行为。
