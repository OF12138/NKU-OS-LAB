## 功能模块二：中断与异常处理

**负责人：** 2414099-李云鹏

### 模块功能描述

本模块补充 `kern/trap/trap.c` 中的两个接口：

```c
void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);
```

汇编入口将寄存器现场保存为 `struct trapframe`，`trap_dispatch()` 根据 `cause` 最高位分派到中断或异常处理函数。处理完成后，功能模块一的 `RESTORE_ALL` 从帧中恢复 `sstatus`、`sepc` 和通用寄存器，再执行 `sret`。本模块通过修改 `tf->epc` 调整异常返回位置。

#### 1. 中断处理

`interrupt_handler()` 先清除 `cause` 的最高位，得到中断编号。采用位掩码而非减去固定高位，是因为评测还会直接传入不带中断标志的编号 15；两种输入都应得到相同的编号。

管理态时钟中断按以下顺序处理：

1. 调用 `clock_set_next_event()` 设置下一次事件；
2. 令全局 `ticks` 增加一；
3. `ticks` 为 `TICK_NUM=100` 的整数倍时调用 `print_ticks()`；
4. 用函数内静态变量累计打印次数，达到 `SHUTDOWN_AFTER=10` 时调用 `sbi_shutdown()`。

`ticks` 一直累计，不在打印时清零。阈值选为十次，是为了让评测等待的 200 ticks 能先返回到测试主流程，输出测试完成信息；若在第 200 次中断中就关机，计数虽然达标，主流程却没有机会继续执行。框架每次把定时器设为当前时间加 100000，在 QEMU 10 MHz 时基下约每秒打印一次。

管理态软件中断通过 `clear_csr(sip, SIP_SSIP)` 清除挂起位，并打印 `Supervisor software interrupt`。其余框架已定义的软件、时钟、外部、协处理器及宿主中断分别输出类型提示。未知中断打印 `Unknown interrupt: cause <编号>` 后调用 `print_trapframe()`。这些诊断分支不是完整的设备驱动；例如外部中断还没有 PLIC 的 claim/complete 流程。

异步中断不是一条需要跳过的故障指令，返回后应从硬件记录的继续执行位置恢复。

#### 2. 异常处理与指令长度

`exception_handler()` 对非法指令和断点分别打印 `Illegal instruction at`、`Breakpoint at` 及推进前的地址，随后读取故障地址处的首个 16 位半字：

```c
uint16_t instruction = *(const volatile uint16_t *)tf->epc;
tf->epc += (instruction & 0x3) == 0x3 ? 4 : 2;
```

| 本实验中的指令 | 编码 | 首半字低两位 | EPC 增量 |
|---|---|---|---|
| 压缩断点 `c.ebreak` | `0x9002` | `10` | 2 字节 |
| 普通断点 `ebreak` | `0x00100073` | `11` | 4 字节 |
| 评测非法指令 | `.word 0x0000007b` | `11` | 4 字节 |

EPC 的增量取决于指令编码：普通指令推进 4 字节，压缩指令推进 2 字节。先读取半字，也能适应压缩指令的 2 字节对齐地址。这一判断只适用于本实验的 16/32 位指令，不包含 RISC-V 长指令的完整解码。

这里直接读取 EPC，要求故障指令位于可读的内核代码中；处理用户地址还需要安全取指和二次异常处理。其他异常调用 `print_trapframe()` 后返回，不跳过指令。若异常条件仍存在，返回后可能再次陷入，因此这些分支目前只提供诊断。

### 最终提示词

<!-- include:T2-trap-handler.prompt.md -->

### 实现迭代过程

#### 第一阶段：核对框架约束并实现处理函数

实现前检查了任务说明、`clock.c`、Trap 返回汇编和原评测的触发方式，确认了以下约束：

- 框架未提供 `SHUTDOWN_AFTER`，且测试需要等到 200 ticks 后继续运行；在功能区域定义阈值为 10，不修改评测。
- 测试既有真实异常，也有直接调用处理函数的合成帧；中断原因码使用清位操作，未知原因保留 Trap 帧诊断。
- `ebreak` 可能被汇编成压缩形式；返回地址从实际编码判断，不从异常种类推断长度。

使用提交 `b95765a` 的 `code/` 归档叠加当前 `trap.c`，在独立目录编译。首次在默认 WSL 的 GCC 15.1.0 下编译通过；后续正式验收改在 OS 专用发行版的 GCC 10.2.0 下重新构建，同样保留原有 `-Wall -Werror -O2 -std=gnu99`，编译、链接均通过。没有发生需要修复处理函数的编译错误。

#### 第二阶段：在 WSL 环境中运行

正式验收采用：

| 项目 | 实际环境 |
|---|---|
| WSL 发行版 | `Ubuntu-22.04-OS` |
| 交叉编译器 | `/opt/riscv/bin/riscv64-unknown-elf-gcc`，SiFive GCC 10.2.0 |
| 调试器 | `/opt/riscv/bin/riscv64-unknown-elf-gdb`，SiFive GDB 10.1 |
| 模拟器 | `~/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64` |
| 固件 | QEMU 自带 OpenSBI v0.4 |

进入该发行版后设置环境变量：

```bash
# Windows 终端先执行：wsl -d Ubuntu-22.04-OS
export PATH=/opt/riscv/bin:$PATH
export QEMU=$HOME/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64
"$QEMU" --version
make -j4
make qemu
make grade
```

`make qemu` 的实际输出中出现 OpenSBI v0.4、内存检查成功和时钟初始化提示，随后共打印十次 `100 ticks`。进程在第十次输出后正常退出，未触发外层 25 秒超时，退出码为 0。

<img src="../images/T2-qemu-run.png" alt="QEMU 4.1.1 中的内核正常运行" style="zoom:67%;" />

<p align="center">图 2-1　统一环境运行：十次时钟输出后正常关机</p>

原始评分的三项结果分别为 35/35、25/25、40/40，总分 100/100。

<img src="../images/T2-make-grade.png" alt="Lab3 原始评测结果" style="zoom:67%;" />

<p align="center">图 2-2　make grade 评分结果摘录：100/100</p>

#### 第三阶段：GDB 核对异常返回地址

用 `make debug` 启动等待调试的 QEMU，在另一终端执行 `make gdb`。两类异常都在 `intr_enable()` 入口触发：先用 `b *intr_enable`、`c` 停在入口，删除断点，再临时改写该地址的指令。压缩断点写入 16 位的 `0x9002`，非法指令写入 32 位的 `0x0000007b`。这些改写只作用于 QEMU 来宾内存，不修改源码或镜像文件；两类测试分别运行。

两组调试都先用 `b *exception_handler` 停在处理函数的第一条指令，此时 `$a0` 就是传入的 Trap frame 指针，可以读取 `cause` 和原始 `epc`。处理函数返回后，再用 `b *__trapret` 停在 `RESTORE_ALL` 之前，通过 `$sp` 读取帧中更新后的 `epc`。最后从反汇编中找到 `sret`，检查写回的 `$sepc` 并单步观察 `$pc`。

**1. 压缩断点异常**

在 `intr_enable` 入口执行 `set {unsigned short}&intr_enable = 0x9002`，然后继续运行，触发压缩断点异常。

<img src="../images/T2-breakpoint-epc.png" alt="压缩断点异常的 GDB 返回地址验证" style="zoom:67%;" />

<p align="center">图 2-3　压缩断点：进入处理函数与帧 EPC 更新</p>

- 进入 `exception_handler` 时，`cause = 0x3`，对应断点异常。帧中的 `epc` 和 `$sepc` 都是 `0xffffffffc020082e`，`x/i $sepc` 显示该地址的指令为 `ebreak`。虽然反汇编使用这个名字，写入的编码 `0x9002` 表明它实际是 2 字节的 `c.ebreak`。
- 停在 `__trapret` 时，`RESTORE_ALL` 尚未执行，`sp` 仍指向 Trap frame。用 `p/x ((struct trapframe *)$sp)->epc` 读到 `0xffffffffc0200830`，比原始地址增加 2 字节，说明处理函数已按压缩指令长度更新了帧中的返回地址。

<img src="../images/T2-breakpoint-ret-pc.png" alt="压缩断点异常执行 sret 前后的返回地址" style="zoom:67%;" />

<p align="center">图 2-4　压缩断点：sepc 写回与 sret 返回</p>

- 反汇编中，`sret` 位于 `0xffffffffc0200cd6`（`__trapret+74`）。停在这里时，`RESTORE_ALL` 已执行完，`$sepc = 0xffffffffc0200830`，与帧中更新后的 EPC 相同。
- 执行 `si` 后，`$pc` 回到 `0xffffffffc0200830`（`intr_enable+2`），`x/i $pc` 显示 `c.nop -32`。处理器跳过了原来的 2 字节断点指令，没有再次回到 `0xffffffffc020082e` 触发同一个异常。

**2. 非法指令异常**

重新运行后，在同一入口执行 `set {unsigned int}&intr_enable = 0x0000007b`，继续运行，触发非法指令异常。

<img src="../images/T2-illegal-epc.png" alt="非法指令异常进入处理函数及帧 EPC 更新" style="zoom:67%;" />

<p align="center">图 2-5　非法指令：原因码与 EPC 增加 4 字节</p>

- 进入 `exception_handler` 时，`cause = 0x2`，对应非法指令异常。原始 `epc = 0xffffffffc020082e`，`x/i $sepc` 显示该地址的编码为 `0x7b`，与临时写入的指令一致。
- `0x0000007b` 的低两位是 `11`，本实验中按 4 字节指令处理。停在 `__trapret` 时，帧中 `epc = 0xffffffffc0200832`，恰好是原地址加 4，说明处理函数已跳过这条非法指令。

<img src="../images/T2-illegal-ret-pc.png" alt="非法指令测试后续 Trap 返回到 kern_init 空闲循环" style="zoom:67%;" />

<p align="center">图 2-6　非法指令测试后续运行：Trap 返回到内核空闲循环</p>

- 这张图停在同一条 `sret`，但此时 `$sepc = 0xffffffffc020009c`（`kern_init+72`），指向 `while (1)` 编译成的自跳转指令 `j`。单步执行 `sret` 后，`$pc` 也等于这个地址，程序回到空闲循环。
- 这里记录的是后续 Trap 的返回过程；非法指令的帧 EPC 更新由图 2-5 验证。

两组结果说明：异常处理函数负责按指令长度推进帧中的 EPC，`RESTORE_ALL` 再把它写回 `sepc`，由 `sret` 使用这个地址返回。压缩断点的截图完整记录了这条路径，非法指令的截图记录了帧 EPC 增加 4 字节以及后续程序继续运行的情况。

### 验证结论

本模块完成以下验证：

| 项目 | 结果 |
|---|---|
| GCC 10.2.0、原编译选项构建 | 通过 |
| QEMU 4.1.1 / OpenSBI v0.4 日常运行 | 十次输出后正常关机，退出码 0 |
| 原始 `make grade` | 100/100 |
| 压缩断点 GDB 验证 | 帧 EPC、sepc、返回 PC 均为 `0xffffffffc0200830`，比原地址增加 2 字节 |
| 非法指令 GDB 验证 | 帧 EPC 从 `0xffffffffc020082e` 增至 `0xffffffffc0200832`（增加 4 字节）；后续 Trap 返回到内核空闲循环 |
