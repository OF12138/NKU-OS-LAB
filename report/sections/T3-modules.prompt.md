# T3 最终提示词

## 1. 链接脚本与内存布局

[PROMPT]
任务：分析 code/tools/kernel.ld 与入口目标文件顺序，撰写 report/sections/T3-modules.md 的链接与内存布局模块。
操作要求：直接编辑真实报告文件，保留实验代码及其他成员章节。
输出要求：结合 nm、readelf 与 make print-kobjs，解释入口、段顺序、边界符号和装载地址；明确链接器与装载器的不同职责。

[RELY]
原样摘自 code/tools/kernel.ld：
```ld
OUTPUT_ARCH(riscv)
ENTRY(kern_entry)

BASE_ADDRESS = 0x80200000;
```
```ld
    .text : {
        *(.text.kern_entry .text .stub .text.* .gnu.linkonce.t.*)
    }
```
```ld
    PROVIDE(edata = .);
```
```ld
    PROVIDE(end = .);
```

[GUARANTEE]
交付段布局表、etext/edata/end 的含义、ENTRY 与 BASE_ADDRESS 的区别、kern_entry 位于最前面的实际原因，以及验证命令和本次结果。

[SPECIFICATION]
## 链接布局分析
Pre-Condition：读取完整 kernel.ld、entry.S、Makefile 和 function.mk，已生成实际 ELF。
Post-Condition：说明当前 entry.S 使用普通 .text，entry.o 为链接输入的第一个目标文件；ENTRY 设置 ELF 入口，不自动重新排列代码。表中地址和边界与实际构建一致。
Case 1：PROVIDE 符号没有被引用、nm 未输出 etext 时，解释它仍表示脚本中 .text 末尾的位置，不补造符号表记录。
Requirements：不把段标志当成已经开启的页表权限；不把页对齐当成分页管理已实现；不修改链接脚本。

## 2. 格式化输出与 SBI 调用

[PROMPT]
任务：分析内核从 cprintf 到 ecall 的输出功能，撰写 T3 报告的输出模块。
操作要求：直接写入真实报告文件，不修改代码；按源码顺序说明每层职责。
输出要求：列出完整调用链、主要接口、格式化与实际设备输出所在特权级、字符参数和 SBI 调用号，以及源码与反汇编之间的区别。

[RELY]
以下声明原样摘自 code/libs/stdio.h、code/kern/driver/console.h、code/libs/sbi.h：
```c
int cprintf(const char *fmt, ...);
int vcprintf(const char *fmt, va_list ap);
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);
void cons_putc(int c);
void sbi_console_putchar(unsigned char ch);
```
以下函数原样摘自 code/libs/sbi.c：
```c
void sbi_console_putchar(unsigned char ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
}
```

[GUARANTEE]
交付 cprintf → vcprintf → vprintfmt → cputch → cons_putc → sbi_console_putchar → sbi_call → ecall 的源码调用关系、参数寄存器与陷入返回解释，以及不能直接使用宿主 printf 的原因。

[SPECIFICATION]
## 输出功能分析
Pre-Condition：读取 stdio.c、printfmt.c、console.c、sbi.c，结合 T2 的固件调试记录和本次反汇编。
Post-Condition：解释变参、回调与计数，在 S 态完成格式化，在当前固件配置下通过 S 态 ecall 进入 M 态；a7=1，a0 为字符；OpenSBI 最终输出，返回后继续内核执行。
Case 1：编译优化内联 vcprintf 或 sbi_call 时，保留源码逻辑链并标明 ELF 中不一定有独立函数符号。
Case 2：讨论 ecall 时，区分 S→M 的 SBI 调用与 U→S 的用户系统调用；不写成任意 ecall 必然进入 M 态。
Requirements：只说明本框架实际支持和使用的旧式 SBI console_putchar；不声称输出经过内核缓冲队列或输出计数就是硬件成功确认。

## 3. 构建与加载流程

[PROMPT]
任务：分析 Makefile/function.mk 从源文件到 ELF 和裸镜像的流程，补全 T3 构建模块及验证记录。
操作要求：直接修改真实报告文件，保留现有代码；在独立干净副本中编译运行。
输出要求：以当前 make 规则和 readelf 结果为准说明产物区别，结合 T0 解释当前 QEMU 加载方式。

[RELY]
原样摘自 code/Makefile：
```make
KOBJS	= $(call read_packet,kernel libs)
```
```make
$(UCOREIMG): $(kernel)
	$(OBJCOPY) $(kernel) --strip-all -O binary $@
```
```make
LDFLAGS	+= -nostdlib --gc-sections
```

[GUARANTEE]
交付 .c/.S→.o→bin/kernel→bin/ucore.img 的流程、ELF 与纯二进制区别、make qemu/debug/gdb 的含义及实测运行结论。

[SPECIFICATION]
## 构建和装载分析
Pre-Condition：已读取完整构建规则，并在已提交源码副本执行 make、readelf、make qemu。
Post-Condition：说明 .S 先预处理、目标文件重定位后形成 ELF；objcopy 生成不含 ELF 入口/符号表的裸镜像；当前 make qemu 仍依赖生成镜像，但实际 -kernel 加载 bin/kernel。
Case 1：看到旧式 loader 相关注释时，说明这是历史方案，不当作当前执行命令。
Case 2：make qemu 因 kern_init 的无限循环被 timeout 停止时，记录输出与预期退出码，不把超时误判为启动失败或测试全面通过。
Requirements：说明 lab1 缺少 tools/grade.sh，make grade 不适用；不得声称在本机验证了未运行的 QEMU 版本。
