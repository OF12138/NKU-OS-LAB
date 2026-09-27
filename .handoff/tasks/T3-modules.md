# T3 核心模块理解
- 负责人：lyp    状态：待开始    依赖：无
- 可改文件：report/sections/T3-*.md

## 要求
- 对应报告要求：说明自己对每个功能的核心函数或功能模块的理解。
- 覆盖以下三块：① tools/kernel.ld 与内存布局（入口地址、各段顺序、edata/end 等符号，以及为什么 kern_entry 恰好位于 0x80200000，可以用 nm 验证）；② 输出链：cprintf → vprintfmt → cputch → cons_putc → sbi_console_putchar → ecall；③ 构建流程：从 Makefile 到 bin/kernel（ELF）再到 bin/ucore.img（objcopy 生成的纯二进制），说明两者的区别。

## 当前进度 / 下一步

## 关键决策与结论

## 验证结果

## 提示词记录

## 留言
