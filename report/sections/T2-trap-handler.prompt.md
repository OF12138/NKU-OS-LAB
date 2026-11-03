# T2 功能模块二：中断与异常处理最终提示词

[PROMPT]
任务：实现 Lab3 `code/kern/trap/trap.c` 的中断与异常处理模块。
操作要求：直接修改真实文件，仅在 `//lab3 function2 YOUR CODE` 区域补充必要头文件、常量及两个处理函数，保留 `lab3 function2` 标记并把 `YOUR CODE` 替换为 `2414099`。不改变已有函数、函数签名、初始化流程、汇编入口或评测文件。
输出要求：提供实现、实际编译与运行结果；说明断点/非法指令的返回地址更新和时钟自动关机边界。未运行的验证明确标为未验证。

[RELY]
以下是导入框架提交 `8cc0679` 中已有的接口与定义。宏仅节选本功能所需内容；省略无关代码，不把待实现的函数当作现成依赖。

`kern/trap/trap.h`：
```c
struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};

void print_trapframe(struct trapframe *tf);
```
其中 `struct pushregs` 是框架中按寄存器编号排列、含全部 32 个通用寄存器槽位的结构，不改变其定义。

`libs/riscv.h`：
```c
#define IRQ_U_SOFT   0
#define IRQ_S_SOFT   1
#define IRQ_H_SOFT   2
#define IRQ_M_SOFT   3
#define IRQ_U_TIMER  4
#define IRQ_S_TIMER  5
#define IRQ_H_TIMER  6
#define IRQ_M_TIMER  7
#define IRQ_U_EXT    8
#define IRQ_S_EXT    9
#define IRQ_H_EXT    10
#define IRQ_M_EXT    11
#define IRQ_COP      12
#define IRQ_HOST     13
#define CAUSE_ILLEGAL_INSTRUCTION 0x2
#define CAUSE_BREAKPOINT 0x3
#define SIP_SSIP MIP_SSIP
```
还可调用框架的 `clear_csr(reg, bit)` 宏清除 CSR 中相应位。

`kern/driver/clock.h`：
```c
extern volatile size_t ticks;

void clock_init(void);
void clock_set_next_event(void);
```

`libs/sbi.h`：
```c
void sbi_shutdown(void);
```

`kern/trap/trap.c`：
```c
#define TICK_NUM 100

static void print_ticks() {
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}
```
已有 `trap_dispatch()` 依据 `(intptr_t)tf->cause < 0` 区分中断和异常；异常返回路径会把 `tf->epc` 写回 `sepc`，再执行 `sret`。不要直接写 `sepc` 来代替修改帧。

[GUARANTEE]
必须实现接口：
```c
void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);
```
必须交付：两个接口的实现；与实际结果一致的验证记录；在统一环境中以 GDB 观察断点及非法指令的原始 EPC、编码、更新后的 EPC 和 `sret` 后的 PC，并保存真实取证图片。

[SPECIFICATION]
1. `interrupt_handler(struct trapframe *tf)`
   - Pre-Condition：`tf` 指向有效 Trap 帧；实际执行环境为 RV64 S 态。真实中断的 `cause` 最高位置位，测试也可能直接传入不带该位的中断编号。
   - Post-Condition：清除中断标志位后按编号分派，不改变 `tf->epc`。管理态时钟中断先调用 `clock_set_next_event()`，再令 `ticks` 加一；每达 `TICK_NUM` 整数倍调用 `print_ticks()`，累计输出 `SHUTDOWN_AFTER=10` 次后调用 `sbi_shutdown()`。不清零或取模覆盖 `ticks`。
   - Case 1：管理态软件中断清除 `sip.SSIP`，输出 `Supervisor software interrupt`，防止 pending 位一直保留造成重复触发。
   - Case 2：其余框架已定义的中断编号输出对应类型的提示；这里只作分类诊断，不宣称已实现完整设备驱动。
   - Case 3：未知编号先输出 `Unknown interrupt: cause <编号>`，再调用 `print_trapframe(tf)`。直接传入编号 15 时也必须正确输出 15。
   - Requirements：关机次数必须大于 2，让等待 `ticks >= 200` 的评测主流程能在中断返回后继续执行；不额外定义 `DEBUG_GRADE`，避免第一次打印即 panic。

2. `exception_handler(struct trapframe *tf)`
   - Pre-Condition：`tf` 指向有效异常帧；对断点及非法指令，本实验保证故障指令地址可按半字读取，待跳过的指令为 16 位或 32 位。不将此假设推广到任意用户地址、取指故障或长指令扩展。
   - Post-Condition：非法指令输出 `Illegal instruction at` 和原始 EPC，断点输出 `Breakpoint at` 和原始 EPC；随后读取 EPC 处的 `uint16_t` 首半字，低两位为 `11` 时推进 4 字节，否则推进 2 字节。保留帧中其他字段。
   - Case 1：`c.ebreak` 编码 `0x9002`，返回地址增加 2。
   - Case 2：32 位 `ebreak` 编码 `0x00100073`，返回地址增加 4；不能把断点一律当作压缩指令。
   - Case 3：`.word 0x0000007b` 触发非法指令，返回地址增加 4。
   - Case 4：其他异常调用 `print_trapframe(tf)`，不擅自修改 EPC。诊断输出不等同于已恢复未知异常。
   - Requirements：从实际指令编码判断长度，不按异常类型硬编码，不依赖 `badvaddr` 一定包含指令编码；先读半字，避免在仅 2 字节对齐地址进行 32 位读取。

3. 验证要求
   - 保留原评测与初始化锚点；在隔离副本中验证，明确测试对象为提交版基线叠加当前 `trap.c`。有多个 WSL 发行版时，先用 `wsl --list --verbose` 核对，并选择安装 QEMU 4.1.1 的发行版；本机为 `Ubuntu-22.04-OS`，工具链在 `/opt/riscv/bin`。
   - 编译采用原选项；日常运行应周期打印 `100 ticks` 并在第十次打印后关机。统一 QEMU 4.1.1 下的原始 `make grade` 目标为 100/100。
   - GDB 使用硬件断点观察真实异常，不修改 EPC 来制造成功返回；核对 `sret` 后 PC 等于原始 EPC 加实际长度。没有运行证据时，不生成或引用伪造截图。
   - 补测软件中断时区分“直接调用处理函数”和“CPU 递交真实中断”：真实触发需要局部 `sie.SSIE`、全局 `sstatus.SIE` 及挂起 `sip.SSIP` 的条件均满足；核对处理后 SSIP 清零，且实际返回 PC 等于未推进的原始 EPC。调试期的触发与临时指令恢复仅作用于专用来宾实例，不修改仓库或评测源码。
