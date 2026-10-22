## 分支任务：GDB 调试 QEMU 的页表查询过程

**负责人：** 2411264-张远（调用路径与页表遍历），2414099-李云鹏（TLB）

本节以下内容由张远完成，对应调试要求 1、2，以及各自要写的 5、6 两项。调试使用带调试信息编译（`--enable-debug`）的 QEMU 4.1.1，下文的地址和输出都来自 lab2 分支最终版本的内核。

### 调试环境与流程

这次调试同时用到两个 GDB：

- **宿主机的 `gdb`** attach 到 QEMU 进程，调试的是 QEMU 的 C 源码，断点打在 `get_physical_address` 这类 QEMU 函数上；
- **`riscv64-unknown-elf-gdb`** 通过 QEMU 内置的 gdbstub（1234 端口）调试 ucore，用法与 lab1 相同。

两者是嵌套关系：宿主机 gdb 停在断点上时，整个 QEMU 进程都暂停了，ucore 一侧的 gdb 也会跟着卡住。所以操作顺序是：终端 1 执行 `make debug`；终端 2 用 `gdb -p <QEMU 的 PID>` attach，执行 `handle SIGPIPE nostop noprint`，设好断点后 `continue`；最后才在终端 3 执行 `make gdb`。如果在终端 2 执行 `continue` 之前就执行 `make gdb`，gdbstub 处于冻结状态，会一直连不上。在 WSL2 中同一个用户就可以 attach，不需要指导书中的 `sudo`。

QEMU 并不是逐条解释 RISC-V 指令。它把一小段指令（一个翻译块，TB）翻译成 x86 代码放进 `code_gen_buffer` 后直接执行，遇到难以直接生成代码的操作（例如访存时 TLB 没有命中），才从生成的代码调用 QEMU 中的 C 函数。我们的断点都打在这些 C 函数上。

### 关键调用路径与分支语句

指导书「思考」一节提示，可以选一个一定会被访问的地址，直接在 QEMU 的翻译函数上打条件断点。`entry.S` 打开 Sv39 之后，最后一条指令 `jr t0` 跳到 `kern_init`（`0xffffffffc02000d6`），所以开启分页后的第一次取指一定访问这个地址：

```gdb
(gdb) break get_physical_address if addr == 0xffffffffc02000d6
(gdb) continue          # 然后在终端 3 执行 make gdb，并在 ucore 的 gdb 中 continue
(gdb) bt
(gdb) p/x env->satp
```

![get_physical_address 断点处的调用栈](../images/T5-walk-bt.png)

<p align="center">图 T5-1　开启分页后第一次取指 0xffffffffc02000d6 时的调用栈</p>

此时终端 3 中 ucore 的 gdb 停在 `Continuing.`，因为整个 QEMU 进程都被终端 2 暂停了。gdb 用十进制显示地址，`18446744072637907158` 就是 `0xffffffffc02000d6`。`access_type=2` 表示取指，`mmu_idx=1` 表示 S 态。从下往上读这条调用栈：

| 层 | 函数 | 作用 |
| :--- | :--- | :--- |
| #12～#8 | `qemu_tcg_cpu_thread_fn` → `cpu_exec` → `cpu_tb_exec` | 模拟 CPU 的线程在主循环里逐个执行翻译块 |
| #7 | `code_gen_buffer` | 正在执行上一个翻译块生成的 x86 代码，它的最后一条指令是 `jr t0` |
| #6～#4 | `helper_lookup_tb_ptr` → `tb_htable_lookup` | `jr t0` 的目标要到运行时才知道，于是查找下一个翻译块。翻译块按**物理地址**缓存，所以要先把目标 pc 翻译成物理地址 |
| #3 | `get_page_addr_code` | 取指方向查 QEMU 的软件 TLB。刚开启分页，`0xffffffffc0200000` 这一页没有被访问过，未命中 |
| #2 | `tlb_fill` | 与体系结构无关的通用代码，通过函数指针转调 RISC-V 的填充函数 |
| #1 | `riscv_cpu_tlb_fill` | 调用翻译函数，成功就写 TLB，失败就向客户机抛缺页异常 |
| #0 | `get_physical_address` | 按 RISC-V 规范遍历页表 |

数据访存走的是另一个入口。在 ucore 一侧把断点设在 `kern_init` 中的 `sd ra,8(sp)`（`0xffffffffc02000ec`，写入地址为 `sp + 8 = 0xffffffffc0207ff8`），QEMU 一侧在 `riscv_cpu_tlb_fill` 上按这个地址设条件断点，ucore 一侧 `si` 之后得到的调用栈是：

```
#0  riscv_cpu_tlb_fill (... address=18446744072637939704, size=8, access_type=MMU_DATA_STORE, mmu_idx=1, ...) at target/riscv/cpu_helper.c:438
#1  tlb_fill (...) at accel/tcg/cputlb.c:878
#2  store_helper (big_endian=false, size=8, ..., val=2147486210, addr=18446744072637939704, ...) at accel/tcg/cputlb.c:1522
#3  helper_le_stq_mmu (...) at accel/tcg/cputlb.c:1672
#4  code_gen_buffer ()
```

这里是**翻译后的代码直接调用 `helper_le_stq_mmu`**，再进入 `store_helper`。`val=2147486210`（`0x80000a02`）正是 ucore 一侧 `p/x $ra` 看到的值。生成的代码会先用内联的 x86 指令查 TLB，只有未命中才调用这个 helper；这部分属于 TLB 的内容，见李云鹏负责的部分。

路径上决定访存走向的关键分支：

| 位置 | 分支语句 | 含义 |
| :--- | :--- | :--- |
| `cputlb.c:1030` | `if (unlikely(!tlb_hit(entry->addr_code, addr)))` | 取指时主 TLB 是否命中 |
| `cputlb.c:1031` | `if (!VICTIM_TLB_HIT(addr_code, addr))` | 主 TLB 未命中时再查 victim TLB，仍未命中才调用 `tlb_fill`（数据访存在 `store_helper`/`load_helper` 中有同样的两层判断） |
| `cpu_helper.c:165` | `if (mode == PRV_M && access_type != MMU_INST_FETCH)` | M 态的数据访存，若设置了 `mstatus.MPRV`，就按 `MPP` 指定的特权级翻译 |
| `cpu_helper.c:171` | `if (mode == PRV_M \|\| !riscv_feature(env, RISCV_FEATURE_MMU))` | M 态不翻译，物理地址等于虚拟地址（OpenSBI 的访存都走这里） |
| `cpu_helper.c:187` | `switch (vm)` | 按 `satp.MODE` 选择页表级数：Sv39 为 3 级；Bare 模式直接返回物理地址等于虚拟地址 |
| `cpu_helper.c:227` | `if (masked_msbs != 0 && masked_msbs != mask)` | Sv39 要求第 63～39 位与第 38 位相同，否则直接失败 |
| `cpu_helper.c:256` 起 | `if (!(pte & PTE_V))` 及之后的 `else if` | 判断页表项无效、非叶子、保留组合、权限不符等情况（见下一节） |
| `cpu_helper.c:471` | `if (ret == TRANSLATE_SUCCESS)` | 成功则 `tlb_set_page` 写 TLB；否则 `raise_mmu_exception` 抛缺页异常 |

### 单步调试页表翻译

`get_physical_address` 的核心是一个 `for` 循环（`cpu_helper.c:237`），每轮查一级页表。循环之前，`base` 是根页表的物理地址（来自 `satp`），Sv39 下 `levels = 3`、`ptidxbits = 9`、`ptesize = 8`，`ptshift` 初值为 `(3 - 1) × 9 = 18`。循环体的关键几行是：

```c
target_ulong idx = (addr >> (PGSHIFT + ptshift)) & ((1 << ptidxbits) - 1);  // 本级 9 位索引
target_ulong pte_addr = base + idx * ptesize;                              // 本级页表项的物理地址
target_ulong pte = ldq_phys(cs->as, pte_addr);                             // 按物理地址读出页表项
target_ulong ppn = pte >> PTE_PPN_SHIFT;                                   // 取出物理页号
```

读出页表项后，一串 `if / else if` 决定下一步：V 为 0 就失败；R、W、X 全为 0 表示非叶子，令 `base = ppn << PGSHIFT` 进入下一级；否则是叶子，依次检查保留组合、U 位与当前特权级、大页对齐、读写执行权限，全部通过后设置 A/D 位并计算物理地址。

在第 254 行（`ppn = pte >> PTE_PPN_SHIFT`，此时页表项刚读出）设断点查看各变量，再在计算物理地址的第 334 行停下，单步执行这一行，结果如图 T5-2 所示：

```gdb
(gdb) break cpu_helper.c:254
(gdb) continue
(gdb) p i
(gdb) p/x idx
(gdb) p/x pte_addr
(gdb) p/x pte
(gdb) p/x base
(gdb) break cpu_helper.c:334
(gdb) continue
(gdb) p/x ppn
(gdb) p ptshift
(gdb) p/x vpn
(gdb) next
(gdb) p/x *physical
```

![页表遍历中的变量值](../images/T5-walk-pte.png)

<p align="center">图 T5-2　第 254 行与第 334 行处的变量值</p>

第 254 行处的变量：

| 变量 | 值 | 含义 |
| :--- | :--- | :--- |
| `i` | 0 | 正在查第一级（根页表） |
| `base` | `0x80208000` | 根页表的物理地址，即 `satp` 中的 `0x80208` 左移 12 位 |
| `idx` | `0x1ff`（511） | `0xffffffffc02000d6` 的第 38～30 位 |
| `pte_addr` | `0x80208ff8` | `0x80208000 + 511 × 8`，根页表的最后一项 |
| `pte` | `0x200000cf` | 与 `entry.S` 中的 `.quad (0x80000 << 10) \| 0xcf` 一致 |

`pte` 的低 8 位 `0xcf` 依次是 D=1、A=1、G=0、U=0、X=1、W=1、R=1、V=1：页表项有效，并且 R、W、X 不全为 0，所以**第一级就是叶子**，这是一个 1 GiB 的大页，循环只执行这一轮。在第 334 行处，`ppn = 0x80000`，`ptshift = 18`，`vpn = 0xffffffffc0200`，单步之后 `*physical = 0x80200000`。

大页的页表项只给出了高位的物理页号 `0x80000`，虚拟页号中还有 18 位（30 − 12）没有被页表“用掉”，要原样搬到物理地址里：`vpn & ((1 << 18) - 1) = 0x200`，与 `ppn` 拼接得到 `0x80200`，左移 12 位就是 `0x80200000`。源码注释称之为 “make a fake leaf PTE for the TLB's benefit”：QEMU 的 TLB 每一项只管 4 KiB，所以即使命中的是 1 GiB 大页，也只把当前访问的这一个 4 KiB 页写进 TLB，与下一节 `size=4096` 一致。另外，`0xcf` 中的 A、D 位已经为 1，第 295 行起更新 A/D 位的分支没有执行写操作。

lab2 只建立了这一个大页映射，所以看不到三级页表都走一遍的情况。如果 `entry.S` 把这一项写成只有 V 位的 `0xc1`，它就会被当成非叶子，`base` 变成 `0x80000000`，QEMU 会把 OpenSBI 的代码当作第二级页表来读。

### 演示：一个虚拟地址被翻译成物理地址的结果

页表遍历结束后，删除前面的断点，用 `finish` 执行完翻译函数，再在写 TLB 的函数处停下（图 T5-3）。结合图 T5-1 末尾打印的 `satp`：

```gdb
(gdb) delete
(gdb) finish
(gdb) break tlb_set_page_with_attrs
(gdb) continue
(gdb) p/x vaddr
(gdb) p/x paddr
(gdb) p prot
(gdb) p size
```

![翻译结果写入 TLB](../images/T5-walk-tlbset.png)

<p align="center">图 T5-3　get_physical_address 的返回值与 tlb_set_page_with_attrs 的参数</p>

- `satp` 的最高 4 位为 8，表示 Sv39；低 44 位 `0x80208` 是根页表的物理页号，即 `boot_page_table_sv39` 所在的物理地址 `0x80208000`。
- 返回值 0 是 `TRANSLATE_SUCCESS`。
- 写进 TLB 的是虚拟页 `0xffffffffc0200000` → 物理页 `0x80200000`，权限 `prot=7`（可读、可写、可执行），粒度为 4096 字节。

所以虚拟地址 `0xffffffffc02000d6` 被翻译为物理地址 `0x802000d6`：虚拟页号换成物理页号，页内偏移 `0xd6` 不变。

### 调试中的有趣细节（张远）

1. **写 `satp` 并不会清空 QEMU 的 TLB。**我们在 `riscv_cpu_tlb_fill` 上设置“打印后继续”的断点，记录了 ucore 启动时每一次 S 态的 TLB 填充：

   ```
   tlb_fill addr=0x80200000 type=2 satp=0
   tlb_fill addr=0x80209000 type=1 satp=0
     -> get_physical_address addr=0x80209000 satp=0
   sfence.vma: helper_tlb_flush pc=0x80200038
   tlb_fill addr=0xffffffffc02000d6 type=2 satp=0x8000000000080208
   tlb_fill addr=0xffffffffc0207ff8 type=1 satp=0x8000000000080208
   ```

   `csrw satp` 会结束当前翻译块，下一个翻译块从物理地址 `0x80200038` 开始。这时 `satp` 已经是 Sv39，而启动页表只映射了 `0xffffffffc0000000` 开始的 1 GiB，`0x80200038` 并没有映射，取指本应缺页，可是输出中根本没有这个地址的 `tlb_fill`。查源码发现，`target/riscv/csr.c` 的 `write_satp` 只在 ASID 改变时才调用 `tlb_flush`。我们在 `get_page_addr_code` 上设条件断点验证：`addr == 0x80200038` 时 `tlb_hit(entry->addr_code, addr)` 为 `true`，命中的 `addr_code` 是 `0x80200000`，而此时 `satp` 已经是 `0x8000000000080208`。也就是说，这次取指命中了开启分页之前留下的旧表项。随后的 `sfence.vma` 才清空 TLB；但它不结束翻译块，同一块中已经翻译好的 `lui sp`、`lui t0`、`addi`、`jr t0` 照样执行，直到跳到高地址。RISC-V 规范允许硬件在执行 `sfence.vma` 之前继续使用旧的翻译，但并不保证；在 QEMU 中，这段代码是靠“旧 TLB 表项加上已经翻译好的翻译块”跑通的。
2. **未开启分页时，访存同样经过翻译函数。**上面第二、三行是 `entry.S` 开头写 `boot_hartid` 的 `sd`，此时 `satp` 为 0，它照样走了 `store_helper → tlb_fill → riscv_cpu_tlb_fill → get_physical_address`，只是在 `switch (vm)` 的 Bare 分支直接返回。
3. **gdbstub 读内存也会调用翻译函数。**在 ucore 一侧执行 `x/i $pc`、`b *地址` 时，QEMU 一侧 `riscv_cpu_get_phys_page_debug` 上的断点会被触发，调用栈为 `target_memory_rw_debug`（gdbstub.c）→ `cpu_memory_rw_debug` → `riscv_cpu_get_phys_page_debug` → `get_physical_address`；设置断点时还会经过 `cpu_breakpoint_insert` → `breakpoint_invalidate`。所以在 `get_physical_address` 上打不带条件的断点时，有一部分命中其实来自 gdb 自己读内存，看调用栈才能区分。这条路径不经过、也不写 TLB。
4. **`riscv_cpu_tlb_fill` 里看到的 `env->pc` 不一定准确。**源码注释也提到了这一点：pc 只在翻译块的边界更新。在没有 ucore 断点的情况下，`sd ra,8(sp)` 触发的 `tlb_fill` 中，`env->pc` 是这条指令所在翻译块的起点 `0xffffffffc02000d6`，而不是指令本身的地址 `0xffffffffc02000ec`。

### 借助大模型解决的问题（张远）

1. **从零搭建双重调试流程。**指导书给出了三个终端的步骤，但我们对“两个 gdb 分别调试谁、谁先谁后”并不清楚。我们让 Claude Code 先用脚本把整个流程跑通：在 WSL 中后台启动 `make debug`，用 `gdb -p` attach 并以批处理方式执行断点命令，再启动 ucore 一侧的 gdb。它在这个过程中确认了三点：在 WSL2 中 attach 不需要 `sudo`；终端 2 必须先 `continue`，否则终端 3 连不上；以及应该在哪些函数上打条件断点。之后我们再按照它整理的教程手动操作。
2. **看不懂第一版教程。**第一版教程按函数和行号罗列，对没读过 QEMU 源码、也基本没用过 gdb 的人来说很难看懂。我们直接告诉大模型这两点，它把教程重写为“背景知识（翻译块、软件 TLB、一次访存经过的函数）→ gdb 速成 → 每一步写明目的、输入、输出和解释”的结构。我们照着第二版完成了调试。
3. **理解软件 TLB。**调试时我们追问了几个问题：软件 TLB 是什么？我们的 RISC-V 系统里的 TLB 在哪里？它是否把客户机虚拟地址直接映射到宿主机虚拟地址？大模型的解释是：QEMU 用一个 C 数组模拟硬件 TLB，表项中的 `addend` 让“客户机虚拟地址 + `addend`”直接得到宿主机指针，客户机物理地址只在填表时计算一次。我们把这段解释整理进了教程。
4. **一键启动三个窗口。**每次调试都要按顺序开三个终端，很容易出错。我们让大模型写了 `dual-gdb.cmd` 和 `dual-gdb.sh`：双击后打开 Windows Terminal，ucore 一侧的窗格会等 QEMU 一侧执行 `continue` 之后再自动连接（通过检查 QEMU 进程是否处于被跟踪的停止状态来判断）。写的过程中遇到 Windows Terminal 的窗格只有在进程退出码为 0 时才会自动关闭的问题，于是退出脚本先发 SIGTERM，窗格内的脚本收到后以 0 退出。
5. **大模型自己的一次失误。**第一版教程中的地址是在 WSL 里一份过时的代码副本上测出来的：同步脚本应当在 WSL 中运行，大模型却在 Git Bash 中执行了它，同步失败却没有报错。后来在审核其他成员的提交时，它发现编译列表中缺少新加入的 `buddy_pmm.c`，才意识到这个问题。修复的方式是给同步脚本加上运行环境检查，并在新代码上把全部实验重新跑了一遍。这件事提醒我们：大模型给出的“实测结果”也要核对它是在什么代码上测的。
