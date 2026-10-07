## 练习2：使用 GDB 验证启动流程

[PROMPT]

**任务**：在 lab1 工程（`code/`）中，使用 QEMU + GDB 跟踪 RISC-V 虚拟机从加电复位到执行内核第一条指令（0x80200000，`kern_entry`）的完整过程，并撰写实验报告中的「练习2」一节，写入 `report/sections/T2-ex2-gdb.md`。

**操作要求**：你必须在本机实际运行 QEMU 和 GDB 获取调试输出，报告中的每个结论都要有对应的 GDB 输出作为依据，不能凭记忆或照搬指导书。不要修改 `code/` 下的任何文件。指导书中与实测不符的说法，要在报告中指出并给出证据。

**输出要求**：直接写入报告文件，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中的信息，以实际运行结果为准。

[RELY]

```makefile
# code/Makefile
debug: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios $(OPENSBI) \
		-kernel $(kernel) \
		-s -S

gdb:
	riscv64-unknown-elf-gdb \
    -ex 'file bin/kernel' \
    $(GDB_OPENSBI) \
    -ex 'set arch riscv:rv64' \
    -ex 'target remote localhost:1234'
```

```asm
# code/kern/init/entry.S（省略注释）
kern_entry:
    la sp, bootstacktop
    tail kern_init
```

```c
// code/libs/sbi.c
uint64_t SBI_CONSOLE_PUTCHAR = 1;
void sbi_console_putchar(unsigned char ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
}
```

- 运行环境：QEMU 8.2.2，自带 OpenSBI v1.3（fw_dynamic）。固件 ELF 位于 `/usr/share/qemu/opensbi-riscv64-generic-fw_dynamic.elf`，**没有符号表**。
- OpenSBI v1.3 的源码可从 https://github.com/riscv-software-src/opensbi 获取，用 `make PLATFORM=generic CROSS_COMPILE=riscv64-unknown-elf-` 编译出带调试信息的 `build/platform/generic/firmware/fw_dynamic.elf`。QEMU 可以用 `-bios <路径>` 换上这份固件，GDB 用 `add-symbol-file <路径>` 加载它的符号。
- 指导书给出的提示：复位地址是 0x1000；“SBI 固件将内核加载到 0x80200000，可以用 `watch *0x80200000` 观察加载瞬间”；用 `b *0x80200000` 在内核入口断下。
- GDB 可以通过 `$priv` 查看当前特权级，通过 `$mepc`、`$mstatus`、`$mcause`、`$mtval`、`$medeleg`、`$mideleg` 等读取 CSR。

[GUARANTEE]

必须回答的问题和需要交付的产出：

1. 加电后最初执行的几条指令位于什么地址，每条指令完成了什么功能（附 GDB 输出）。
2. 从 0x1000 到 0x80200000 经过了哪几个阶段，每个阶段运行在什么特权级，由谁提供。
3. OpenSBI 把控制权交给内核的具体时刻和方式（哪条指令、跳转地址、特权级如何变化）。
4. 核实指导书中关于“OpenSBI 加载内核、用 watch 观察”的提示是否成立。
5. 一张启动流程与物理内存布局的示意图，以及调试截图（复位处、交接给内核处、watch 实验、内核调用 SBI），每张图插在对应的文字中间。
6. 说明真实硬件上内核由谁加载，与 QEMU 的做法有何不同。
7. 编译带符号的 OpenSBI，按函数名梳理它从入口到交接内核的完整初始化流程，以及处理内核 ecall 的调用链；为 Makefile 增加可选变量，使 `make debug` / `make gdb` 能切换到这份固件。

[SPECIFICATION]

## 阶段一：复位与 MROM

**Pre-Condition**:
- `make debug` 已启动，CPU 停在第一条指令之前；GDB 通过 `make gdb` 连接成功。

**Post-Condition**:
- 报告列出 0x1000 处的全部指令，并给出单步执行后的寄存器变化；说明 a0/a1/a2 各自的含义，以及紧随其后的数据区（固件入口、设备树地址、`fw_dynamic_info` 各字段）。

## 阶段二：OpenSBI 初始化与交接

**Pre-Condition**:
- 已到达 0x80000000。QEMU 自带的 OpenSBI 没有符号，无法按函数名下断点。

**Post-Condition**:
- 报告说明 OpenSBI 入口处几条指令的作用，按函数列出 OpenSBI 从入口到交接内核的初始化流程，并找到“降到 S 态并跳往内核”的那条指令。

  **Case 1**:
  - 使用带符号的 OpenSBI 时：在各关键函数上设断点，用 `bt` 确认调用关系，给出每个函数的作用；还要说明这份固件与 QEMU 自带固件在内存布局上的差异，避免把两者的地址混用。

  **Case 2**:
  - 使用 QEMU 自带的无符号固件时：如果 OpenSBI 中有多条可能执行的 `mret`，就要区分哪一次是交接给内核的（mepc = 0x80200000，MPP = S 态）。其余各次 mret 的成因要结合 mcause、mtval，以及带符号固件中对应的源码行给出解释。

  **Case 3**:
  - 交接时 OpenSBI 设置好的中断和异常委托（medeleg、mideleg），要说明其含义，特别是 S 态 `ecall` 为什么没有被委托。

## 阶段三：进入内核

**Post-Condition**:
- 在 `kern_entry` 处报告特权级、`satp`、`a0`、`a1`、`sp` 的取值，并解释它们与练习 1 中 `la sp, bootstacktop` 的关系。

## 指导书提示的核实

**Post-Condition**:
- 用 GDB 证明 0x80200000 处的内核代码在 CPU 执行第一条指令之前是否已经存在，以及 watchpoint 是否会被触发；如果与指导书不符，就指出谁、在什么时候把内核放进了内存。

## 延伸：内核如何调用 SBI

**Post-Condition**:
- 在 `sbi_console_putchar` 中的 `ecall` 处断下，说明 a7、a0 的含义，以及 ecall 前后特权级、mcause、mepc、mtvec 的变化。再用带符号固件给出这次调用在 OpenSBI 内部从陷阱入口到串口驱动的完整调用链。

**Requirements**:
- 所有地址、寄存器值和指令都必须来自本次实际运行的输出。
- 面向不熟悉 OpenSBI 的读者，先讲清楚 M/S/U 特权级和 SBI 的作用，再展开细节。
- 示意图采用传统工科风格：黑白、直角矩形。截图使用真实终端画面，左栏 make debug、右栏 make gdb。
