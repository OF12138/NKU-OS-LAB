## 功能模块：链接脚本与内核内存布局

**负责人：** 2414099－李云鹏（lyp）

### 模块功能描述

`kernel.ld` 把各目标文件的代码和数据组织成内核 ELF，确定入口及各段的地址。本实验分析已有配置，不需要新增函数。

```ld
OUTPUT_ARCH(riscv)
ENTRY(kern_entry)

BASE_ADDRESS = 0x80200000;
```

`ENTRY(kern_entry)` 指定 ELF 的入口字段，`. = BASE_ADDRESS` 则让接下来的 `.text` 从 `0x80200000` 开始。二者不是同一件事：入口字段告诉装载器从哪里执行，位置计数器决定链接布局。

为什么 `kern_entry` 恰好在最前面？`entry.S` 使用的是普通 `.text`，而 `make print-kobjs` 输出的第一个文件是 `obj/kern/init/entry.o`，之后才是 `init.o`、`stdio.o` 等。链接器收集这些输入节时，先放入 `entry.o` 的入口代码，所以 `kern_entry` 的地址就是 `.text` 的起点。仅有 `ENTRY` 并不会把这个函数移到最前面。

脚本按 `.text → .rodata → 页对齐 → .data → .sdata → .bss` 排列。用 SiFive GCC 10.2.0 构建后，`readelf -S` 和 `nm` 的结果如下：

| 区域 | 地址范围（左闭右开） | 本次内容 |
|---|---|---|
| `.text` | `[0x80200000,0x802004c8)` | 入口和函数机器码 |
| `.rodata` | `[0x802004c8,0x80200738)` | 字符串和格式化所需的只读数据 |
| 对齐空隙 | `[0x80200738,0x80201000)` | 后续数据按4 KiB对齐 |
| `.data` | `[0x80201000,0x80203000)` | 8 KiB启动栈 |
| `.sdata` | `[0x80203000,0x80203008)` | `SBI_CONSOLE_PUTCHAR` |
| BSS清零区间 | `[0x80203008,0x80203008)` | 空区间，`edata=end` |

`bootstacktop` 和 `SBI_CONSOLE_PUTCHAR` 同为 `0x80203000`，看起来像栈与变量重叠，实际没有冲突：前者是栈区域结束的标签，栈不包含这一边界字节，后者从这里开始占用空间。

脚本还定义了三个边界：`etext` 在 `.text` 之后，`edata` 在 `.data/.sdata` 之后，`end` 在 `.bss` 之后。`kern_init` 用后两个地址确定清零范围。它们不是源码中的普通数组，`extern char edata[], end[]` 只是引用链接符号的写法。脚本使用 `PROVIDE`，本次未引用的 `etext` 没出现在 `nm` 的结果中。

`readelf -l` 中有两个 `LOAD` 段，分别覆盖 `.text/.rodata` 和 `.data/.sdata`，起始物理地址为 `0x80200000`、`0x80201000`。QEMU 按这些程序头装载内核。入口处读到 `satp=0`，本实验尚未启用分页，因此这里的段布局还不是进程的虚拟地址空间，也没有据此建立页表权限。

### 最终提示词

````markdown
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
````

### 分析迭代过程

检查入口位置时，发现 `entry.S` 没有定义单独的 `.text.kern_entry`，于是继续看实际链接输入顺序，确认 `entry.o` 排在首位。报告据此补充了 `ENTRY` 与段排序的区别。BSS的说明也按本次符号表调整为零长度清零。

## 功能模块：格式化输出与 SBI 服务

**负责人：** 2414099－李云鹏（lyp）

### 模块功能描述

`kern_init` 调用 `cprintf("%s\n\n", message)` 后，加载提示便出现在终端上。沿着代码往下看，这次输出经过以下路径：

```text
cprintf → vcprintf → vprintfmt
  → cputch → cons_putc → sbi_console_putchar → sbi_call
    → ecall → OpenSBI控制台服务 → UART
```

主要函数为：

```c
int cprintf(const char *fmt, ...);
int vcprintf(const char *fmt, va_list ap);
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);
static void cputch(int c, int *cnt);
void cons_putc(int c);
void sbi_console_putchar(unsigned char ch);
uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0, uint64_t arg1, uint64_t arg2);
```

`cprintf` 用 `va_start` 取得变参，交给 `vcprintf`。`vcprintf` 准备字符计数器，再把 `cputch` 和计数器地址交给 `vprintfmt`。后者解析 `%s` 等格式，每得到一个字符就调用一次回调。`cputch` 输出字符后增加计数，`cons_putc` 再将字符转成 `unsigned char`，交给 SBI。

这一层回调让格式化与输出位置分开了：`vprintfmt` 只管生成字符，换一个回调就可以把结果写入内存缓冲区，项目中的 `vsnprintf` 正是这样复用它的。当前 `cprintf` 的通路没有字符缓冲队列，也没有把设备错误反馈为返回值；返回的计数是回调处理的字符数。

`sbi_console_putchar` 选择旧式 SBI 的字符输出调用号1。`sbi_call` 用内联汇编把调用号放到 `a7`，字符放到 `a0`，其余参数放到 `a1/a2`，最后执行 `ecall`。

图 T3-1 显示的是小组练习2记录的一次字符输出。执行前 `priv=1`，仍在 S 态；`a7=1` 表示字符输出，`a0=0x28` 是加载提示的第一个字符 `(`。

![输出第一个字符时的ecall陷入和返回](../images/T2-4-ecall.png)

<center>图 T3-1　输出字符 '(' 时，从S态陷入M态，再返回S态（复用T2的QEMU 8.2.2调试截图）</center>

单步执行后，`priv` 变为3，`mcause=9`，`mepc` 记录了原来的 `ecall` 地址，PC 来到 `mtvec` 指向的固件陷阱入口。继续执行到下一条内核指令时，`priv` 又变为1，左侧也已经输出 `(`。因此，格式串的解析在 S 态完成，只有字符请求通过 `ecall` 进入 M 态。

换上带符号的 OpenSBI v1.3 后，练习2在 `uart8250_putc` 下断点，得到了图 T3-2 的调用栈：

![OpenSBI字符输出的调用栈](../images/T2-6-ecall-chain.png)

<center>图 T3-2　SBI请求经陷阱分发和控制台服务，最终到达uart8250_putc（复用T2截图）</center>

从下向上读这段栈：`_trap_handler` 保存现场，`sbi_trap_handler` 判断异常原因，`sbi_ecall_handler` 分发请求，旧式控制台处理函数再经 `sbi_putc` 到达 UART 驱动。图中 `uart8250_putc` 的参数为 `ch=40 '('`，与陷入前的 `a0` 相符。内核只提供调用号和字符，UART寄存器的操作留在固件中。

这里的 SBI 请求是 S态内核调用 M态固件，和用户程序的 U→S 系统调用不同。当前配置没有把 S态 `ecall` 委托回 S态，固件处理完请求后推进返回地址，再用 `mret` 回到内核。

本机 GCC 10.2.0 构建出的 `ecall` 在 `0x80200492`，图中另一工具链的地址为 `0x8020046c`。地址变化不影响这条调用关系。核对时还发现 `vcprintf`、`sbi_call` 没有独立存活符号：它们的逻辑被 `-O2` 内联，剩余未使用节又被链接器删除，所以源码里的函数层次不一定都出现在最终调用栈中。

内核不能直接使用宿主环境的 `printf`，因为这里没有用户态C运行库及它依赖的文件、系统调用接口，构建也使用了 `-nostdlib/-nostdinc`。项目通过自己的格式化函数和 SBI 输出完成这项工作。

### 最终提示词

````markdown
[PROMPT]
任务：分析内核从 cprintf 到 ecall 的输出功能，撰写 T3 报告的输出模块。
操作要求：直接写入真实报告文件，不修改代码；按源码顺序说明每层职责。
输出要求：列出源码调用链，解释各层职责；插入 T2 的 ecall 和 UART 调试截图，结合寄存器与调用栈说明格式化、陷入、设备输出和返回。注明截图环境与本机产物的地址差异。

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
````

### 分析迭代过程

按源码整理调用链后，`nm` 中找不到 `vcprintf` 和 `sbi_call`。检查编译参数和 `sbi_console_putchar` 的反汇编，确认是内联与未使用节删除，因而在报告中分别说明源码关系和实际机器码。输出的特权级则结合图 T3-1 的寄存器变化判断。

## 功能模块：构建与镜像加载

**负责人：** 2414099－李云鹏（lyp）

### 模块功能描述

执行 `make` 后，构建流程为：

```text
.c/.S → obj/.../*.o → bin/kernel → bin/ucore.img
```

`function.mk` 收集源文件并生成编译规则，Makefile把 kernel/libs 两组目标文件组成 `KOBJS`。`.S` 先由GCC预处理，因此能引用头文件中的栈大小宏。链接时，`ld -T tools/kernel.ld` 合并各节、确定地址和完成重定位，生成 ELF；`--gc-sections` 删除未使用的节。最后，`objcopy --strip-all -O binary` 生成裸镜像。

| 文件 | 内容 | 当前用途 |
|---|---|---|
| `bin/kernel` | ELF头、入口、程序头、装载内容和符号／调试信息 | QEMU按ELF装载，GDB读取符号 |
| `bin/ucore.img` | 内核内容字节及地址间隙的填充，无ELF元数据 | 原框架按指定地址加载的裸镜像 |

本次 `wc -c bin/ucore.img` 得到12296字节，即 `0x3008`，对应从 `0x80200000` 到 `0x80203008` 的跨度。ELF中还有头和调试信息，因此不能用它的文件大小直接表示内核占用的内存。

`make qemu` 仍然依赖 `ucore.img`，会先生成两个文件，但现在实际运行的参数是：

```sh
qemu-system-riscv64 -machine virt -nographic -bios default -kernel bin/kernel
```

这个修改来自T0：原来的 loader 只复制裸镜像，而 fw_dynamic 还需要有效的下一阶段入口。`-kernel` 让QEMU按ELF装载，并把入口信息提供给OpenSBI。练习2在PC还停在 `0x1000` 时就读到了内核入口指令，说明装载发生在虚拟CPU开始执行之前；OpenSBI随后完成初始化和特权级交接。

`make debug` 多加 `-s -S`，前者开启默认1234端口的GDB服务，后者让CPU启动时暂停；`make gdb` 读取 `bin/kernel` 的符号后连接QEMU。调试自编译固件时，两端传入相同的 `OPENSBI` 路径。

### 最终提示词

````markdown
[PROMPT]
任务：分析 Makefile/function.mk 从源文件到 ELF 和裸镜像的流程，补全 T3 构建模块及验证记录。
操作要求：直接修改真实报告文件，保留现有代码；在独立干净副本中编译运行。
输出要求：以当前 make 规则和 readelf 结果说明产物区别，结合 T0 解释当前 QEMU 加载方式；正文集中讲代码、观察和结果，环境调用故障留在任务文件。

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
````

### 验证结果

本机使用 WSL Ubuntu-22.04-OS、SiFive GCC 10.2.0、QEMU 6.2.0和OpenSBI v0.9，在已提交代码的独立副本中完成构建，并检查了ELF头、段表和符号表。运行 `timeout 8 make qemu` 得到：

```text
OpenSBI v0.9
Domain0 Next Address      : 0x0000000080200000
Domain0 Next Mode         : S-mode
(THU.CST) os is loading ...
```

加载提示输出后，内核按源码进入无限循环，8秒后由 `timeout` 停止，退出码为124。本机运行结果保留为文字记录；图 T3-1、T3-2 是小组T2任务的调试证据。框架没有 `tools/grade.sh`，Lab1不执行 `make grade`。

构建分析主要依据 `Makefile`、`tools/function.mk`、`tools/kernel.ld`；输出分析依据 `stdio.c`、`printfmt.c`、`console.c`、`sbi.c`，并结合T2的调试截图。指导书的[项目组成与执行流](http://8.135.34.58/lab2026/_book/lab1/lab1_2_2_file.html)用于核对任务范围。
