## 功能模块：链接脚本与内核内存布局

**负责人：** 2414099－李云鹏（lyp）

### 模块功能描述

本模块分析已有链接配置，不需要新增函数。`code/tools/kernel.ld` 为链接器规定内核的布局与入口；它决定符号地址和 ELF 的组织，而实际把内容放入模拟内存的是 QEMU 的装载过程。以下结果来自 2026-10-07 对提交 `7b0948d` 的干净副本构建，工具链为 SiFive GCC 10.2.0。

脚本的三个关键设置各有职责：

```ld
OUTPUT_ARCH(riscv)
ENTRY(kern_entry)

BASE_ADDRESS = 0x80200000;
```

`OUTPUT_ARCH` 指定目标架构；`ENTRY(kern_entry)` 把 `kern_entry` 的最终地址写入 ELF 入口字段；`. = BASE_ADDRESS` 设置位置计数器，让接下来的 `.text` 从 `0x80200000` 开始。**ENTRY 本身不把函数移动到最前面**，也不负责复制程序到内存。

当前 `.text` 的输入规则是 `*(.text.kern_entry .text .stub .text.* .gnu.linkonce.t.*)`。但 `entry.S` 实际声明的是普通 `.text`，没有创建专门的 `.text.kern_entry`。用 `make print-kobjs` 查到链接输入依次为：

```text
obj/kern/init/entry.o obj/kern/init/init.o obj/kern/libs/stdio.o
obj/kern/driver/console.o obj/libs/printfmt.o obj/libs/readline.o
obj/libs/sbi.o obj/libs/string.o
```

因此本次 `entry.o` 的入口代码先被收进从基地址开始的 `.text`，且 `kern_entry` 位于其开头，最终得到 `kern_entry=0x80200000`。这依赖当前源文件收集和目标文件顺序，不能把规则中的 `.text.kern_entry` 字样当成已经生效的单独入口段优先保证。当前代码运行正确，本分析不修改链接方式。

脚本按 `.text → .rodata → 页对齐 → .data → .sdata → .bss` 排列内核主体，本次 `readelf -S` 给出：

| 区域 | 地址范围（左闭右开） | 内容或含义 |
|---|---|---|
| `.text` | `[0x80200000,0x802004c8)` | 入口和存活的函数机器码 |
| `.rodata` | `[0x802004c8,0x80200738)` | 字符串、格式化所需的只读数据 |
| 对齐空隙 | `[0x80200738,0x80201000)` | `. = ALIGN(0x1000)` 使后续数据按页对齐 |
| `.data` | `[0x80201000,0x80203000)` | 8192 字节启动栈 |
| `.sdata` | `[0x80203000,0x80203008)` | 本次存活的小数据 `SBI_CONSOLE_PUTCHAR` |
| BSS 清零区间 | `[0x80203008,0x80203008)` | 本次为空，没有非空 `.bss` 输出段 |

`bootstacktop` 与 `SBI_CONSOLE_PUTCHAR` 在本次产物中地址相同，因为前者是栈区域的结束标签，后者在紧接着的 `.sdata` 开始。栈区不包含高端边界，向下增长，不占用这个全局变量的字节。

三个边界符号的意义如下：`etext` 表示脚本中 `.text` 结束位置；`edata` 在 `.data/.sdata` 后，表示已初始化数据区结束；`end` 在 `.bss` 后，表示上述内核静态布局结束。它们是链接时的地址符号，不是源码新建的普通数组。`extern char edata[], end[]` 用数组形式引用这些地址，随后 `memset(edata,0,end-edata)` 清零其中的 BSS 区域。脚本使用 `PROVIDE` 条件提供符号；本次 `edata/end` 被代码引用而出现在 `nm` 中，`etext` 未被引用，没有出现在最终符号表，不能把它伪造成实测的 `nm` 输出。

`readelf -l` 还显示两个 `LOAD` 段：一个覆盖 `.text/.rodata`，起始虚拟和物理地址均为 `0x80200000`，大小为 `0x738`；另一个覆盖 `.data/.sdata`，起始地址为 `0x80201000`，大小为 `0x2008`。程序头描述装载范围，节表则更细地描述链接组织。本实验未启用分页，GDB 在内核入口读到 `satp=0`，地址转换处于 Bare 模式；ELF 的 R E／RW 标志也不能直接当成已经建立的 S 态页表保护。

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

核对链接规则时发现 `entry.S` 没有定义 `.text.kern_entry`，于是进一步查 `make print-kobjs`，把“ENTRY 保证放在最前”修正为“基地址、目标文件顺序和实际入口偏移共同决定”。同时根据 `readelf/nm` 明确本次 BSS 为空、`etext` 未产生可见符号。结论已经反映在最终提示词中，本模块没有代码修改。

## 功能模块：格式化输出与 SBI 服务

**负责人：** 2414099－李云鹏（lyp）

### 模块功能描述

本模块分析已有输出功能。主要接口和源码内部函数为：

```c
int cprintf(const char *fmt, ...);
int vcprintf(const char *fmt, va_list ap);
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);
static void cputch(int c, int *cnt);
void cons_putc(int c);
void sbi_console_putchar(unsigned char ch);
uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0, uint64_t arg1, uint64_t arg2);
```

`kern_init` 调用 `cprintf("%s\n\n", message)`，其中 `message` 自身末尾已有换行。因此字符内容来自 `%s` 替换后的字符串，随后还有格式串中的两个换行，屏幕输出不是由宿主 C 库的 `printf` 完成的。

完整的源码逻辑链为：

```text
kern_init
  → cprintf → vcprintf → vprintfmt
       → cputch（每输出一个字符调用一次）
          → cons_putc → sbi_console_putchar → sbi_call
             → ecall
                → OpenSBI 的陷阱处理和控制台服务 → UART
                → 返回内核，继续处理下一个字符
```

| 层次和位置 | 实际职责 |
|---|---|
| `kern/libs/stdio.c:cprintf` | 用 `va_start` 建立变参列表，交给 `vcprintf`，最后返回字符计数 |
| `vcprintf` | 将计数器置零，把 `cputch` 回调和计数器地址传给格式化函数 |
| `libs/printfmt.c:vprintfmt` | 解释格式串，处理 `%s`、整数、宽度等；逐字符调用给定回调 |
| `cputch` | 调用 `cons_putc` 后增加计数；这里没有实现缓冲队列 |
| `kern/driver/console.c:cons_putc` | 将参数转换为 `unsigned char`，转交 SBI 字符输出接口 |
| `libs/sbi.c:sbi_console_putchar` | 调用号为 `SBI_CONSOLE_PUTCHAR=1`，字符是第一个参数 |
| `sbi_call` | 将调用号置于 `a7=x17`，参数置于 `a0/a1/a2`，执行 `ecall`，从 `a0` 取得返回值 |

`vprintfmt` 不依赖 UART，只依赖一个“输出字符”的回调，因此相同的格式化逻辑也可供 `vsnprintf` 的内存缓冲回调使用。`cprintf` 的计数是回调处理的字符数；当前 `cons_putc` 没有把设备错误返回上来，不能把计数解释成硬件逐字节确认成功的数量。

**特权级切换发生在 `ecall`，格式化过程仍在 S 态。** 本实验用旧式 SBI console_putchar 请求固件服务，当前 OpenSBI 配置没有把 S 态环境调用委托回 S 态，因此执行 `ecall` 后，处理器进入 M 态陷阱入口，`mcause=9` 表示来自 S 态的环境调用，`mepc` 保存该指令地址。固件处理字符输出并推进返回 PC，最终用 `mret` 返回 S 态的下一条指令。陷入所需的软件现场保存由固件实现，硬件不会自动保存所有通用寄存器。T2 已用带符号 OpenSBI 追踪到 `uart8250_putc`，这里沿用其服务链结论。

这不是用户进程向内核发起的 U→S 系统调用：本实验没有用户进程，调用者是 S 态内核，服务方是 M 态固件。也不能由此推出任意来源的 `ecall` 都进入 M 态，实际目的特权级还取决于来源和异常委托设置。

本次最终 ELF 中，`sbi_console_putchar` 内的 `ecall` 位于 `0x80200492`，与 T2 工具链产物中的地址不同。`vcprintf` 和 `sbi_call` 没有独立存活符号：在 `-O2` 下逻辑被内联，未使用的独立代码段又被 `--gc-sections` 删除。源码调用链解释职责，反汇编解释实际执行，二者不能机械地一一对应。

内核不能直接链接宿主环境的标准 `printf`，因为它运行在没有用户态 C 运行库、文件描述符和宿主系统调用支持的环境中，构建规则也使用了 `-nostdinc`、`-fno-builtin` 和 `-nostdlib`。这里通过项目自己的变参、格式化和 SBI 输出实现所需功能；同名 `memset` 也来自 `libs/string.c`，不是链接进来的宿主库函数。

### 最终提示词

````markdown
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
````


### 分析迭代过程

最初按源码列函数时，最终 `nm` 中找不到 `vcprintf/sbi_call`。检查优化参数及 `sbi_console_putchar` 反汇编后，确认应描述源码逻辑链与优化产物的差异，而非把缺失符号当成函数未实现。报告也补上了原简写调用链省略的 `vcprintf/sbi_call`，明确格式化在 S 态完成，没有编造 M 态格式化或内核字符缓冲。

## 功能模块：构建、镜像生成与 QEMU 加载

**负责人：** 2414099－李云鹏（lyp）

### 模块功能描述

本模块分析构建规则，不需要新增接口。`code/Makefile` 与 `tools/function.mk` 共同把源文件变成可调试的 ELF 和裸二进制镜像：

```text
.c / .S
  → GCC 编译或预处理后汇编 → obj/.../*.o（并生成依赖 .d）
  → ld -T tools/kernel.ld → bin/kernel（ELF）
  → objcopy --strip-all -O binary → bin/ucore.img（纯二进制）
```

`function.mk` 中的 `listf` 收集源文件，`add_files` 为文件生成编译规则并归入 kernel/libs 两组；`KOBJS=$(call read_packet,kernel libs)` 按组形成链接输入。`.S` 使用 GCC 驱动，会先处理 `#include` 和宏，因而能使用 `PGSHIFT/KSTACKSIZE`。C 编译采用 `-mcmodel=medany`、`-O2`、`-g`，每个函数／数据项可以拥有独立节；链接用 `-m elf64lriscv -nostdlib --gc-sections`，删除不可达的节，依据链接脚本完成重定位。链接后还生成 `obj/kernel.asm` 和 `obj/kernel.sym` 辅助分析。

两个最终文件的区别是：

| 产物 | 保存的信息 | 用途 |
|---|---|---|
| `bin/kernel` | ELF 文件头、程序头、节表、入口地址、装载内容及符号／调试信息 | 当前 QEMU 按 ELF 装载；GDB 按源码和符号调试 |
| `bin/ucore.img` | 从最低装载地址开始的内容字节及区间间填充，无 ELF 头、入口字段或符号表 | 可按外部指定地址加载的裸镜像；原框架的 loader 使用它 |

`objcopy` 做格式转换，不负责在运行时启动 CPU，也不会把二进制变成磁盘文件系统。按本次 ELF 的装载内容，裸镜像覆盖从 `0x80200000` 到 `0x80203008` 的地址跨度，长度为 `0x3008`（12296）字节；ELF 另含文件头、调试信息等，二者不能用相同文件偏移解释机器码。BSS 通常不占镜像中的初始化字节，需要装载器或启动代码为其建立零值；本次 BSS 本身为空。

`make qemu` 的依赖仍包含 `bin/ucore.img`，所以会先把 ELF 和裸镜像都构建出来；但当前真正执行的是：

```sh
qemu-system-riscv64 -machine virt -nographic -bios default -kernel bin/kernel
```

这里省略未设置的附加镜像依赖。QEMU 在虚拟 CPU 执行复位指令前，依据 ELF 把内核放到模拟 RAM，并把下一阶段入口信息交给 OpenSBI；OpenSBI 完成固件初始化与特权级交接，进入内核。指导书中“OpenSBI 加载内核”的描述不能直接当成本配置的装载事实：本次 GDB 停在 `0x1000` 时已能读到 `0x80200000` 的入口指令，T2 也验证了这一点。

T0 将原先的 `-device loader,file=bin/ucore.img,addr=0x80200000` 改为 `-kernel bin/kernel`，原因是动态固件还需要有效的下一阶段入口，单独复制镜像不够。这个历史问题和修复由 T0 负责，本次验证证明该修复也可在本机 QEMU 6.2.0 / OpenSBI v0.9 下启动；不额外声称本机验证了 QEMU 4.1 或 8.2。

`make debug` 使用同一内核和固件，增加 `-s -S`：`-s` 开启默认 TCP 1234 的 GDB 服务，`-S` 让 CPU 从启动时暂停等待调试。`make gdb` 用 `file bin/kernel` 读取内核符号，再连接 QEMU。两端如果使用自编译固件，都应传相同的 `OPENSBI` 路径。

### 最终提示词

````markdown
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
````


### 分析迭代过程与验证

Windows 向 WSL 传递多层引号的 `bash -c` 命令时发生解析错误，改用标准输入脚本；末尾 CR 又被 Bash 当成命令，随后改用 Python 子进程直接传参数完成 GDB 和运行验证。导出副本时曾误用不存在且无权限创建的 `/home/lyp`，查询实际用户后改用 `/home/lenovo/Code/NKU-OS-LAB/lyp-lab1`。这些是环境调用调整，没有修改实验代码。

在干净副本执行以下命令：

```sh
export PATH=/opt/riscv/bin:$PATH
make -j2
make print-kobjs
riscv64-unknown-elf-readelf -h -l -S bin/kernel
riscv64-unknown-elf-nm -n bin/kernel
riscv64-unknown-elf-objdump -d bin/kernel
wc -c bin/ucore.img
timeout 8 make qemu
```

本次 `make -j2` 完成八个源文件的编译、ELF 链接和镜像生成。`readelf` 的入口为 `0x80200000`，段范围与上表一致。`make qemu` 输出关键内容：

```text
OpenSBI v0.9
Domain0 Next Address      : 0x0000000080200000
Domain0 Next Mode         : S-mode
(THU.CST) os is loading ...
```

内核随后按源码无限循环，由 `timeout` 在 8 秒后停止，退出码 124 是这次验证主动限制运行时间的预期结果，不是内核启动失败。本次 GDB 批处理退出 0，验证了入口、栈和尾跳转；截图属于 T2/T6 的任务范围，本模块保留文字记录和可复现命令。框架虽保留 `grade` 目标，但缺少 `tools/grade.sh`，Lab1 的 `make grade` 不适用，不能宣称评分脚本通过。

### 参考依据

以当前 `Makefile`、`tools/function.mk`、`tools/kernel.ld`、`kern/libs/stdio.c`、`libs/printfmt.c`、`kern/driver/console.c`、`libs/sbi.c` 和上述构建输出为依据；参阅[指导书项目组成与执行流](http://8.135.34.58/lab2026/_book/lab1/lab1_2_2_file.html)、[提示词结构](http://8.135.34.58/lab2026/_book/lab0.5/3_prompt_structure.html)以及 T0/T2 已完成章节。指导书的目录与装载描述均经过源码核对，未照搬。
