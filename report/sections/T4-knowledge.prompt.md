# T4 最终提示词：实验与 OS 原理的知识点对照

[PROMPT]
任务：根据当前 lab1 与 T1/T2/T3 分析，完成 report/sections/T4-knowledge.md，对应报告模板「六、实验总结与收获／对操作系统的理解」。
操作要求：直接编写真实 section 文件，保留其他成员内容和实验代码，不编辑最终 report.md。
输出要求：以简体中文列出本实验的重要知识点及其原理对应，分别解释含义、联系、差异；另列重要但未覆盖的 OS 知识点，并用实际代码说明边界。

[RELY]
原样摘自 code/kern/init/init.c：
```c
int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    const char *message = "(THU.CST) os is loading ...\n";
    cprintf("%s\n\n", message);
   while (1)
        ;
}
```
原样摘自 code/kern/driver/console.c：
```c
void cons_init(void) {}
```

[GUARANTEE]
交付知识点对照表，覆盖引导、特权级与陷入、栈和 ABI、段布局和 BSS、编译链接装载、SBI 与设备抽象；列出分页/物理页管理、进程调度、用户系统调用、内核中断处理、同步、文件系统及完整设备管理等未覆盖内容。

[SPECIFICATION]
## 知识点映射
Pre-Condition：已阅读当前源码、任务要求、T1/T2/T3 及参考资料，并核对指导书与实测的差异。
Post-Condition：每项明确给出实验事实、OS 概念与二者关系；启动栈不等于进程栈切换，SBI 不等于用户系统调用，段划分不等于页表保护，忙循环不等于调度或休眠。
Case 1：说明异常机制时，承认 ecall 已触发固件陷阱处理，只说未实现内核自己的中断/异常处理，不声称实验完全没有异常。
Case 2：讨论页与对齐时，区分大小常量和实际页表/分配器；不从头文件名推断功能已经实现。
Requirements：仅依据真实代码判断范围，不抄写参考报告，不编造原理课讲授进度或人工学习体验。
