# T1 最终提示词：入口汇编与启动栈分析

[PROMPT]
任务：分析 code/kern/init/entry.S 的两个入口伪指令及其与 kern_init 的关系，完成练习1。
操作要求：直接撰写真实文件 report/sections/T1-ex1-entry.md，保留现有实验代码，不修改其他成员文件。
输出要求：围绕源码和单步观察回答问题，只使用 lyp 自己实测产生的图片并解释寄存器变化，不引用 T2 图片。区分伪指令、链接松弛后的机器指令与编译期栈空间预留，协作记录不写进正文。

[RELY]
以下内容分别原样摘自 entry.S、mmu.h、memlayout.h、init.c（entry.S 与 init.c 省略了注释行和行尾注释）：
```asm
kern_entry:
    la sp, bootstacktop

    tail kern_init
```
```c
#define PGSIZE          4096                    // bytes mapped by a page
#define PGSHIFT         12                      // log2(PGSIZE)
#define KSTACKPAGE          2                           // # of pages in kernel stack
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // sizeof kernel stack
int kern_init(void) __attribute__((noreturn));
```
```c
    extern char edata[], end[];
    memset(edata, 0, end - edata);
```

[GUARANTEE]
交付练习1章节：回答 la 加载什么、为什么先建立栈，tail 怎样转移控制权、是否保存返回地址；给出真实反汇编、8192 字节栈范围与对齐、.data 与 BSS 清零关系、tail/call/noreturn 的区别、验证命令及真实迭代说明。

[SPECIFICATION]
## kern_entry 与 kern_init 的分析
Pre-Condition：已阅读当前 lab1 源码、任务要求和最新指导书；从已提交代码副本构建 ELF，记录本次工具链版本。
Post-Condition：报告中每个具体地址与本次 nm/objdump/GDB 输出一致；说明 la 加载符号地址而非内存内容；跳转不会自动清理栈，noreturn 是编译器约定，实际不返回由函数循环保证。
Case 1：链接松弛改变 tail 的展开形式时，同时说明通用的 auipc/jalr 语义与最终 ELF 的指令，不写成固定展开。
Case 2：edata 与 end 相等时，说明本次清零长度为零，不声称已清除非空 BSS。
Requirements：不得编造截图或人工答辩经历；运行时需要先建立有效栈再进入需要栈的 C 函数，但不要断言所有内核入口的第一条机器指令都必须设置 sp。
