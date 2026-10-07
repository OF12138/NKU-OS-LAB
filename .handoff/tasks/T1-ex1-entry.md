# T1 练习1：理解内核启动中的程序入口操作
- 负责人：lyp    状态：进行中    依赖：无
- 可改文件：report/sections/T1-*.md

## 要求
- 指导书原题：阅读 kern/init/entry.S，结合内核启动流程，说明 `la sp, bootstacktop` 完成了什么操作、目的是什么；`tail kern_init` 完成了什么操作、目的是什么。
- 答案要以实际代码和反汇编为准（`riscv64-unknown-elf-objdump -d bin/kernel`），说明伪指令展开后的真实指令。
- 可以展开的方向：栈的大小和对齐方式（memlayout.h / mmu.h）；为什么 bootstack 放在 .data 段而不是 .bss 段（提示：kern_init 一开始就把 edata 到 end 这段内存清零）；tail 和 call 的区别，以及它和 kern_init 的 noreturn 属性有什么关系。

## 当前进度 / 下一步

- 已核对在线练习题、源码、前辈参考报告；从提交 7b0948d 导出独立副本并构建。
- nm/objdump/GDB 已验证栈范围、la 展开、tail 的压缩跳转及 ra 不变；已整理 T1 最终提示词。
- 下一步：写完整练习1章节，复核术语与实测记录后交 openfar 审核。

## 关键决策与结论

## 验证结果

- 2026-10-07：WSL Ubuntu-22.04-OS，SiFive GCC 10.2.0，make -j2 成功；objdump、nm、readelf 完成。GDB 批处理退出 0；make qemu 输出预期信息，8 秒后 timeout 以 124 停止持续运行的内核。

## 迭代素材

- 最终 tail 并非固定 auipc/jalr → 链接松弛生成 c.j → 同时解释目标文件重定位与最终机器码。
- 本次 edata=end=0x80203008 → 无存活 BSS 内容 → 区分清零语义和本次零长度调用。

## 留言
