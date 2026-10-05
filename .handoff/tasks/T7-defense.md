# T7 答辩准备
- 负责人：全员（openfar / lyp / nagilix）    状态：待开始    依赖：T1–T6 完成后进入复习
- 可改文件：本文件中「各人准备情况」下**自己的小节**

## 要求
- 往届每个 lab 都有答辩，助教会当面提问（往届问题记录见 `reference/lhz/Lab1/report/Lab0-Lab1答辩准备/os答辩.md`，注意那一届的 lab1 对应的是我们的 lab3 中断部分，只有一部分问题适用）。
- **每个人都要能回答下面的全部问题**，而不只是自己负责的部分。答辩时可能问到任何人，自己负责的部分要能讲到细节。
- 准备方式：先读完各 section（report/sections/），再对照下面的问题清单，自己用一两句话口述答案，答不上来的就去看对应 section 或问负责人。
- **前辈笔记有错，不能照搬**：例如他写“格式化输出在 M 态”，实际上格式化（cprintf → vprintfmt）在 S 态完成，只有逐字符输出那一步经 ecall 进入 M 态；又如他写“tail 会清理栈，假定被调函数会返回”，实际上 tail 是不保存返回地址的跳转，正是因为 kern_init 不会返回。

## 问题清单（括号内为参考出处）

**启动流程（T2）**
1. 加电后第一条指令在哪个地址？0x1000 处的几条指令做了什么？a0/a1/a2 分别是什么？
2. OpenSBI 是什么？运行在哪个特权级？它在启动时做了哪些事？
3. OpenSBI 怎样把控制权交给内核？（mret、mepc、mstatus.MPP）进入内核后特权级是什么？
4. 内核是谁、在什么时候放到 0x80200000 的？为什么 `watch *0x80200000` 不会触发？真机上又是谁加载内核？
5. M / S / U 三个特权级有什么区别？内核为什么不直接运行在 M 态？

**入口与栈（T1）**
6. `la sp, bootstacktop` 做了什么，为什么必须是第一条指令？（提示：进入内核时 sp 还指向 OpenSBI 的栈，这块区域被 PMP 禁止 S 态访问）
7. 内核栈有多大，栈底和栈顶在哪里？栈向哪个方向增长？`bootstack` 为什么放在 .data 段而不是 .bss 段？
8. `tail kern_init` 和 `call kern_init` 有什么区别？和 `noreturn` 有什么关系？
9. `kern_init` 中 `memset(edata, 0, end - edata)` 的作用是什么？edata、end 是在哪里定义的？

**链接与构建（T3）**
10. 为什么内核入口在 0x80200000？链接脚本中 `BASE_ADDRESS` 和 `ENTRY` 各起什么作用？怎样保证 kern_entry 位于最前面？
11. `make qemu` 做了哪些事？`bin/kernel` 和 `bin/ucore.img` 有什么区别？objcopy 起什么作用？
12. 我们为什么修改了 Makefile（T0）？新版 QEMU 和 4.1 的启动方式有什么不同？

**输出与 SBI（T2、T3）**
13. `cprintf` 的完整调用链是什么？格式化在哪个特权级完成，字符最终由谁输出？
14. `ecall` 之后 CPU 发生了什么？（mcause = 9、mtvec、mepc）为什么 S 态的 ecall 没有被委托给 S 态？
15. 为什么内核不能直接用 C 标准库的 printf？

**原理对照（T4）**
16. 本实验中的知识点与 OS 原理课中的哪些概念对应？有什么差异？
17. 原理课里哪些重要内容在本实验中没有体现？

**工具（全员）**
18. GDB 常用命令：`si` / `ni` / `b` / `watch` / `x` / `info registers` 各做什么？`make debug` 中 `-s -S` 的含义是什么？

## 各人准备情况
### openfar

### lyp

### nagilix

## 留言
