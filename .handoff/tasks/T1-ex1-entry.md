# T1 练习1：理解内核启动中的程序入口操作
- 负责人：lyp    状态：待开始    依赖：无
- 可改文件：report/sections/T1-*.md

## 要求
- 指导书原题：阅读 kern/init/entry.S，结合内核启动流程，说明 `la sp, bootstacktop` 完成了什么操作、目的是什么；`tail kern_init` 完成了什么操作、目的是什么。
- 答案要以实际代码和反汇编为准（`riscv64-unknown-elf-objdump -d bin/kernel`），说明伪指令展开后的真实指令。
- 可以展开的方向：栈的大小和对齐方式（memlayout.h / mmu.h）；为什么 bootstack 放在 .data 段而不是 .bss 段（提示：kern_init 一开始就把 edata 到 end 这段内存清零）；tail 和 call 的区别，以及它和 kern_init 的 noreturn 属性有什么关系。

## 当前进度 / 下一步

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
