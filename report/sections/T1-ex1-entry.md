## 练习1：理解内核启动中的程序入口操作

**负责人：** 2414099－李云鹏（lyp）

本练习分析已有代码，不需要补写内核功能。根据[指导书练习1](http://8.135.34.58/lab2026/_book/lab1/lab1_2_1_exercise.html)，需要回答 `la sp, bootstacktop` 和 `tail kern_init` 的操作及目的。下面的地址来自 2026-10-07 对提交 `7b0948d` 的干净代码副本的验证：WSL Ubuntu-22.04-OS、SiFive GCC 10.2.0、QEMU 6.2.0、自带 OpenSBI v0.9。它们与练习2使用的 QEMU 8.2.2 环境分别记录，固件地址不能混用。

### 1. 建立内核自己的启动栈

`code/kern/init/entry.S` 的入口只有两条伪指令：

```asm
kern_entry:
    la sp, bootstacktop

    tail kern_init
```

`la`（load address）把符号 `bootstacktop` 的**地址**写入栈指针 `sp`（x2），不是读取该地址处的数据。RISC-V 的栈向低地址增长，因此先把 `sp` 指向预留区域的高地址端，随后 C 函数通过减小 `sp` 分配栈帧，保存返回地址、寄存器或局部数据。

启动栈在同一文件的 `.data` 段中预留：

```asm
.section .data
    # .align 2^12
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE
    .global bootstacktop
bootstacktop:
```

`mmu.h` 定义 `PGSIZE=4096`、`PGSHIFT=12`，`memlayout.h` 定义 `KSTACKPAGE=2`、`KSTACKSIZE=(KSTACKPAGE * PGSIZE)`，所以区域大小为 **8192 字节，即 8 KiB**。这里 RISC-V 汇编的 `.align 12` 表示按 `2^12=4096` 字节对齐；栈顶也满足常用 RISC-V ABI 的 16 字节对齐要求。`.space` 是汇编、链接阶段的空间预留，执行 `la` 并不在运行时申请物理页。

本次符号表结果为：

| 符号或范围 | 地址／大小 | 含义 |
|---|---|---|
| `bootstack` | `0x80201000` | 预留区域的低地址端 |
| `bootstacktop` | `0x80203000` | 高地址端，空栈的初始 `sp` |
| 可用栈区域 | `[0x80201000, 0x80203000)` | 8192 字节；栈帧向低地址扩展 |

这里用“低地址端／高地址端”避免混淆：`bootstacktop` 名字里的 top 表示初始空栈的位置，后续运行时的栈顶由不断变化的 `sp` 表示。

建立有效栈的目的，是为接下来的 C 代码提供可写、可访问的栈空间。GDB 停在 `kern_entry` 时，本机 `sp=0x80017ee0`，仍位于 OpenSBI 的固件区域；固件的 PMP 配置不允许 S 态访问该区域，不能直接沿用这个栈。实际 `kern_init` 的反汇编会执行 `addi sp,sp,-16`、`sd ra,8(sp)`，后续 `cprintf` 也要使用栈，因此必须在进入这些 C 函数前设置内核栈。对本入口而言，这是第一项工作；一般的内核入口也可以先执行不依赖栈的寄存器操作，并非所有系统的第一条机器指令都只能设置 `sp`。

### 2. `la` 的真实指令与验证

本次 ELF 的入口反汇编如下：

```text
80200000: 00003117    auipc sp,0x3
80200004: 00010113    mv    sp,sp
80200008: a009        j     8020000a <kern_init>
```

前两条对应 `la`。`auipc` 用当前指令地址加上左移 12 位的立即数：`0x80200000 + (0x3 << 12) = 0x80203000`；下一条补上低位偏移。本次低位偏移恰为零，`objdump` 默认把 `addi sp,sp,0` 显示为别名 `mv sp,sp`。使用 `-M no-aliases` 可看到原指令，不能仅凭 `mv` 误认为这里没有加载栈地址。

在 GDB 中于入口断下，连续执行两次 `si` 后，`PC=0x80200008`、`sp=0x80203000`，与 `bootstacktop` 一致。设置 `sp` 的两条机器指令执行完成后，才进入 C 函数。

### 3. `tail kern_init`：不建立返回路径的控制转移

`tail kern_init` 把控制权转移给 `code/kern/init/init.c` 的 `kern_init`。与 `call` 的主要区别是：`call` 通常把返回地址写到 `ra`（x1），供被调用函数返回；`tail` 的跳转不写 `ra`，因此不会为 `kern_entry` 建立新的返回地址。它也**不会自动清理栈或保存现场**。当前 `kern_entry` 没有建立自己的栈帧，所以不需要额外回收栈帧。

在未链接的 `entry.o` 中，`tail` 对应的是带重定位的 `auipc t1,...` 和 `jalr x0,0(t1)`（默认显示为 `jr t1`）。目标地址接近，且工具链支持压缩指令，链接器松弛后把它缩短为位于 `0x80200008` 的 16 位 `c.j`，默认反汇编显示为 `j 0x8020000a`。因此“尾跳转不保存返回地址”是稳定语义，而“固定展开为两条机器指令”不是本次最终产物的事实。

GDB 的实测如下：

| 时刻 | PC | sp | ra |
|---|---|---|---|
| 刚进入 `kern_entry` | `0x80200000` | `0x80017ee0` | `0x800078cc` |
| 执行完 `la` | `0x80200008` | `0x80203000` | `0x800078cc` |
| 执行完 `tail` | `0x8020000a` | `0x80203000` | `0x800078cc` |

跳转前后 `ra` 不变，说明这次转移没有保存返回地址。`ra` 中碰巧还留着固件的值，不代表内核可以安全返回 OpenSBI。

### 4. 与 `noreturn`、BSS 初始化的关系

源码声明为 `int kern_init(void) __attribute__((noreturn));`，`noreturn` 告诉编译器该函数不返回，让编译器按这个约定生成代码；它不是一条硬件指令，也不会自动让函数停止返回。当前实现先执行 `memset`，再输出加载提示，最后执行 `while (1)`，这段无限循环才实际保证函数不返回。本次反汇编中末尾为 `0x8020003a` 处跳回自身的指令。即使声明返回类型为 `int`，也不存在需要正常返回的整数结果。

`kern_init` 的第一项初始化是：

```c
    extern char edata[], end[];
    memset(edata, 0, end - edata);
```

`edata`、`end` 是链接脚本提供的边界，分别在 `.data/.sdata` 后和 `.bss` 后，清零范围为 `[edata,end)`。这样可以满足静态存储期未显式初始化变量的零初始化语义。本次 ELF 中 `edata=end=0x80203008`，没有存活的非空 BSS，因此此次调用的长度是零；功能仍然保留，供以后加入 BSS 数据时使用。

启动栈放在 `.data` 的关键原因是：进入 `kern_init` 后，栈已经在使用，清零区域不能覆盖正在保存返回地址和局部数据的栈帧。若直接把这块启动栈移进当前会整体清零的 BSS 区域，`memset` 可能破坏自己的运行现场。当前 `.data` 中的预留栈则位于 `edata` 之前，不受这次清零影响；代价是 `.space` 的零字节也占据镜像的文件空间。

### 5. 验证方法与分析迭代

在已提交代码导出的独立副本中执行：

```sh
export PATH=/opt/riscv/bin:$PATH
make -j2
riscv64-unknown-elf-objdump -dr obj/kern/init/entry.o
riscv64-unknown-elf-objdump -d -M no-aliases bin/kernel
riscv64-unknown-elf-nm -n bin/kernel
```

用 `make debug` 和另一个终端的 `make gdb` 可复现单步检查：

```text
b *kern_entry
continue
info registers pc sp ra
si
si
info registers pc sp ra
si
info registers pc sp ra
p/x &bootstack
p/x &bootstacktop
p/x &edata
p/x &end
```

本次自动化验证使用独立的 GDB 端口 1235，GDB 批处理退出码为 0；以上交互命令适用于 Makefile 默认的 1234 端口。

分析中实际遇到两项需要修正的预期：一是最终 ELF 的尾跳转只有一条压缩指令，于是补查 `entry.o` 的重定位记录，区分汇编展开和链接松弛；二是 `edata` 与 `end` 相同，于是将报告改为说明零长度清零，避免声称观察到非空 BSS 被初始化。相应约束已整理进 [T1 最终提示词](T1-ex1-entry.prompt.md)。本练习没有修改代码，也没有虚构代码生成或修复轮次。

### 参考依据

结论以 `code/kern/init/entry.S`、`init.c`、`kern/mm/mmu.h`、`memlayout.h`、`tools/kernel.ld` 和本次构建／GDB 输出为主；`reference/OS lab1.pdf` 仅用于了解报告覆盖范围，其旧环境地址和装载结论未直接采用。
