# T1 练习1：理解内核启动中的程序入口操作
- 负责人：lyp    状态：待审    依赖：无
- 可改文件：report/sections/T1-*.md

## 要求
- 指导书原题：阅读 kern/init/entry.S，结合内核启动流程，说明 `la sp, bootstacktop` 完成了什么操作、目的是什么；`tail kern_init` 完成了什么操作、目的是什么。
- 答案要以实际代码和反汇编为准（`riscv64-unknown-elf-objdump -d bin/kernel`），说明伪指令展开后的真实指令。
- 可以展开的方向：栈的大小和对齐方式（memlayout.h / mmu.h）；为什么 bootstack 放在 .data 段而不是 .bss 段（提示：kern_init 一开始就把 edata 到 end 这段内存清零）；tail 和 call 的区别，以及它和 kern_init 的 noreturn 属性有什么关系。

## 当前进度 / 下一步

- 已按用户要求撤销 T2 图片复用，保留重写后的正文与最终提示词。独立运行 GDB 并生成 report/images/T1-entry-step.jpg，已配图解释 sp 和 ra。
- 状态：待审。下一步由 openfar 审核集成，统一图号；用户要求 lyp 独立补图，本轮新增图片由 lyp 维护。

## 关键决策与结论

- la 加载地址，最终 auipc sp,0x3 + addi sp,sp,0；tail 在 entry.o 为 auipc/jalr x0，最终松弛为 c.j，跳转前后 ra 不变。
- 栈范围 [0x80201000,0x80203000)，8192 字节；本次 edata=end=0x80203008，清零长度为零。
- 地址取自 SiFive GCC 10.2.0 / QEMU 6.2.0 / OpenSBI v0.9，固件地址不与 T2 的 8.2.2 产物混用。

## 验证结果

- 本轮独立验证：GDB 两次批处理均退出0；入口 sp/ra 与输出调用的 priv/mcause/mepc/mtvec 已截图核对。readelf/nm 输出与正文地址一致。图片来自真实命令输出的浏览器展示页，不是交互式终端截图。T2 文件未修改。

- 本轮文档复核：已打开检查三张原始截图，逐图核对正文；图片路径、图题、代码围栏、[RELY] 原样摘录及 T3 内嵌提示词一致性通过。只改文档，无需重跑内核。

- 2026-10-07：WSL Ubuntu-22.04-OS，SiFive GCC 10.2.0，make -j2 成功；objdump、nm、readelf 完成。GDB 批处理退出 0；make qemu 输出预期信息，8 秒后 timeout 以 124 停止持续运行的内核。

## 迭代素材

- 用户明确要求不复用 T2 图片 → 已删除全部复用引用和对应解读，改为独立运行 lyp 本机实验并采集输出展示页截图。

- 首版只写单步结果，没有引用现成截图 → 将采集分工误当成章节无需配图 → 补入入口换栈实测截图、图题和逐图分析，删除审计式声明。

- 最终 tail 并非固定 auipc/jalr → 链接松弛生成 c.j → 同时解释目标文件重定位与最终机器码。
- 本次 edata=end=0x80203008 → 无存活 BSS 内容 → 区分清零语义和本次零长度调用。

- 2026-10-07 文档检查：section 本地链接、代码围栏、最终提示词完整性及 [RELY] 源码逐行一致检查通过；git diff --check 通过。

## 留言
