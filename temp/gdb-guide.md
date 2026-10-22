# 双重 GDB 调试 QEMU 地址翻译：入门教程

对应指导书 lab2「分支任务：gdb 调试页表查询过程」。

**读者假设**：学过页表和 TLB 的原理，但**没读过 QEMU 源码，也基本没用过 gdb**。所以本教程分三部分：

1. **背景知识**：QEMU 是怎么“模拟”一颗 CPU 的，一次访存会经过 QEMU 里的哪些函数。不懂这一部分，后面看到的函数名都是天书。
2. **gdb 速成**：只讲本教程用到的十几条命令，以及怎么读 gdb 的输出。
3. **实验**：每一步都写明 **目的 → 输入 → 输出 → 解释**。

下文所有命令和输出都在统一环境（WSL2 Ubuntu 24.04、QEMU 4.1.1 `--enable-debug`、lab2 分支提交 c9318e8 的代码）中实际跑过。代码改动后，地址和行号可能会变，第 3.3 节说明了怎么核对。

---

# 第一部分 背景知识

## 1.1 我们要看的是什么

组成原理课里，CPU 执行一条访存指令（例如 `sd ra,8(sp)`）时，硬件 MMU 要做这几件事：

```
虚拟地址 VA ──► 查 TLB ──命中──► 物理地址 PA ──► 访问内存
                  │
                 未命中
                  ▼
           按 satp 找到根页表，逐级查页表（Sv39 最多三级）
                  │
                  ▼
           得到 PA，同时把“VA 页 → PA 页”写进 TLB
```

在我们的实验里，“硬件”就是 QEMU 这个程序。上面每一步都对应 QEMU 里的一段 C 代码。**本实验就是用 gdb 去看这些 C 代码一步步执行的过程。**

## 1.2 QEMU 怎么模拟一颗 CPU：翻译块（TB）

最直观的模拟方式是“解释执行”：取一条 RISC-V 指令，用 `switch` 判断是什么指令，再用 C 代码模拟它的效果，然后取下一条。这样太慢了。

QEMU 用的是 **TCG（Tiny Code Generator）动态翻译**：

1. 从当前 pc 开始，把一小段 RISC-V 指令（一直到跳转、CSR 写入这类指令为止）**翻译成一段等价的 x86 机器码**。这一小段叫一个 **翻译块（Translation Block，TB）**。
2. 翻译出来的 x86 代码放在一块叫 **`code_gen_buffer`** 的内存里，QEMU 直接跳过去执行。这段代码是 QEMU 运行时生成的，**没有对应的 C 源码**，所以 gdb 的调用栈里只显示 `code_gen_buffer ()`。
3. 一个 TB 执行完，再查找或翻译下一个 TB。翻译过的 TB 会缓存起来，下次执行到同一段代码时直接复用。

有些操作很难直接生成 x86 指令（比如“这次访存没命中 TLB，要去查页表”），生成的代码就会**调用 QEMU 里的一个 C 函数**来处理。这类函数叫 **helper 函数**，比如 `helper_le_stq_mmu`，意思是“小端（le）、存储（st）、8 字节（q）、要走 MMU”。我们的断点都打在这些 C 函数上。

## 1.3 QEMU 的软件 TLB

QEMU 也有一个 TLB，但它是一个**普通的 C 数组**，叫软件 TLB（softmmu TLB）。每一项（`CPUTLBEntry`）主要存这几个字段：

| 字段 | 含义 |
| :--- | :--- |
| `addr_read` | 允许读的那一页的**客户机虚拟地址**（页对齐）；不允许读时为 -1 |
| `addr_write` | 同上，对应写 |
| `addr_code` | 同上，对应取指 |
| `addend` | 一个偏移量：**客户机虚拟地址 + addend = QEMU 进程里真正存这个字节的指针** |

“客户机”指被模拟的那台 RISC-V 机器（ucore 运行在它上面），“宿主机”指运行 QEMU 的这台电脑。模拟出来的 128 MB 内存，其实就是 QEMU 用 `malloc`（准确地说是 mmap）申请的一大块宿主机内存。所以 QEMU 的 TLB 一步到位，直接从“客户机虚拟地址”映射到“宿主机指针”，中间的客户机物理地址只在填表时算一次。

查表的方法：**下标 = 虚拟页号的低几位**（直接映射）。本实验中 S 态的表有 64 项，所以下标就是虚拟页号的低 6 位。

另外还有一个 8 项的 **victim TLB**：主表里的一项被别的页挤掉时，旧的那项先存进这里，下次再用到可以换回来，不必重新查页表。

## 1.4 一次访存在 QEMU 里经过哪些函数

先记住下面这张“地图”，后面所有实验都是在其中某个位置打断点：

```
            取指（要执行新的代码时）              读/写数据（load/store 指令）
                     │                                   │
                     ▼                                   ▼
    get_page_addr_code()                  生成的 x86 代码里直接查 TLB（快速路径）
    cputlb.c:1023                                 │命中：直接访存，不调用任何 C 函数
    查 TLB，没命中 ──┐                            │没命中
                     │                            ▼
                     │                  helper_le_stq_mmu() 等 → store_helper() / load_helper()
                     │                  cputlb.c:1501 / 1248
                     │                  用 C 再查一次 TLB 和 victim TLB，都没命中 ──┐
                     ▼                                                             │
                tlb_fill()  cputlb.c:868   ◄───────────────────────────────────────┘
                “TLB 缺失了，请当前 CPU 架构自己去填”
                     │  通过函数指针调用 RISC-V 的实现
                     ▼
                riscv_cpu_tlb_fill()  target/riscv/cpu_helper.c:435
                     │  ① 翻译         ② 成功就写 TLB，失败就抛缺页异常
                     ▼                        │
                get_physical_address()        ▼
                cpu_helper.c:155        tlb_set_page() → tlb_set_page_with_attrs()  cputlb.c:697
                ★ 查页表就在这里 ★       把“虚拟页 → 物理页（+宿主机指针）”写进 TLB
```

各函数的作用，用一句话概括：

| 函数 | 所在文件 | 一句话说明 |
| :--- | :--- | :--- |
| `get_page_addr_code` | accel/tcg/cputlb.c | 要翻译一个新的 TB 前，先把它的 pc 翻译成物理地址（TB 是按物理地址缓存的） |
| `store_helper` / `load_helper` | accel/tcg/cputlb.c | 数据访存的“慢速路径”：快速路径没命中才进来 |
| `victim_tlb_hit` | accel/tcg/cputlb.c | 在 8 项 victim TLB 里找，找到就和主表交换 |
| `tlb_fill` | accel/tcg/cputlb.c | 通用代码，不认识 RISC-V，只负责转调当前架构的填表函数 |
| `riscv_cpu_tlb_fill` | target/riscv/cpu_helper.c | RISC-V 的填表函数：调用翻译函数，再写 TLB 或抛异常 |
| `get_physical_address` | target/riscv/cpu_helper.c | **按 RISC-V 规范查页表**，就是我们在课上学的 Sv39 三级查表 |
| `tlb_set_page_with_attrs` | accel/tcg/cputlb.c | 把翻译结果写进 TLB 数组 |

`accel/tcg/` 下面是与架构无关的通用代码（x86、ARM、RISC-V 都用它），`target/riscv/` 下面是 RISC-V 专用的代码。

## 1.5 反复出现的参数

断点停下时，gdb 会打印函数的参数。这几个参数在每个实验里都会出现：

| 参数 | 含义 | 本实验中常见的值 |
| :--- | :--- | :--- |
| `env` | 指向被模拟的 CPU 的全部状态（各寄存器、CSR 等），类型是 `CPURISCVState *`。例如 `env->satp` 就是 satp 寄存器，`env->pc` 是 pc | 一个宿主机指针，不用管它的值 |
| `cs` / `cpu` | 也指向这颗 CPU，只是类型是通用的 `CPUState *` | 同上 |
| `addr` / `address` | 要访问的**客户机虚拟地址** | 如 `0xffffffffc02000d6` |
| `access_type` | 访问类型：0 = 读，1 = 写，2 = 取指 | |
| `mmu_idx` | 用哪个特权级的 TLB：0 = U 态，1 = S 态，3 = M 态 | ucore 运行在 S 态，所以是 1 |
| `size` | 访问几个字节 | `sd` 是 8 |
| `retaddr` | 生成的 x86 代码中的返回地址；为 0 表示不是从生成的代码中调用的 | |

---

# 第二部分 gdb 速成

## 2.1 两个 gdb 分别在调试谁

```
 终端 3：riscv64-unknown-elf-gdb ──网络端口 1234──┐
         调试 ucore（RISC-V 程序）                  │  QEMU 内置的 gdbstub 负责应答
                                                   ▼
 终端 1：qemu-system-riscv64 进程 —— 一个普通的 x86 程序，里面模拟了一颗 RISC-V CPU
                                                   ▲
 终端 2：gdb（普通的 x86 gdb）──attach（接管一个正在运行的进程）──┘
         调试 QEMU（C 程序）
```

* **终端 2 的 gdb**：调试对象是 QEMU 的 C 源码。断点打在 QEMU 的函数上（如 `get_physical_address`），能看到的变量也是 QEMU 的 C 变量。
* **终端 3 的 gdb**：调试对象是 ucore，用法和 lab1 一样。
* **两者是嵌套关系**：终端 2 停在断点上时，整个 QEMU 进程都暂停了，被模拟的 CPU 和应答终端 3 的 gdbstub 也都停了，所以终端 3 会“卡住”不动。等终端 2 执行 `continue`，终端 3 才会恢复。

## 2.2 本教程用到的 gdb 命令

| 命令 | 简写 | 作用 | 例子 |
| :--- | :--- | :--- | :--- |
| `attach <PID>` / `gdb -p <PID>` | | 接管一个正在运行的进程，接管后该进程暂停 | `gdb -q -p 527` |
| `break <位置>` | `b` | 设断点，程序执行到这里时停下。位置可以是函数名、`文件:行号`，或 `*地址` | `b get_physical_address`<br>`b cpu_helper.c:254`<br>`b *0xffffffffc02000ec` |
| `break <位置> if <条件>` | | **条件断点**：执行到这里，并且条件成立时才停下 | `b get_physical_address if addr == 0xffffffffc02000d6` |
| `continue` | `c` | 继续运行，直到下一个断点 | |
| `next` | `n` | 执行当前这一行 C 代码。遇到函数调用时，把整个函数执行完，不进去 | `next 7` 表示执行 7 次 |
| `step` | `s` | 同上，但遇到函数调用时会进入函数内部 | |
| `finish` | | 把当前函数执行完，回到调用它的地方，并打印返回值 | |
| `si` | | 执行**一条机器指令**（终端 3 中用于单步执行 RISC-V 指令） | |
| `backtrace` | `bt` | 打印调用栈：当前函数是谁调用的，调用它的函数又是谁调用的…… | `bt 4` 只显示最内层 4 层 |
| `print <表达式>` | `p` | 打印变量或表达式的值，十进制显示 | `p mmu_idx` |
| `p/x <表达式>` | | 同上，十六进制显示 | `p/x env->satp` |
| `x/<n><格式> <地址>` | | 查看内存。`x/i` 按指令反汇编，`x/gx` 按 8 字节十六进制显示 | `x/i $pc` |
| `display/x <表达式>` | | 每次停下都自动打印这个表达式 | `display/x base` |
| `delete` | `d` | 删除全部断点（`delete 2` 只删除 2 号断点） | |
| `info breakpoints` | `i b` | 列出所有断点及其编号 | |
| `ignore <编号> <次数>` | | 让某个断点前几次命中时不停下 | `ignore 1 1` |
| `commands <编号>` … `end` | | 设置断点命中后自动执行的命令，常用来“打印一行就继续”，不必手动操作 | 见实验五 |
| `handle SIGPIPE nostop noprint` | | 收到 SIGPIPE 信号时不停下也不提示（原因见第 4 节） | |
| `set $i = <表达式>` | | 定义一个 gdb 自己的临时变量 | `set $i = 4` |
| `detach` / `quit` | `q` | 放开被调试的进程 / 退出 gdb | |

## 2.3 怎么读 gdb 的输出

**断点命中时**，gdb 会打印一行：

```
Thread 2 "qemu-system-ris" hit Breakpoint 1, get_physical_address (env=0x5654eb54a080, physical=0x7f0e951061f0,
    prot=0x7f0e951061e4, addr=18446744072637907158, access_type=2, mmu_idx=1) at .../target/riscv/cpu_helper.c:158
158	{
```

逐段看：

* `Thread 2 "qemu-system-ris"`：QEMU 是多线程程序，模拟 CPU 的是其中一个线程。
* `hit Breakpoint 1`：命中了 1 号断点。
* `get_physical_address (...)`：停在这个函数里，括号里是**这次调用的参数值**。
* `at .../cpu_helper.c:158`：停在这个文件的第 158 行。
* 最后一行 `158 {` 是**即将执行**（还没执行）的那一行源码。

**gdb 默认用十进制显示整数。** `addr=18446744072637907158` 就是十六进制的 `0xffffffffc02000d6`。想看十六进制，就执行 `p/x addr`。

**`print` 的结果**形如 `$3 = 2`：`$3` 是 gdb 给这个结果的编号，后面可以用 `$3` 引用它；等号后面才是值。

**调用栈 `bt`** 的每一行是一层函数调用：`#0` 是当前所在的函数，`#1` 是调用 `#0` 的函数，依此类推。**从下往上读**就是程序的执行顺序。

---

# 第三部分 准备工作（只需做一次）

## 3.1 确认 QEMU 和 gdb

```bash
export PATH=$HOME/Environment/riscv/riscv-elf-toolchains/bin:$PATH
export QEMU=$HOME/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64
$QEMU --version          # 应显示 QEMU emulator version 4.1.1
file $QEMU               # 输出中应包含 "with debug_info, not stripped"
gdb --version | head -1  # 普通的 x86 gdb；没有就 sudo apt install gdb
```

**目的**：

* `with debug_info` 表示这个 QEMU 带调试信息，gdb 才能把机器码对应回 C 源码的行号，才能按函数名打断点、按变量名查看值。统一环境用的 `setup-qemu-4.1.1.sh` 已经加了 `--enable-debug`，不用另外编译。
* 导出 `QEMU` 环境变量后，`make debug` 就会使用这个 QEMU（Makefile 里写的是 `ifndef QEMU`），**不用修改 Makefile**。

## 3.2 准备代码

```bash
bash .claude/tools/sync-wsl.sh                 # 在仓库根目录执行：把已提交的 code/ 复制到 ~/Code/NKU-OS-LAB/lab2/code
cd ~/Code/NKU-OS-LAB/lab2/code && make
```

## 3.3 查出要观察的地址

```bash
riscv64-unknown-elf-nm bin/kernel | grep -E " (kern_entry|kern_init|boot_page_table_sv39)$"
riscv64-unknown-elf-objdump -d bin/kernel --start-address=0xffffffffc02000d6 | head -20
```

**目的**：条件断点需要一个确切的地址，这些地址随代码变化，所以每次都要查。`nm` 列出符号的地址，`objdump -d` 反汇编。

本次的输出（只保留用到的部分）：

```
ffffffffc0200000 T kern_entry
ffffffffc02000d6 T kern_init
ffffffffc0205000 D boot_page_table_sv39

ffffffffc02000e6:  1141       addi  sp,sp,-16
ffffffffc02000ec:  e406       sd    ra,8(sp)
```

**解释**：

* `kern_init = 0xffffffffc02000d6`：`entry.S` 打开分页后，最后一条 `jr t0` 跳到这里。所以**开启分页后的第一次取指一定访问这个地址**，实验一就用它。
* `sd ra,8(sp)` 位于 `0xffffffffc02000ec`。上一条指令执行完后 `sp = 0xffffffffc0204ff0`，所以这条指令会**写地址 `0xffffffffc0204ff8`**，实验三用它演示数据访存。
* `boot_page_table_sv39` 在虚拟地址 `0xffffffffc0205000`，即物理地址 `0x80205000`，后面会在 satp 里看到它。

你查到的地址如果不同，后面的命令照着替换即可。

## 3.4 开三个终端

三个终端都 `cd ~/Code/NKU-OS-LAB/lab2/code`，并且都执行 3.1 中的两条 `export`。可以用 tmux 分栏，也可以开三个 Windows Terminal 标签页。

---

# 第四部分 每个实验都一样的启动流程

后面每个实验开始前，都按这个顺序操作。**顺序不能乱。**

**① 终端 1：启动 QEMU**

```bash
make debug
```

`make debug` 比 `make qemu` 多了 `-s -S` 两个参数：`-s` 表示在 1234 端口等待 gdb 连接，`-S` 表示启动后先不执行任何指令。所以这时终端 1 什么都不输出，这是正常的。

**② 终端 2：接管 QEMU 进程**

```bash
pidof qemu-system-riscv64          # 查 QEMU 的进程号，例如输出 527
gdb -q -p 527                      # -q 表示不打印版本信息；-p 后面跟进程号
```

attach 成功后会打印一些 `[New LWP ...]`（QEMU 的各个线程）和一行 `__GI_ppoll (...)`，表示 QEMU 正停在一个等待事件的系统调用里，都不用管。**此时整个 QEMU 进程被暂停了。**

> 在 WSL2 中，同一个用户直接执行 `gdb -p` 就可以 attach。在原生 Ubuntu 上，如果报 `ptrace: Operation not permitted`，改用 `sudo gdb -q -p <PID>`。

**③ 终端 2：做准备，设断点，然后让 QEMU 继续运行**

```gdb
(gdb) handle SIGPIPE nostop noprint
(gdb) （按各个实验的说明设置断点）
(gdb) continue
```

`handle SIGPIPE nostop noprint` 的目的：QEMU 运行时会收到 SIGPIPE 信号（例如网络连接的另一端断开时），gdb 默认收到任何信号都会停下来，看起来就像莫名其妙地停住了。这条命令让 gdb 忽略它。

`continue` 之后 QEMU 进程恢复运行，但因为有 `-S`，被模拟的 CPU 仍然没有开始执行。

**④ 终端 3：连接 ucore 并开始执行**

```bash
make gdb                  # 相当于 riscv64-unknown-elf-gdb 加载 bin/kernel 并连接 localhost:1234
(gdb) continue            # 被模拟的 CPU 开始执行：OpenSBI → ucore
```

**为什么 ③ 必须在 ④ 之前**：② 之后 QEMU 整个进程被冻结，负责应答终端 3 的 gdbstub 也被冻结了，这时执行 `make gdb` 会一直连不上。

**⑤ 终端 2 的断点命中** → 在终端 2 查看、单步。此时终端 3 卡住不动，这是正常的。

**⑥ 结束**：终端 2 执行 `detach`、`quit`；终端 3 执行 `quit`；终端 1 先按 `Ctrl+A`，松开后再按 `X`，退出 QEMU。如果下次 `make gdb` 报 `vMustReplyEmpty: timeout`，说明有残留的 QEMU 进程，执行 `pkill -f qemu-system-riscv64` 清理掉即可。

---

# 第五部分 实验

## 实验一：看一次取指是怎样被翻译的（对应调试要求 1）

**思路**：指导书「思考」一节提到，不必先在 ucore 里找访存指令，只要选一个**一定会被访问**的地址，在 QEMU 的翻译函数上打条件断点即可。`kern_init` 的地址就是这样一个地址。

### 步骤 1：设条件断点

**终端 2**（按第四部分走到第 ③ 步）：

```gdb
(gdb) handle SIGPIPE nostop noprint
(gdb) break get_physical_address if addr == 0xffffffffc02000d6
(gdb) continue
```

**目的**：`get_physical_address` 是查页表的函数（见 1.4 节），所有需要翻译的访存都会进来，OpenSBI 阶段就要调用几千次。加上 `if addr == ...` 后，只有翻译 `kern_init` 这个地址时才会停下。

然后 **终端 3**：`make gdb`，再输入 `continue`。

### 步骤 2：断点命中，看参数

```
Thread 2 "qemu-system-ris" hit Breakpoint 1, get_physical_address (env=0x5654eb54a080, physical=0x7f0e951061f0,
    prot=0x7f0e951061e4, addr=18446744072637907158, access_type=2, mmu_idx=1) at .../target/riscv/cpu_helper.c:158
```

**解释**（参数含义见 1.5 节）：

* `addr=18446744072637907158` 即 `0xffffffffc02000d6`，正是 `kern_init`。
* `access_type=2`：这是一次**取指**。CPU 要执行 `kern_init` 的第一条指令，得先把这条指令从内存里取出来。
* `mmu_idx=1`：S 态。
* `physical`、`prot` 是两个指针，这个函数会把算出来的物理地址和权限写到它们指向的地方。这是 C 语言“通过指针返回多个结果”的常见写法。

### 步骤 3：看调用栈——这次翻译是谁发起的

```gdb
(gdb) bt
```

```
#0  get_physical_address (... addr=18446744072637907158, access_type=2, mmu_idx=1) at target/riscv/cpu_helper.c:158
#1  riscv_cpu_tlb_fill (... access_type=MMU_INST_FETCH, mmu_idx=1, probe=false, retaddr=0) at target/riscv/cpu_helper.c:451
#2  tlb_fill (... access_type=MMU_INST_FETCH, mmu_idx=1, retaddr=0) at accel/tcg/cputlb.c:878
#3  get_page_addr_code (env=..., addr=18446744072637907158) at accel/tcg/cputlb.c:1032
#4  tb_htable_lookup (cpu=..., pc=18446744072637907158, ...) at accel/tcg/cpu-exec.c:339
#5  tb_lookup__cpu_state (...) at include/exec/tb-lookup.h:43
#6  helper_lookup_tb_ptr (env=...) at accel/tcg/tcg-runtime.c:154
#7  code_gen_buffer ()
#8  cpu_tb_exec (...) at accel/tcg/cpu-exec.c:173
#9  cpu_loop_exec_tb (...) at accel/tcg/cpu-exec.c:621
#10 cpu_exec (cpu=...) at accel/tcg/cpu-exec.c:732
#11 tcg_cpu_exec (...) at cpus.c:1435
#12 qemu_tcg_cpu_thread_fn (...) at cpus.c:1743
```

**目的**：看清楚这次查页表是从哪里一路调用过来的。这就是调试要求 1 中的“关键调用路径”。

**从下往上读**：

| 层 | 函数 | 在做什么 |
| :--- | :--- | :--- |
| #12～#8 | `qemu_tcg_cpu_thread_fn` → `cpu_exec` → `cpu_tb_exec` | 模拟 CPU 的线程在主循环里执行一个个 TB（1.2 节） |
| #7 | `code_gen_buffer` | 正在执行上一个 TB 翻译出来的 x86 代码。那个 TB 的最后一条指令是 `entry.S` 的 `jr t0` |
| #6～#4 | `helper_lookup_tb_ptr` → `tb_htable_lookup` | `jr t0` 的跳转目标要到运行时才知道，于是调用 helper 去找“下一个 TB 在哪”。TB 是按**物理地址**缓存的，所以要先把目标 pc 翻译成物理地址 |
| #3 | `get_page_addr_code` | 取指方向的 TLB 查找：TLB 里没有 `0xffffffffc0200000` 这一页（刚打开分页，从没访问过），未命中 |
| #2 | `tlb_fill` | 通用代码处理 TLB 缺失，转调 RISC-V 的填表函数 |
| #1 | `riscv_cpu_tlb_fill` | RISC-V 的填表函数，第 451 行调用了翻译函数 |
| #0 | `get_physical_address` | 开始查页表 |

### 步骤 4：看此时 CPU 的状态

```gdb
(gdb) p/x env->satp
$1 = 0x8000000000080205
(gdb) p/x env->pc
$2 = 0xffffffffc02000d6
```

**目的**：确认分页已经打开，并找到根页表在哪里。

**解释**：satp 的最高 4 位 `8` 表示 Sv39 模式，低 44 位 `0x80205` 是根页表的物理页号。乘以 4096（左移 12 位）得到 `0x80205000`，正是 3.3 节查到的 `boot_page_table_sv39` 的物理地址。

### 步骤 5：跳过查表过程，直接看结果

查表的细节放到实验二。这里先看结果：

```gdb
(gdb) finish
```

```
Run till exit from #0 get_physical_address ...
0x00005654d79380de in riscv_cpu_tlb_fill (...) at .../cpu_helper.c:451
451	    ret = get_physical_address(env, &pa, &prot, address, access_type, mmu_idx);
Value returned is $14 = 0
```

**目的**：`finish` 把 `get_physical_address` 执行完，回到调用它的 `riscv_cpu_tlb_fill`。

**解释**：返回值 0 就是 `TRANSLATE_SUCCESS`，表示翻译成功。接下来 `riscv_cpu_tlb_fill` 会把结果写进 TLB，我们在写 TLB 的函数上再停一次：

```gdb
(gdb) break tlb_set_page_with_attrs
(gdb) continue
```

```
Breakpoint 4, tlb_set_page_with_attrs (cpu=..., vaddr=18446744072637906944, paddr=2149580800, attrs=...,
    prot=7, mmu_idx=1, size=4096) at accel/tcg/cputlb.c:700
(gdb) p/x vaddr
$15 = 0xffffffffc0200000
(gdb) p/x paddr
$16 = 0x80200000
```

**解释**：

* `vaddr = 0xffffffffc0200000`：要写进 TLB 的虚拟页（`kern_init` 所在页的页首）。
* `paddr = 0x80200000`：翻译得到的物理页。
* `prot = 7`：可读（1）、可写（2）、可执行（4）三者相加。
* `size = 4096`：QEMU 按 4 KiB 一页写 TLB（实验二会说明这一点为什么值得注意）。

**结论**：虚拟地址 `0xffffffffc02000d6` → 所在虚拟页 `0xffffffffc0200000` → 物理页 `0x80200000` → 加上页内偏移 `0xd6` → **物理地址 `0x802000d6`**。

### 步骤 6：清理

```gdb
(gdb) delete
(gdb) continue
```

删掉所有断点再继续运行，否则后面每次写 TLB 都会停下来。

### 写报告时：路径上的关键分支

调试要求 1 还要求说明“路径上的关键分支语句”。下面这些 `if` 决定了一次访存走哪条路，可以打开源码对照：

| 位置 | 代码 | 含义 |
| :--- | :--- | :--- |
| cputlb.c:1030 | `if (unlikely(!tlb_hit(entry->addr_code, addr)))` | 取指时主 TLB 是否命中 |
| cputlb.c:1031 | `if (!VICTIM_TLB_HIT(addr_code, addr))` | 主 TLB 未命中时再查 victim TLB；仍未命中才调用 `tlb_fill` |
| cpu_helper.c:165 | `if (mode == PRV_M && access_type != MMU_INST_FETCH)` | M 态的数据访存，如果设置了 `mstatus.MPRV`，就按 `MPP` 指定的特权级来翻译 |
| cpu_helper.c:171 | `if (mode == PRV_M \|\| !riscv_feature(env, RISCV_FEATURE_MMU))` | M 态不翻译：物理地址 = 虚拟地址 |
| cpu_helper.c:187 | `switch (vm)` | 根据 satp 的模式决定页表级数：Sv39 → 3 级；Bare（未开启分页）→ 物理地址 = 虚拟地址，直接返回 |
| cpu_helper.c:227 | `if (masked_msbs != 0 && masked_msbs != mask)` | Sv39 要求虚拟地址的第 63～39 位都与第 38 位相同，否则直接失败 |
| cpu_helper.c:256 起 | `if (!(pte & PTE_V))` 等一串 `else if` | 逐项检查页表项，见实验二 |
| cpu_helper.c:471 | `if (ret == TRANSLATE_SUCCESS)` | 成功就写 TLB；失败就向 ucore 抛缺页异常 |

---

## 实验二：单步看页表遍历（对应调试要求 2）

### 先读懂要调试的代码

`get_physical_address` 的核心是一个 `for` 循环，每轮查一级页表（`cpu_helper.c:237`）。去掉次要的部分后是这样：

```c
// 循环之前：base = 根页表的物理地址（来自 satp）；levels = 3；ptidxbits = 9；ptesize = 8
//          ptshift = (levels - 1) * ptidxbits = 18
for (i = 0; i < levels; i++, ptshift -= ptidxbits) {
    // ① 从虚拟地址中取出这一级的 9 位索引
    //    i=0 时取 VA[38:30]，i=1 时取 VA[29:21]，i=2 时取 VA[20:12]
    target_ulong idx = (addr >> (PGSHIFT + ptshift)) & ((1 << ptidxbits) - 1);

    // ② 这一级页表项的物理地址 = 页表基址 + 索引 × 8 字节
    target_ulong pte_addr = base + idx * ptesize;

    // ③ 按物理地址从模拟的内存中读出 8 字节，就是页表项
    target_ulong pte = ldq_phys(cs->as, pte_addr);

    // ④ 页表项的第 10 位往上是物理页号 PPN
    target_ulong ppn = pte >> PTE_PPN_SHIFT;

    if (!(pte & PTE_V)) {
        return TRANSLATE_FAIL;                    // V=0：无效项 → 缺页
    } else if (!(pte & (PTE_R | PTE_W | PTE_X))) {
        base = ppn << PGSHIFT;                    // R、W、X 全为 0：不是叶子，指向下一级页表，继续循环
    } else if ( ...一系列权限检查... ) {
        return TRANSLATE_FAIL;                    // 例如要写但 W=0、U 位与当前特权级不符、大页没有对齐……
    } else {
        // 是叶子，并且检查都通过了：
        // 必要时设置 A/D 位；算出物理地址写到 *physical；算出权限写到 *prot
        return TRANSLATE_SUCCESS;
    }
}
```

这和课上讲的 Sv39 查表过程一一对应。下面用 gdb 验证每个变量的值。

### 步骤 1：停在读出页表项之后

先按实验一的步骤 1～2 停在 `get_physical_address` 的入口（同一个条件断点）。然后：

```gdb
(gdb) break cpu_helper.c:254
(gdb) continue
```

**目的**：第 254 行是 `target_ulong ppn = pte >> PTE_PPN_SHIFT;`，停在这一行时，上一行已经把页表项读出来了。这样就能一次看到 ①②③ 三步的结果。这个断点不用加条件，因为它在 `get_physical_address` 内部，而我们已经停在要观察的那次调用里了，`continue` 后紧接着就会停在这里。

```
Breakpoint 2, get_physical_address (... addr=18446744072637907158, access_type=2, mmu_idx=1) at .../cpu_helper.c:254
254	        target_ulong ppn = pte >> PTE_PPN_SHIFT;
```

### 步骤 2：逐个查看变量

```gdb
(gdb) p i
$5 = 0
(gdb) p/x idx
$6 = 0x1ff
(gdb) p/x pte_addr
$7 = 0x80205ff8
(gdb) p/x pte
$8 = 0x200000cf
(gdb) p/x base
$9 = 0x80205000
```

**逐个解释**：

| 变量 | 值 | 怎么来的 |
| :--- | :--- | :--- |
| `i` | 0 | 正在查第一级（根页表） |
| `base` | `0x80205000` | 根页表的物理地址，就是 satp 中的 `0x80205` 左移 12 位 |
| `idx` | `0x1ff`（511） | `0xffffffffc02000d6` 右移 30 位后取低 9 位：`0x...ff` 的低 9 位全是 1，即 511 |
| `pte_addr` | `0x80205ff8` | `0x80205000 + 511 × 8`，即根页表的最后一项 |
| `pte` | `0x200000cf` | 从这个地址读出的 8 字节。对照 `entry.S`：`.quad (0x80000 << 10) \| 0xcf`，`0x80000 << 10 = 0x20000000`，再或上 `0xcf`，完全一致 |

把 `pte` 的低 8 位 `0xcf` = `1100 1111` 按位展开：

| 位 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 名称 | D | A | G | U | X | W | R | V |
| 值 | 1 | 1 | 0 | 0 | 1 | 1 | 1 | 1 |

V=1 表示有效；R、W、X 都是 1，说明**这一项是叶子**。也就是说，**第一级就找到了最终的映射**，不会再查第二、三级。第一级的叶子覆盖 2^30 字节，这就是 1 GiB 的“大页”。

### 步骤 3：看大页的物理地址是怎么拼出来的

```gdb
(gdb) break cpu_helper.c:334
(gdb) continue
```

```
334	            *physical = (ppn | (vpn & ((1L << ptshift) - 1))) << PGSHIFT;
```

**目的**：第 334 行就是计算物理地址的那一行。停在这里时，前面那串权限检查已经全部通过了。

```gdb
(gdb) p/x ppn
$10 = 0x80000
(gdb) p ptshift
$11 = 18
(gdb) p/x vpn
$12 = 0xffffffffc0200
(gdb) next
(gdb) p/x *physical
$13 = 0x80200000
```

**目的**：先看参与计算的三个量，再用 `next` 执行这一行，看结果。

**解释**：

* `ppn = 0x80000`：页表项给出的物理页号，即 1 GiB 大页的起点 `0x80000000`。
* `vpn = 0xffffffffc0200`：虚拟页号（虚拟地址右移 12 位）。
* `ptshift = 18`：大页内部还有 18 位页号（30 − 12）没有被页表“用掉”，它们要从虚拟地址里原样搬过来。
* `vpn & ((1 << 18) - 1)` = `0x00200`：取虚拟页号的低 18 位。
* `ppn | 0x00200` = `0x80200`，左移 12 位 → `0x80200000`。

源码注释说这是 “make a fake leaf PTE for the TLB's benefit”：**QEMU 的 TLB 每项只管 4 KiB**，所以即使是 1 GiB 的大页，也只把当前访问的这一个 4 KiB 页写进 TLB（实验一中 `size=4096`）。访问大页里的其他 4 KiB 页时，会重新查一次页表，再写一项。真实 CPU 通常有能直接存大页的 TLB 项，这是两者的一个区别。

**补充**：`0xcf` 中 A 位和 D 位已经是 1，所以代码中“设置 A/D 位”的那个分支（第 295 行起）没有执行任何写操作。lab2 只建了这一个大页，所以看不到三级都走一遍的情况；后续 lab 建立 4 KiB 页的页表后，可以用同样的方法观察 `i = 0, 1, 2` 三轮。

做完后 `delete`、`continue`。

---

## 实验三：从 ucore 侧停在一条访存指令前（指导书的标准流程）

**思路**：实验一、二是“只在 QEMU 中打断点”。这一节按指导书的原始思路，先在**终端 3** 让 ucore 停在 `sd ra,8(sp)` 这条存储指令前，再单步执行它，然后在**终端 2** 看 QEMU 怎么处理这次写操作。

### 步骤 1：终端 2 设条件断点

```gdb
(gdb) handle SIGPIPE nostop noprint
(gdb) break riscv_cpu_tlb_fill if address == 0xffffffffc0204ff8
(gdb) continue
```

**目的**：`0xffffffffc0204ff8` 是 `sd ra,8(sp)` 要写的地址（3.3 节）。这次把断点打在 `riscv_cpu_tlb_fill` 上，因为我们关心的是“谁请求了填 TLB”。在这个函数里，地址参数叫 `address`，不叫 `addr`。

### 步骤 2：终端 3 让 ucore 停在这条指令前

```gdb
$ make gdb
(gdb) b *0xffffffffc02000ec
(gdb) continue
```

```
Breakpoint 1, 0xffffffffc02000ec in kern_init () at kern/init/init.c:30
```

```gdb
(gdb) x/i $pc
=> 0xffffffffc02000ec <kern_init+22>:	sd	ra,8(sp)
(gdb) p/x $sp
$1 = 0xffffffffc0204ff0
(gdb) p/x $ra
$2 = 0x80000a02
```

**目的和解释**：

* `b *0xffffffffc02000ec`：在这条指令的地址上设断点。地址前面要加 `*`，否则 gdb 会把它当成符号名。
* `x/i $pc`：反汇编当前 pc 处的指令，确认停在了 `sd ra,8(sp)`。
* `$sp = 0xffffffffc0204ff0`，所以写入地址 = sp + 8 = `0xffffffffc0204ff8`，与终端 2 的条件一致。
* `$ra = 0x80000a02`：这条指令要写进内存的值（OpenSBI 中的一个返回地址），下面会在 QEMU 中看到它。

### 步骤 3：终端 3 单步执行这条指令

```gdb
(gdb) si
```

执行后终端 3 会卡住。这是因为终端 2 的断点命中，QEMU 被暂停了。

### 步骤 4：终端 2 看调用栈

```
Breakpoint 2, riscv_cpu_tlb_fill (cs=..., address=18446744072637927416, size=8,
    access_type=MMU_DATA_STORE, mmu_idx=1, probe=false, retaddr=139745316372805) at .../cpu_helper.c:438
```

```gdb
(gdb) bt 4
```

```
#0  riscv_cpu_tlb_fill (... access_type=MMU_DATA_STORE ...) at target/riscv/cpu_helper.c:438
#1  tlb_fill (...) at accel/tcg/cputlb.c:878
#2  store_helper (big_endian=false, size=8, ..., val=2147486210, addr=18446744072637927416, ...) at accel/tcg/cputlb.c:1522
#3  helper_le_stq_mmu (...) at accel/tcg/cputlb.c:1672
#4  code_gen_buffer ()
```

**解释**：

* `access_type=MMU_DATA_STORE`、`size=8`：一次 8 字节的写，正是 `sd`。
* `val=2147486210` 即 `0x80000a02`，就是终端 3 中看到的 `ra` 的值，要写进内存。
* **与实验一的调用栈对比**：取指是从 `get_page_addr_code` 进来的；数据访存则是**翻译后的 x86 代码（`code_gen_buffer`）直接调用 helper 函数 `helper_le_stq_mmu`**，再进入 `store_helper`。这就是 1.4 节地图的右半边：生成的代码先在快速路径里查 TLB，没命中才调用 helper。
* `retaddr` 不为 0：它记录了生成代码中的返回位置。如果翻译失败需要抛缺页异常，QEMU 靠它算出是哪条 RISC-V 指令出的错。

### 步骤 5：看写进 TLB 的那一项

```gdb
(gdb) break cpu_helper.c:474
(gdb) continue
```

**目的**：第 474 行是 `riscv_cpu_tlb_fill` 中写完 TLB 之后的 `return true;`。停在这里时 TLB 已经写好了。

```gdb
(gdb) p/x ((RISCVCPU*)cs)->neg.tlb.f[1].mask
$2 = 0x7e0
```

**目的**：查看 S 态（下标 1）TLB 的大小。

**解释**：`cs` 是通用的 CPU 指针，先转换成 RISC-V 的 CPU 类型 `RISCVCPU*`，再取出其中的 TLB 结构 `neg.tlb.f[1]`。不必记住这个写法，照抄即可。`mask = 0x7e0` = `63 << 5`：表中有 64 项，每项 32 字节（2^5）。

```gdb
(gdb) set $i = (address >> 12) & (((RISCVCPU*)cs)->neg.tlb.f[1].mask >> 5)
(gdb) p $i
$3 = 4
```

**目的**：按 QEMU 的规则（`include/exec/cpu_ldst.h:163 tlb_index`）算出这一页放在表中的哪个位置。

**解释**：`address >> 12` 是虚拟页号 `0xffffffffc0204`，与 63 做与运算，得到低 6 位 `4`。所以它在第 4 项。

```gdb
(gdb) p/x ((RISCVCPU*)cs)->neg.tlb.f[1].table[$i]
$4 = {{{addr_read = 0xffffffffc0204000, addr_write = 0xffffffffc0204000,
        addr_code = 0xffffffffc0204000, addend = 0x7f1927e00000}, ...}}
```

**解释**（字段含义见 1.3 节）：

* `addr_read`、`addr_write`、`addr_code` 都是 `0xffffffffc0204000`：这一页可读、可写、可执行（权限来自页表项的 R、W、X）。
* `addend = 0x7f1927e00000`：之后再访问这一页时，`0xffffffffc0204ff8 + addend` 就直接得到 QEMU 进程里对应的宿主机指针。**中间不再经过客户机物理地址**，这是 QEMU 的 TLB 和真实 TLB 最大的不同（真实 TLB 存的是物理页号）。

### 步骤 6：确认写入成功

终端 2：`delete`，`continue`。终端 3 恢复后：

```gdb
(gdb) x/gx 0xffffffffc0204ff8
0xffffffffc0204ff8:	0x0000000080000a02
```

`ra` 的值已经写进了这个地址。

---

## 实验四：查 TLB 的代码（对应调试要求 3）

### 先说清楚：查 TLB 有两条路径

这是最容易困惑的地方。**“TLB 命中”时，你在 C 代码里打的断点是停不下来的。**

1. **快速路径**：QEMU 翻译每条 load/store 指令时，会在生成的 x86 代码里直接插入几条指令：取虚拟页号的低 6 位作为下标 → 读出表项 → 比较页号 → 相等就加上 `addend` 直接访存。生成这几条指令的代码在 `tcg/i386/tcg-target.inc.c:1699 tcg_out_tlb_load`。**命中时整个过程都在生成的代码里完成，不调用任何 C 函数。**
2. **慢速路径**：快速路径比较失败时，才调用 `store_helper` / `load_helper`。这两个函数**用 C 代码再查一遍**，下面是 `load_helper` 中的相关代码（`cputlb.c:1253` 起，`store_helper` 与之相同）：

```c
uintptr_t index = tlb_index(env, mmu_idx, addr);       // 下标 = 虚拟页号 & (表项数 - 1)
CPUTLBEntry *entry = tlb_entry(env, mmu_idx, addr);    // 取出这一项
target_ulong tlb_addr = code_read ? entry->addr_code : entry->addr_read;
...
if (!tlb_hit(tlb_addr, addr)) {                         // 1271 行：这一项存的是不是同一页？
    if (!victim_tlb_hit(env, mmu_idx, index, tlb_off,  // 1272 行：不是，再去 victim TLB 找
                        addr & TARGET_PAGE_MASK)) {
        tlb_fill(env_cpu(env), addr, size,              // 1274 行：还找不到，去查页表
                 access_type, mmu_idx, retaddr);
        ...
```

取指走的是另一个入口 `get_page_addr_code`（`cputlb.c:1023`），逻辑相同：`tlb_hit` → `VICTIM_TLB_HIT` → `tlb_fill`。

### 步骤 1：看一次“主 TLB 未命中”

终端 2：

```gdb
(gdb) break store_helper if addr == 0xffffffffc0204ff8
(gdb) continue
```

终端 3：`make gdb`，`continue`。

```
Breakpoint 2 at 0x5574b1cfb3e6: store_helper. (7 locations)
...
Breakpoint 2.6, store_helper (big_endian=false, size=8, ..., val=2147486210, addr=18446744072637927416, ...) at accel/tcg/cputlb.c:1504
```

**解释**：`(7 locations)` 表示 `store_helper` 是一个内联函数，被编译进了 7 个不同的地方（1 字节、2 字节、4 字节、8 字节……各一份）。gdb 在 7 处都打了断点，这是正常的。命中的是第 6 处（`2.6`）。

```gdb
(gdb) next 7
1519	    if (!tlb_hit(tlb_addr, addr)) {
(gdb) p tlb_hit(tlb_addr, addr)
$4 = false
```

**目的**：`next 7` 连续执行 7 行，走到判断是否命中的那一行，然后**在 gdb 中直接调用 `tlb_hit` 函数**，看它的结果。

**解释**：结果为 `false`，说明主 TLB 中第 4 项存的不是这一页（这一页是第一次被访问）。接下来会进入 `victim_tlb_hit`，再进入 `tlb_fill`，也就是实验三看到的路径。

> gdb 能直接调用 `tlb_hit`、`env_tlb` 这些内联函数，是因为 `--enable-debug` 关闭了编译优化（`-O0`），这些函数被保留了下来。

### 步骤 2：看一次 victim TLB 命中

**背景**：`dtb_init` 读取设备树时，设备树所在的页 `0xffffffffc2200000` 和内核代码页 `0xffffffffc0200000` 的虚拟页号低 6 位都是 0，在主 TLB 里抢**同一个位置（第 0 项）**。两者交替访问时，就会在主 TLB 和 victim TLB 之间来回交换。

终端 2（重新启动一轮，见第四部分）：

```gdb
(gdb) break victim_tlb_hit if mmu_idx == 1 && page == 0xffffffffc2200000
(gdb) ignore 1 1
(gdb) continue
```

**目的**：在 S 态查 victim TLB、并且要找的是设备树这一页时停下。`ignore 1 1` 让 1 号断点第一次命中时不停：第一次访问设备树时 victim TLB 里还没有它，看不到命中。

```
Breakpoint 1, victim_tlb_hit (env=..., mmu_idx=1, index=0, elt_ofs=0, page=18446744072671461376) at accel/tcg/cputlb.c:979
```

```gdb
(gdb) bt 2
#0  victim_tlb_hit (... index=0, elt_ofs=0, page=18446744072671461376) at accel/tcg/cputlb.c:979
#1  load_helper (..., code_read=false, size=4, ..., addr=18446744072671461384, ...) at accel/tcg/cputlb.c:1272
```

**解释**：

* 是 `load_helper` 在第 1272 行调用的，说明这是一次 4 字节的读（`size=4`），主 TLB 没有命中。
* `index=0`：在主 TLB 中的位置是第 0 项。
* `page=18446744072671461376` 即 `0xffffffffc2200000`，设备树所在的页（物理地址 0x82200000）。

```gdb
(gdb) p/x env_tlb(env)->f[1].table[index].addr_read
$2 = 0xffffffffc0200000
```

**目的**：看主 TLB 的第 0 项现在被谁占着。

**解释**：被内核页 `0xffffffffc0200000` 占着，所以主 TLB 没有命中。

```gdb
(gdb) finish
Value returned is $3 = true
(gdb) p/x env_tlb(env)->f[1].table[0].addr_read
$4 = 0xffffffffc2200000
```

**目的**：执行完 `victim_tlb_hit`，看它是否找到了，以及主 TLB 有什么变化。

**解释**：返回 `true`，在 victim TLB 里找到了。再看第 0 项，已经变成了设备树页：`victim_tlb_hit` 把两项**交换**了（代码在 `cputlb.c:996～1006`），内核页进了 victim TLB。这样**省掉了一次查页表**。

### 步骤 3：看一次冲突导致的再次未命中

**问题**：同一页访问过一次之后，后面的访问是不是都在快速路径中命中了？

终端 2（重新启动一轮）：

```gdb
(gdb) break store_helper if addr == 0xffffffffc0204ff8
(gdb) continue                                         # 第一次访问栈页，停下
(gdb) delete
(gdb) break store_helper if (addr >> 12) == 0xffffffffc0204
(gdb) continue                                         # 只要再有对栈页的写进入慢速路径，就停下
```

**目的**：第一个断点停在第一次访问栈页时；然后换成“任何写这一页的操作进入了慢速路径就停”。如果以后的访问都命中快速路径，这个断点就永远不会停。

```
Breakpoint 3.6, store_helper (..., addr=18446744072637927336, ...) at accel/tcg/cputlb.c:1504
(gdb) p/x addr
$3 = 0xffffffffc0204fa8
```

结果它还是停了，写的是同一个栈页上的另一个地址。查一下原因：

```gdb
(gdb) p/x env_tlb(env)->f[1].table[4]
$4 = {{{addr_read = 0xffffffffc0344000, addr_write = 0xffffffffc0344000, addr_code = 0xffffffffc0344000, ...}}}
(gdb) p/x env_tlb(env)->d[1].vtable
（8 项依次为 0xffffffffc0342000、0xffffffffc0340000、0xffffffffc0303000、0xffffffffc0304000、
  0xffffffffc0305000、0xffffffffc0306000、0xffffffffc0341000、0xffffffffc0346000）
```

**解释**：

* 第 4 项已经变成了 `0xffffffffc0344000`（页号低 6 位也是 4），栈页被它**挤掉**了。
* victim TLB 的 8 项也全被 `0xffffffffc03xx000` 这些页占满了，栈页已经不在里面。
* 这些页是 `pages` 数组所在的区域：在两次访问栈页之间，`page_init` 依次遍历了上百个页的 `struct Page`，把 TLB 冲刷了一遍。

所以栈页只能重新走 `tlb_fill` 查页表。这就是“直接映射 + 小容量 victim”结构下典型的**冲突缺失**。

---

## 实验五：未开启分页和开启分页的对比（对应调试要求 4）

**思路**：指导书提示，找一条未开启分页时的访存，和开启分页后的访存比较调用路径。最方便的办法是**把每一次 S 态的 TLB 填充都打印出来，但不停下**，这样整个启动过程一目了然。

### 步骤 1：终端 2 设置“打印一行就继续”的断点

```gdb
(gdb) handle SIGPIPE nostop noprint
(gdb) break riscv_cpu_tlb_fill if mmu_idx == 1
(gdb) commands
> silent
> printf "tlb_fill addr=%#lx type=%d satp=%#lx\n", address, access_type, ((RISCVCPU*)cs)->env.satp
> continue
> end
```

**目的**：

* 断点只在 S 态（`mmu_idx == 1`，即 ucore）填 TLB 时触发，跳过 OpenSBI 阶段（M 态）的几千次调用。
* `commands` … `end` 之间的命令会在断点命中时自动执行：`silent` 表示不打印断点命中的提示，`printf` 打印我们关心的三个值，`continue` 自动继续运行。输入 `commands` 后，gdb 会显示 `>` 提示符，逐行输入即可，最后输入 `end` 结束。
* `%#lx` 表示以 0x 开头的十六进制。

再加两个同样“打印就继续”的断点：

```gdb
(gdb) break get_physical_address if addr == 0x80206000
(gdb) commands
> silent
> printf "  -> get_physical_address addr=%#lx satp=%#lx\n", addr, env->satp
> continue
> end
(gdb) break helper_tlb_flush
(gdb) commands
> silent
> printf "sfence.vma: helper_tlb_flush pc=%#lx\n", env->pc
> continue
> end
(gdb) continue
```

**目的**：第二个断点专门看未开启分页时的一次访存是否也进了翻译函数（`0x80206000` 是 `entry.S` 开头 `sd a0,0(t0)` 写 `boot_hartid` 的物理地址）；第三个断点标出 `sfence.vma` 执行的时刻（这条指令在 QEMU 中由 `helper_tlb_flush` 实现）。

终端 3：`make gdb`，`continue`。

### 步骤 2：看输出

终端 2 中实测输出的前几行：

```
tlb_fill addr=0x80200000 type=2 satp=0
tlb_fill addr=0x80206000 type=1 satp=0
  -> get_physical_address addr=0x80206000 satp=0
sfence.vma: helper_tlb_flush pc=0x80200038
tlb_fill addr=0xffffffffc02000d6 type=2 satp=0x8000000000080205
tlb_fill addr=0xffffffffc0204ff8 type=1 satp=0x8000000000080205
tlb_fill addr=0xffffffffc02018be type=2 satp=0x8000000000080205
tlb_fill addr=0xffffffffc0206018 type=1 satp=0x8000000000080205
...
```

**逐行解释**：

| 输出 | 对应 ucore 的哪一步 |
| :--- | :--- |
| `addr=0x80200000 type=2 satp=0` | OpenSBI 跳到 `kern_entry`，第一次取指。satp=0，即未开启分页（Bare 模式），地址是物理地址 |
| `addr=0x80206000 type=1 satp=0` | `entry.S` 中的 `sd a0,0(t0)`，把 hartid 写进 `boot_hartid` |
| `-> get_physical_address addr=0x80206000` | **未开启分页时，同样调用了翻译函数**，只是它在 `switch (vm)` 的 `case VM_1_10_MBARE` 分支直接返回“物理地址 = 虚拟地址” |
| `sfence.vma ... pc=0x80200038` | `entry.S` 写完 satp 后执行 `sfence.vma`，清空 TLB |
| `addr=0xffffffffc02000d6 type=2 satp=0x80...` | `jr t0` 跳到 `kern_init`，**第一次按 Sv39 翻译**（实验一看的就是这一次） |
| `addr=0xffffffffc0204ff8 type=1` | `kern_init` 中的 `sd ra,8(sp)`（实验三） |
| `addr=0xffffffffc02018be type=2` | 调用 `memset`，取指到了另一页代码 |
| `addr=0xffffffffc0206018 type=1` | `memset` 开始清零 `.bss` |

### 结论：QEMU 的 TLB 与真实 TLB 在逻辑上的区别

1. **未开启分页时也要查 TLB。** 真实 CPU 在 Bare 模式下根本不经过 TLB。而 QEMU 的 TLB 本质上是“客户机地址 → 宿主机指针”的缓存，无论是否开启分页都要用它，否则每次访存都要在 QEMU 的内存布局里搜索一遍这个地址属于哪块内存。所以在上面的输出中，satp=0 时照样有 `tlb_fill`。M 态（OpenSBI）也一样，把条件改成 `mmu_idx == 3` 可以看到成百上千次调用。
2. **表项内容不同。** 真实 TLB 存“虚拟页号 → 物理页号 + 权限位”。QEMU 为读、写、取指各存一个页号（不允许的那种访问对应 -1），再加一个直接得到宿主机指针的 `addend`（实验三步骤 5）。
3. **按特权级分表。** QEMU 为 U、S、M 各维护一张 TLB（`f[mmu_idx]`）；真实 CPU 用 ASID 和 G 位来区分。
4. **只有 4 KiB 一种粒度。** 大页被拆成 4 KiB 的表项（实验二步骤 3）。
5. **容量会变。** 表的大小会随使用率在 64 项（`CPU_TLB_DYN_MIN_BITS 6`）和更多项之间调整，本次看到的 64 项就是下限；真实 TLB 的容量是固定的。
6. **刷新时机不同。** 见下面“有趣的细节”第 1 条。

---

# 第六部分 调试中遇到的有趣细节（对应调试要求 5，可选用）

1. **写 satp 不会清空 QEMU 的 TLB。**
   * 现象：实验五的输出中，`sfence.vma` 之前没有任何一行访问 `0x80200038` 附近的地址。
   * 背景：`entry.S` 中 `csrw satp` 是一条 CSR 写指令，QEMU 遇到它会结束当前 TB，所以下一个 TB 从 `0x80200038`（物理地址）开始。这时 satp 已经是 Sv39 了，而 `boot_page_table_sv39` 只映射了 `0xffffffffc0000000` 开始的 1 GiB，`0x80200038` 没有映射，按理说取指应该缺页。
   * 原因：`target/riscv/csr.c:729 write_satp` 只在 **ASID 改变**时才清空 TLB。ASID 没变，开启分页之前留下的那一项（`0x80200000 → 0x80200000`）还在，取指直接命中了。
   * 验证：`break get_page_addr_code if addr == 0x80200038`，命中后 `next` 三次，再执行 `p tlb_hit(entry->addr_code, addr)` 得到 `true`，`p/x entry->addr_code` 为 `0x80200000`，而此时 `p/x env->satp` 已经是 `0x8000000000080205`。
   * 接着 `sfence.vma` 清空了 TLB。但它不结束当前 TB，后面的 `lui sp`、`lui t0`、`addi`、`jr t0` 已经翻译好了，照样执行，直到 `jr t0` 跳到高地址。
   * 按 RISC-V 特权规范，写 satp 之后、执行 `sfence.vma` 之前，硬件可以继续使用旧的翻译，但不保证一定如此，所以这段代码在真实硬件上能否运行取决于具体实现；在 QEMU 中，它是靠“旧 TLB 项 + TB 已翻译好”这一巧合跑通的。
2. **gdbstub 也会调用翻译函数。** 在终端 3 执行 `x/i $pc`、`b *地址` 时，QEMU 要把 ucore 的虚拟地址翻译成物理地址才能读内存。它走的是另一条路：`target_memory_rw_debug`（gdbstub.c）→ `cpu_memory_rw_debug` → `riscv_cpu_get_phys_page_debug` → `get_physical_address`。所以在 `get_physical_address` 上打不带条件的断点时，有一部分停下其实是 gdb 自己在读内存，看 `bt` 就能区分。这条路径不经过 TLB，也不写 TLB。
3. **`riscv_cpu_tlb_fill` 中看到的 `env->pc` 不一定准。** 源码注释也写了这一点：pc 只在 TB 的边界才更新。实测：不设 ucore 断点时，`sd ra,8(sp)` 触发的 `tlb_fill` 中 `env->pc` 是 `0xffffffffc02000d6`，即这条指令所在 TB 的起点；在终端 3 对这条指令设了断点时（实验三），TB 会在断点处切开，`env->pc` 才恰好是 `0xffffffffc02000ec`。要知道准确的指令地址，应该看终端 3。
4. **读页表项本身不经过 TLB。** `get_physical_address` 用 `ldq_phys` 按物理地址读页表项，所以不会出现“查页表时又要查页表”的递归。
5. **所有表项的 `addend` 都一样。** 实验四步骤 3 中打印的十几个表项，`addend` 都是同一个值。因为 lab2 的内核页表是线性映射（虚拟地址 − 物理地址为常数），而模拟的内存在宿主机上又是一整块连续的内存，所以“宿主机指针 − 客户机虚拟地址”对每一页都相同。
6. 不加条件地在 `get_physical_address` 上打断点，在 OpenSBI 阶段就会停几千次，所以条件断点是必需的。

---

# 第七部分 报告要求与各实验的对应

| 指导书调试要求 | 负责人 | 对应实验 | 建议截图 |
| :--- | :--- | :--- | :--- |
| 1. 关键调用路径与分支，演示一次虚拟地址到物理地址的翻译 | openfar | 实验一、三 | 实验一的 `bt` 和 `tlb_set_page_with_attrs` 参数；实验三三个终端同框 |
| 2. 单步调试页表翻译，解释关键流程 | openfar | 实验二 | 254 行和 334 行处的变量值 |
| 3. 查 TLB 的 C 代码及调试细节 | lyp | 实验四 | `tlb_hit` 为 false、victim 命中前后表项的对比 |
| 4. QEMU 的 TLB 与真实 TLB 的区别 | lyp | 实验五 | 打印输出（Bare 与 Sv39 对比） |
| 5. 有趣的细节 | 各自 | 第六部分 | 优先写自己发现的 |
| 6. 大模型帮助解决的问题 | 各自 | — | 如实记录 |

# 第八部分 常见问题

| 现象 | 原因与处理 |
| :--- | :--- |
| 终端 3 的 `make gdb` 一直连不上 | 终端 2 attach 之后还没执行 `continue`，QEMU 被冻结了 |
| 终端 3 执行命令后没有反应 | 终端 2 停在了断点上。去终端 2 执行 `continue` |
| `ptrace: Operation not permitted` | 原生 Linux 不允许普通用户 attach，改用 `sudo gdb` |
| `No symbol "get_physical_address" in current context` | attach 的不是带调试信息的 QEMU。用 `file $QEMU` 确认有 `debug_info`，用 `echo $QEMU` 确认终端 1 的 make 用的就是它 |
| 断点设上了却一直不停 | 地址写错了，或者这次访问在快速路径中命中了 TLB（实验四）。先用实验五的打印方法确认这个地址确实会进入 `tlb_fill` |
| `vMustReplyEmpty: timeout` | 残留的 QEMU 占用了 1234 端口，执行 `pkill -f qemu-system-riscv64` |
| 变量显示 `<optimized out>` | QEMU 编译时没有加 `--enable-debug` |
| gdb 莫名其妙地停在 `SIGPIPE` | 忘了执行 `handle SIGPIPE nostop noprint` |
