# T3 知识点与 OS 原理对应：最终提示词

[PROMPT]
任务：为 Lab3 报告撰写“对操作系统的理解”，保存到 `report/sections/T3-knowledge.md`，负责人为 2414099-李云鹏。
操作要求：结合本仓库真实代码进行分析，不修改代码或最终汇总报告，不替代其他成员负责的整体逻辑分析。参考 `reference/OS_lab2_report.pdf` 的原理与代码对应写法，但不复用其表述。
输出要求：按报告模板的两个问题组织，使用简体中文；每个重要知识点说明含义、与代码的关系及抽象原理和本实验实现的差异。准确区分“实现了什么”和“验证了什么”。

[RELY]
以下为当前代码中可核对的最小上下文，省略与分析无关的行和注释。

`code/kern/trap/trap.h`：
```c
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};
```

`code/kern/trap/trap.c`：
```c
bool trap_in_kernel(struct trapframe *tf) {
    return (tf->status & SSTATUS_SPP) != 0;
}
```

`code/kern/driver/clock.c`：
```c
volatile size_t ticks;
static uint64_t timebase = 100000;
void clock_set_next_event(void) { sbi_set_timer(get_cycles() + timebase); }
```

`code/kern/trap/trapentry.S` 中返回路径的关键指令：
```asm
    LOAD s1, 32*REGBYTES(sp)
    LOAD s2, 33*REGBYTES(sp)
    csrw sstatus, s1
    csrw sepc, s2
```

```asm
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret
```

补充事实：`SAVE_ALL` 分配 `36 * REGBYTES` 的帧；当前为 RV64，`REGBYTES=8`。`trap_dispatch()` 按 cause 最高位区分中断与异常；T2 处理时钟、软件中断、断点和非法指令。`kern_init()` 初始化后进入无限循环，尚无进程调度或用户程序运行流程。

[GUARANTEE]
必须回答：
1. 本实验中哪些知识点重要？它们在 OS 原理中的含义是什么？与本实验的实现有什么联系和差异？
2. OS 原理中哪些重要机制在本实验没有对应实现？明确缺失的组件，不能只列名词。
必须交付：可独立合并的中文报告 section；不虚构运行结果或截图。

[SPECIFICATION]
1. 原理对应分析
   - Pre-Condition：以当前实验代码及已明确的任务范围为依据，不假定已有进程、用户态和完整设备驱动。
   - Post-Condition：至少覆盖中断与异常语义、软硬件上下文保存、特权级与 SBI、时钟机制、EPC 与指令长度、中断挂起和应答。
   - Case 1：区分 Trap 保存现场和切换到另一进程；保存寄存器不代表已实现调度。
   - Case 2：区分中断继续执行、断点/非法指令按实验策略跳过，以及缺页修复后可能重试；不声称所有异常都增加 EPC。
   - Case 3：明确当前只处理可读内核代码中的 16/32 位故障指令，未知异常诊断不等于成功恢复。
   - Case 4：解释 `volatile` 不提供原子性；分类打印外部中断不等于完成中断控制器和设备应答。
2. 未覆盖机制
   - Pre-Condition：把“尚未实现”与“此前实验已有基础”区分开。
   - Post-Condition：说明进程线程与调度、系统调用及用户栈切换、按需分页、同步互斥与多核、完整 I/O、文件系统、故障隔离的作用及本实验缺项。
   - Requirements：不过度扩展成其他实验的实现教程，不改写 openfar 负责的整体流程分析，不将静态分析或编译通过表述为端到端运行成功。
