# 双重 GDB 调试 QEMU 地址翻译：分步教程

对应指导书 lab2「分支任务：gdb 调试页表查询过程」。下文的命令和输出都是在统一环境（WSL2 Ubuntu 24.04、QEMU 4.1.1 `--enable-debug`、lab2 分支提交 c9318e8 的代码）中实际跑出来的。地址和行号会随代码变化，做之前先按第 1 步核对一遍。

---

## 0. 先弄清楚在调试什么

```
 终端 3：riscv64-unknown-elf-gdb ──TCP:1234──┐
                                              ▼
 终端 1：qemu-system-riscv64 进程（x86-64 程序，内部模拟一颗 RISC-V CPU，运行 ucore）
                                              ▲
 终端 2：宿主机 gdb ──ptrace attach───────────┘
```

* **终端 2 的 gdb**（x86 的普通 `gdb`）调试的是 QEMU 这个 C 程序本身，断点打在 QEMU 的函数上，比如 `get_physical_address`。
* **终端 3 的 gdb**（`riscv64-unknown-elf-gdb`）通过 QEMU 内置的 gdbstub 调试 ucore，断点打在 ucore 的地址上，比如 `0xffffffffc02000ec`。
* 两者是嵌套关系：终端 2 一旦停在断点上，整个 QEMU 进程都会暂停，终端 3 也就跟着卡住，直到终端 2 执行 `continue`。

QEMU 中与地址翻译相关的代码只有两个文件：

| 文件 | 函数 | 作用 |
| :--- | :--- | :--- |
| `accel/tcg/cputlb.c` | `load_helper` / `store_helper`（1248 / 1501 行） | 访存的慢速路径：软件 TLB 没命中时才会进来 |
| | `get_page_addr_code`（1023） | 取指时查 TLB（翻译新的 TB 之前调用） |
| | `victim_tlb_hit`（977） | 查 8 项的 victim TLB |
| | `tlb_fill`（868） | TLB 缺失，调用 CPU 相关的填充函数 |
| | `tlb_set_page_with_attrs`（697） | 把翻译结果写进软件 TLB |
| `target/riscv/cpu_helper.c` | `riscv_cpu_tlb_fill`（435） | RISC-V 的 TLB 填充：翻译、PMP 检查、写 TLB 或抛缺页异常 |
| | `get_physical_address`（155） | **页表遍历**（Sv39 三级查表） |
| | `riscv_cpu_get_phys_page_debug`（382） | gdbstub 读写客户机内存时用的翻译（不经过 TLB） |
| `include/exec/cpu_ldst.h` | `tlb_index` / `tlb_entry`（163 / 177） | 由虚拟页号算出 TLB 下标 |
| `tcg/i386/tcg-target.inc.c` | `tcg_out_tlb_load`（1699） | 生成内联的 x86 指令，在**翻译后的代码里**直接比较 TLB（快速路径） |

---

## 1. 准备

### 1.1 确认调试版 QEMU 和宿主机 gdb

```bash
export PATH=$HOME/Environment/riscv/riscv-elf-toolchains/bin:$PATH
export QEMU=$HOME/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64
$QEMU --version                 # QEMU emulator version 4.1.1
file $QEMU                      # 应包含 "with debug_info, not stripped"
gdb --version | head -1         # 宿主机 gdb；没有就 sudo apt install gdb
```

还没装 QEMU 4.1.1 的，按 `temp/env.md` 安装（脚本已经带了 `--enable-debug`，不用另编一份）。Makefile 里是 `ifndef QEMU`，只要导出了 `QEMU` 环境变量，`make debug` 就会用它，**不用修改 Makefile**。

### 1.2 准备干净的代码副本

```bash
# 在仓库根目录（Git Bash / WSL 均可）
bash .claude/tools/sync-wsl.sh          # 已提交的 code/ → ~/Code/NKU-OS-LAB/lab2/code
cd ~/Code/NKU-OS-LAB/lab2/code && make
```

### 1.3 记下要观察的地址

```bash
riscv64-unknown-elf-nm bin/kernel | grep -E " (kern_entry|kern_init|bootstacktop|boot_page_table_sv39)$"
riscv64-unknown-elf-objdump -d bin/kernel --start-address=0xffffffffc02000d6 | head -20
```

本次实测得到的地址：

```
ffffffffc0200000 T kern_entry
ffffffffc02000d6 T kern_init
ffffffffc0205000 D boot_page_table_sv39      # 物理地址 0x80205000
ffffffffc0205000 D bootstacktop

ffffffffc02000e6:  1141       addi  sp,sp,-16       # sp = 0xffffffffc0204ff0
ffffffffc02000ec:  e406       sd    ra,8(sp)        # 写 0xffffffffc0204ff8 ← 后面用这条指令演示数据访存
```

后文用到两个地址：**`kern_init` = `0xffffffffc02000d6`**（开启分页后第一次取指的地址），以及 **`0xffffffffc0204ff8`**（`sd ra,8(sp)` 写入的地址）。你的地址如果不同，后面的命令照着替换即可。

### 1.4 开三个终端

都进入 `~/Code/NKU-OS-LAB/lab2/code`，并在三个终端里都执行 1.1 中的两条 `export`（非交互的 shell 不会读 `.bashrc`）。可以用 tmux 分三栏，也可以开三个 Windows Terminal 标签页。

---

## 2. 启动顺序（最容易乱的地方）

| 顺序 | 终端 | 操作 | 此时的状态 |
| :---: | :---: | :--- | :--- |
| ① | 1 | `make debug` | QEMU 启动，因为有 `-S` 参数，CPU 停在复位地址 0x1000，等待 gdb 连接 |
| ② | 2 | `pidof qemu-system-riscv64`，然后 `gdb -q -p <PID>` | 宿主机 gdb attach 到 QEMU，**整个 QEMU 进程被冻结**（gdbstub 也一样） |
| ③ | 2 | `handle SIGPIPE nostop noprint`，设置断点，最后 `continue` | QEMU 进程恢复运行，但模拟的 CPU 仍被 `-S` 挡着 |
| ④ | 3 | `make gdb`，然后（可选）设置 ucore 断点，`continue` | 模拟的 CPU 开始执行：OpenSBI → ucore |
| ⑤ | 2 | 断点命中，开始查看和单步 | 终端 3 卡住不动，这是正常现象 |
| ⑥ | 2 | `continue` | 控制权回到终端 3 |

要点：

* **②③ 必须在 ④ 之前。** 终端 2 attach 之后、`continue` 之前，QEMU 的 gdbstub 也是冻结的，这时执行 `make gdb` 会一直连不上或者超时。
* **attach 是否需要 sudo**：WSL2 的内核没有启用 Yama，同一个用户直接 `gdb -p` 就能 attach（本机实测不需要 sudo）。原生 Ubuntu 默认 `/proc/sys/kernel/yama/ptrace_scope` = 1，需要 `sudo gdb`，或者临时执行 `echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope`。
* **`handle SIGPIPE nostop noprint` 的作用**：QEMU 内部会收到 SIGPIPE，比如 gdbstub 的连接断开时。默认情况下宿主机 gdb 每次都会停下来，看起来像是“莫名其妙地断住了”。
* `pidof` 可以换成 `pgrep -f riscv64-softmmu/qemu-system-riscv64`。只写 `pgrep -f qemu-system-riscv64` 有可能把 make 的子 shell 也匹配进来。
* 退出时：先在终端 2 执行 `detach` 再 `quit`，然后在终端 3 执行 `quit`，最后在终端 1 按 `Ctrl+A` 再按 `X`。如果之后 `make gdb` 报 `vMustReplyEmpty: timeout`，说明有残留的 qemu 进程，执行 `pkill -f qemu-system-riscv64` 即可。

---

## 3. 实验一：用条件断点看一次取指的完整翻译（对应要求 1）

这是指导书「思考」一节提到的简便做法：不用操作 ucore，只要选一个**一定会被访问**的虚拟地址，在 QEMU 中打一个条件断点即可。`entry.S` 最后一条指令 `jr t0` 会跳到 `kern_init`，所以开启分页后第一次取指访问的一定是 `0xffffffffc02000d6`。

**终端 2**（attach 之后）：

```gdb
(gdb) handle SIGPIPE nostop noprint
(gdb) break get_physical_address if addr == 0xffffffffc02000d6
(gdb) continue
```

**终端 3**：

```gdb
$ make gdb
(gdb) continue
```

**终端 2** 中断点命中，接着执行 `bt`：

```
Thread 2 "qemu-system-ris" hit Breakpoint 1, get_physical_address (env=..., physical=..., prot=...,
    addr=18446744072637907158, access_type=2, mmu_idx=1) at .../target/riscv/cpu_helper.c:158
(gdb) bt
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

gdb 默认用十进制显示地址，`18446744072637907158` 就是 `0xffffffffc02000d6`，用 `p/x addr` 可以看到十六进制。继续查看当时的状态：

```gdb
(gdb) p/x env->satp
$1 = 0x8000000000080205        # MODE=8（Sv39），PPN=0x80205 → 根页表在物理地址 0x80205000
(gdb) p mmu_idx
$2 = 1                         # 1 = S 态（PRV_S），3 = M 态
(gdb) p access_type
$3 = 2                         # 0 = 读，1 = 写，2 = 取指（MMU_INST_FETCH）
```

**怎么读这条调用栈**（从下往上看）：

1. `qemu_tcg_cpu_thread_fn` → `cpu_exec`：模拟 CPU 的主循环。QEMU 不是一条一条地解释指令，而是把一段客户机代码（一个 TB，translation block）翻译成宿主机 x86 代码，放进 `code_gen_buffer` 后直接执行。
2. 上一个 TB 以 `jr t0` 结尾，跳转目标要等到运行时才知道，于是在生成的代码里调用 `helper_lookup_tb_ptr` 查找下一个 TB。
3. `tb_htable_lookup` 用 **物理地址** 作为键查找已经翻译好的 TB，所以要先把 pc 翻译成物理地址：`get_page_addr_code`。
4. `get_page_addr_code` 查软件 TLB，没有命中（`satp` 刚切换，这一页从来没有访问过），于是调用 `tlb_fill`。
5. `tlb_fill` 通过 `cc->tlb_fill` 函数指针调用与体系结构相关的 `riscv_cpu_tlb_fill`。
6. `riscv_cpu_tlb_fill` 调用 `get_physical_address` 遍历页表。

**路径上的关键分支**（建议在报告中逐条解释）：

| 位置 | 分支 | 含义 |
| :--- | :--- | :--- |
| `cputlb.c:1030` `get_page_addr_code` | `if (unlikely(!tlb_hit(entry->addr_code, addr)))` | 主 TLB 是否命中 |
| `cputlb.c:1031` | `if (!VICTIM_TLB_HIT(addr_code, addr))` | 主 TLB 没命中时再查 victim TLB；仍然没命中才调用 `tlb_fill` |
| `cpu_helper.c:165` `get_physical_address` | `if (mode == PRV_M && access_type != MMU_INST_FETCH)` + `MPRV` | M 态访存可以借用 MPP 指定的特权级 |
| `cpu_helper.c:171` | `if (mode == PRV_M \|\| !riscv_feature(env, RISCV_FEATURE_MMU))` | M 态不翻译，物理地址等于虚拟地址 |
| `cpu_helper.c:187` | `switch (vm)`：`VM_1_10_SV39` → `levels=3, ptidxbits=9, ptesize=8`；`VM_1_10_MBARE` → 直接返回 | 由 satp.MODE 决定几级页表 |
| `cpu_helper.c:227` | `if (masked_msbs != 0 && masked_msbs != mask)` | Sv39 要求第 63～39 位都等于第 38 位（规范地址），否则失败 |
| `cpu_helper.c:256` 起 | `!(pte & PTE_V)` / `!(pte & (R\|W\|X))` / 各种权限检查 | 见第 4 节 |
| `cpu_helper.c:471` `riscv_cpu_tlb_fill` | `if (ret == TRANSLATE_SUCCESS)` → `tlb_set_page`；否则 `raise_mmu_exception` | 成功就填 TLB，失败就向客户机抛缺页异常 |

不想单步的话，用 `finish` 跑完 `get_physical_address`，再在 `tlb_set_page_with_attrs` 处停下，就能看到翻译结果：

```gdb
(gdb) finish
Run till exit from #0 get_physical_address ...
Value returned is $14 = 0                  # TRANSLATE_SUCCESS
(gdb) break tlb_set_page_with_attrs
(gdb) continue
Breakpoint 4, tlb_set_page_with_attrs (cpu=..., vaddr=18446744072637906944, paddr=2149580800, attrs=...,
    prot=7, mmu_idx=1, size=4096) at accel/tcg/cputlb.c:700
(gdb) p/x vaddr
$15 = 0xffffffffc0200000
(gdb) p/x paddr
$16 = 0x80200000
```

**结论：虚拟地址 `0xffffffffc02000d6` 所在的页 `0xffffffffc0200000` 被翻译到物理页 `0x80200000`，页内偏移 `0xd6` 不变，物理地址为 `0x802000d6`；权限 `prot=7` 即 R|W|X。** 做完这一步记得 `delete` 删掉断点再 `continue`，否则后面会一直停下来。

---

## 4. 实验二：单步调试页表遍历（对应要求 2）

在实验一停在 `get_physical_address` 入口处时，不要 `finish`，改为单步执行。先设置自动显示：

```gdb
(gdb) display/x base
(gdb) display levels
(gdb) next          # 反复 next，直到进入 for 循环（237 行）
```

也可以直接在关键行下断点：

```gdb
(gdb) break cpu_helper.c:254       # target_ulong ppn = pte >> PTE_PPN_SHIFT;（刚读出 PTE）
(gdb) continue
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

**逐行解释**（循环位于 `cpu_helper.c:237～351`）：

```c
for (i = 0; i < levels; i++, ptshift -= ptidxbits) {          // levels=3，ptshift 初值 (3-1)*9=18
    target_ulong idx = (addr >> (PGSHIFT + ptshift)) &         // 第 i 级索引：
                       ((1 << ptidxbits) - 1);                 //   i=0 取 VA[38:30]，i=1 取 VA[29:21]，i=2 取 VA[20:12]
    target_ulong pte_addr = base + idx * ptesize;              // 页表基址 + 索引 × 8 = 这一项 PTE 的物理地址
    ... pmp 检查 ...
    target_ulong pte = ldq_phys(cs->as, pte_addr);             // 按物理地址从客户机内存读 8 字节，读 PTE 本身不经过 TLB
    target_ulong ppn = pte >> PTE_PPN_SHIFT;                   // PTE_PPN_SHIFT = 10，取出 PPN
    if (!(pte & PTE_V))                  return TRANSLATE_FAIL;     // 无效项 → 缺页
    else if (!(pte & (PTE_R|PTE_W|PTE_X))) base = ppn << PGSHIFT;   // 非叶子：下一级页表的基址，继续循环
    else if (...保留组合 W、W+X...)       return TRANSLATE_FAIL;
    else if (...U 位与当前特权级不符...)  return TRANSLATE_FAIL;
    else if (ppn & ((1ULL << ptshift) - 1)) return TRANSLATE_FAIL;  // 大页的 PPN 低位必须为 0（对齐）
    else if (...读/写/取指权限不符...)    return TRANSLATE_FAIL;
    else { 设置 A/D 位；用 vpn 低位补齐大页；计算 prot；return TRANSLATE_SUCCESS; }
}
```

代入本次实测的数值：

| 量 | 值 | 说明 |
| :--- | :--- | :--- |
| `base` | `0x80205000` | satp.PPN `0x80205` 左移 12 位，即 `boot_page_table_sv39` 的物理地址 |
| `idx`（i=0） | `0x1ff` = 511 | `0xffffffffc02000d6 >> 30` 的低 9 位 |
| `pte_addr` | `0x80205ff8` | `0x80205000 + 511×8`，即页表最后一项 |
| `pte` | `0x200000cf` | `entry.S` 中的 `(0x80000 << 10) \| 0xcf` |
| 低 8 位 `0xcf` | D A - - X W R V | V=1，R/W/X 都为 1 → **第一级就是叶子**，即 1 GiB 大页 |
| `ppn` | `0x80000` | |
| `ptshift` | 18 | 第 0 级的叶子，偏移部分有 18+12=30 位 |

所以这次**循环只执行了一轮**（`i=0`），没有走到第二、三级。在 334 行停下看大页是怎么处理的：

```gdb
(gdb) break cpu_helper.c:334
(gdb) continue
334	            *physical = (ppn | (vpn & ((1L << ptshift) - 1))) << PGSHIFT;
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

`vpn & ((1<<18)-1)` = `0x00200`：取出虚拟页号中属于“大页内偏移”的低 18 位，与 `ppn` 拼接，得到 4 KiB 粒度的物理页 `0x80200`。注释中写的 “make a fake leaf PTE for the TLB's benefit” 就是这个意思：**QEMU 的软件 TLB 只存 4 KiB 的页**，1 GiB 大页会被拆成许多 4 KiB 的表项，在用到时逐页填入（实验一中 `tlb_set_page` 的 `size=4096` 也说明了这一点）。

`0xcf` 中 A 和 D 已经置 1，所以 `updated_pte == pte`，跳过了 295 行开始的原子更新 A/D 位的分支。如果想看三级页表都走一遍的情况，要等后续 lab 建立了 4 KiB 页的页表；在 lab2 中只能看到这一个大页。

> 可以继续思考：如果 `entry.S` 写的是 `0xc1`（只有 V，没有 RWX），这一项就会被当成非叶子，`base = 0x80000000`，然后去读 `0x80000000` 处的“页表”，也就是 OpenSBI 的代码，结果会怎样？

---

## 5. 实验三：从 ucore 侧停在访存指令处（指导书的标准流程）

这一节演示指导书描述的完整三终端流程：先让 ucore 停在一条访存指令前，再在 QEMU 中看这条指令的翻译过程。

**终端 2**：

```gdb
(gdb) handle SIGPIPE nostop noprint
(gdb) break riscv_cpu_tlb_fill if address == 0xffffffffc0204ff8
(gdb) continue
```

**终端 3**：

```gdb
$ make gdb
(gdb) b *0xffffffffc02000ec
(gdb) continue
Breakpoint 1, 0xffffffffc02000ec in kern_init () at kern/init/init.c:30
(gdb) x/i $pc
=> 0xffffffffc02000ec <kern_init+22>:	sd	ra,8(sp)
(gdb) p/x $sp
$1 = 0xffffffffc0204ff0          # 写入地址 = sp + 8 = 0xffffffffc0204ff8
(gdb) si                         # 执行这条 sd，终端 3 卡住
```

**终端 2** 中断点命中：

```
Breakpoint 2, riscv_cpu_tlb_fill (cs=..., address=18446744072637927416, size=8,
    access_type=MMU_DATA_STORE, mmu_idx=1, probe=false, retaddr=139745316372805) at target/riscv/cpu_helper.c:438
(gdb) bt 4
#0  riscv_cpu_tlb_fill (... access_type=MMU_DATA_STORE ...) at target/riscv/cpu_helper.c:438
#1  tlb_fill (...) at accel/tcg/cputlb.c:878
#2  store_helper (big_endian=false, size=8, ..., val=2147486210, addr=18446744072637927416, ...) at accel/tcg/cputlb.c:1522
#3  helper_le_stq_mmu (...) at accel/tcg/cputlb.c:1672
#4  code_gen_buffer ()
```

与实验一对比：数据访存的入口不再是 `get_page_addr_code`，而是**从翻译后的代码（`code_gen_buffer`）直接调用 `helper_le_stq_mmu` → `store_helper`**。`val=2147486210` 即 `0x80000a02`，是此时 `ra` 的值，可以在终端 3 用 `p/x $ra` 核对。`retaddr` 不为 0，指向生成代码中的返回位置，缺页时 QEMU 靠它恢复出准确的客户机 pc。

接着看填进 TLB 的那一项（在 `return true;` 那一行停下）：

```gdb
(gdb) break cpu_helper.c:474
(gdb) continue
(gdb) p/x ((RISCVCPU*)cs)->neg.tlb.f[1].mask
$2 = 0x7e0                                   # (64-1) << 5：S 态的 TLB 当前有 64 项，每项 32 字节
(gdb) set $i = (address >> 12) & (((RISCVCPU*)cs)->neg.tlb.f[1].mask >> 5)
(gdb) p $i
$3 = 4                                       # 下标 = 虚拟页号的低 6 位：直接映射（direct-mapped）
(gdb) p/x ((RISCVCPU*)cs)->neg.tlb.f[1].table[$i]
$4 = {{{addr_read = 0xffffffffc0204000, addr_write = 0xffffffffc0204000,
        addr_code = 0xffffffffc0204000, addend = 0x7f1927e00000}, ...}}
```

`addend` 不是客户机的物理地址，而是 **宿主机虚拟地址 − 客户机虚拟地址**。之后访问这一页时，`客户机 VA + addend` 直接得到宿主机上的指针，一步就能访问到模拟的内存。这也是 QEMU 的 TLB 与真实 TLB 的一个重要区别（见第 7 节）。最后 `delete`、`continue`，回到终端 3 执行 `x/gx 0xffffffffc0204ff8`，可以看到 `0x0000000080000a02` 已经写进去了。

---

## 6. 实验四：找到查 TLB 的代码并调试（对应要求 3）

查 TLB 有**两条路径**，这是调试时最容易困惑的地方：

1. **快速路径不在 C 代码里。** TCG 翻译每条 load/store 时，都会通过 `tcg/i386/tcg-target.inc.c:1699 tcg_out_tlb_load` 生成一段内联的 x86 指令：取虚拟页号 → 与 `mask` 按位与得到下标 → 比较 `table[index].addr_read/addr_write` → 命中就加上 `addend` 直接访存。**命中时完全不会调用任何 C 函数**，所以在 C 代码里打断点是停不下来的。
2. **慢速路径** 就是 `load_helper` / `store_helper`（`cputlb.c:1248` / `1501`）。快速路径的比较失败（或者需要处理 IO、非对齐）时才会调用它们。它们会**再用 C 代码查一遍**：

```c
uintptr_t index = tlb_index(env, mmu_idx, addr);              // (addr >> 12) & (mask >> 5)
CPUTLBEntry *entry = tlb_entry(env, mmu_idx, addr);           // &f[mmu_idx].table[index]
target_ulong tlb_addr = code_read ? entry->addr_code : entry->addr_read;
...
if (!tlb_hit(tlb_addr, addr)) {                                // 1271 行：比较页号
    if (!victim_tlb_hit(env, mmu_idx, index, tlb_off,         // 1272 行：遍历 8 项 victim TLB
                        addr & TARGET_PAGE_MASK)) {
        tlb_fill(env_cpu(env), addr, size,                     // 1274 行：都没命中 → 查页表
                 access_type, mmu_idx, retaddr);
        ...
```

取指走的是另一个入口 `get_page_addr_code`（1023 行），逻辑相同：`tlb_hit` → `VICTIM_TLB_HIT` → `tlb_fill`。

**调试步骤**（以下输出均为实测）：

① 看一次 TLB 未命中的完整判断。在 `store_helper` 上打条件断点（它是 always_inline 函数，gdb 会提示 `(7 locations)`，这是正常的）：

```gdb
(gdb) break store_helper if addr == 0xffffffffc0204ff8
(gdb) continue
Breakpoint 2.6, store_helper (big_endian=false, size=8, ..., addr=18446744072637927416, ...) at accel/tcg/cputlb.c:1504
(gdb) next 7                                  # 走到 1519 行
1519	    if (!tlb_hit(tlb_addr, addr)) {
(gdb) p tlb_hit(tlb_addr, addr)               # -O0 编译，内联函数可以在 gdb 中直接调用
$4 = false
(gdb) p/x env_tlb(env)->f[1].mask             # env_tlb() 同样可以直接调用
$5 = 0x7e0
```

② 看一次 victim TLB 命中。`dtb_init` 读设备树时，设备树所在的页 `0xffffffffc2200000`（物理地址 0x82200000）与内核页 `0xffffffffc0200000` 的虚拟页号低 6 位都是 0，在直接映射的主 TLB 里抢同一个位置（下标 0）：

```gdb
(gdb) break victim_tlb_hit if mmu_idx == 1 && page == 0xffffffffc2200000
(gdb) ignore 1 1                              # 第一次访问时 victim TLB 里还没有它，跳过
(gdb) continue
Breakpoint 1, victim_tlb_hit (env=..., mmu_idx=1, index=0, elt_ofs=0, page=18446744072671461376) at accel/tcg/cputlb.c:979
(gdb) bt 2
#1  load_helper (..., code_read=false, size=4, ..., addr=18446744072671461384, ...) at accel/tcg/cputlb.c:1272
(gdb) p/x env_tlb(env)->f[1].table[index].addr_read
$2 = 0xffffffffc0200000                       # 主 TLB 的 0 号位置被内核页占着
(gdb) finish
Value returned is $3 = true                   # 在 victim TLB 中找到了
(gdb) p/x env_tlb(env)->f[1].table[0].addr_read
$4 = 0xffffffffc2200000                       # 两项已经交换：设备树页回到主 TLB，内核页进入 victim TLB
```

③ 看冲突导致的再次缺失。对同一页的访问只要命中，就在快速路径里完成，不会进入 `store_helper`。但下面这个断点仍然会再停一次：

```gdb
(gdb) break store_helper if (addr >> 12) == 0xffffffffc0204
(gdb) continue                                # 停在 addr=0xffffffffc0204fa8（更深一层的栈帧）
(gdb) p/x env_tlb(env)->f[1].table[4].addr_write
$6 = 0xffffffffc0344000                       # 4 号位置已经换成了另一页
(gdb) p/x env_tlb(env)->d[1].vtable           # 8 项 victim TLB 也已经被 0xffffffffc03xx000 这些页占满
```

原因是 `page_init` 依次访问 `pages` 数组所在的 0xffffffffc0207000～0xffffffffc0346000，其中下标为 4 的页（如 `0xffffffffc0344000`）把栈页挤出主 TLB，victim TLB 也被后来的页覆盖，所以栈页只能重新走 `tlb_fill`。这是直接映射加小容量 victim 的典型冲突缺失。

**victim TLB 的机制**：主 TLB 是直接映射的，下标为虚拟页号的低几位，两个下标相同的页会互相挤占。`tlb_set_page_with_attrs` 在覆盖一个有效表项之前，会把旧表项移进 8 项全相联的 victim TLB（`cputlb.c:782 copy_tlb_helper_locked(tv, te)`）；`victim_tlb_hit` 找到之后，把它和主 TLB 中的表项交换（`cputlb.c:996～1006`），代价比重新查页表小得多。

---

## 7. 实验五：未开启分页和开启分页的对比（对应要求 4）

把所有 S 态的 `tlb_fill` 调用打印出来，不停下，这样整个启动过程一目了然。**终端 2**：

```gdb
(gdb) handle SIGPIPE nostop noprint
(gdb) break riscv_cpu_tlb_fill if mmu_idx == 1
(gdb) commands
> silent
> printf "tlb_fill addr=%#lx type=%d satp=%#lx\n", address, access_type, ((RISCVCPU*)cs)->env.satp
> continue
> end
(gdb) break get_physical_address if addr == 0x80206000
(gdb) commands
> silent
> printf "  -> get_physical_address(bare) addr=%#lx satp=%#lx\n", addr, env->satp
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

终端 3 执行 `make gdb` → `continue`。实测输出的前几行：

```
tlb_fill addr=0x80200000 type=2 satp=0                       # kern_entry 第一次取指，S 态，satp=0（Bare）
tlb_fill addr=0x80206000 type=1 satp=0                       # sd a0,0(t0) 写 boot_hartid（物理地址）
  -> get_physical_address(bare) addr=0x80206000 satp=0
sfence.vma: helper_tlb_flush pc=0x80200038
tlb_fill addr=0xffffffffc02000d6 type=2 satp=0x8000000000080205   # jr t0 跳到 kern_init，开始走 Sv39
tlb_fill addr=0xffffffffc0204ff8 type=1 satp=0x8000000000080205   # sd ra,8(sp)
tlb_fill addr=0xffffffffc02018be type=2 satp=0x8000000000080205   # memset 的代码
tlb_fill addr=0xffffffffc0206018 type=1 satp=0x8000000000080205   # memset 清 .bss
...
```

要说明的区别：

* **未开启分页时，QEMU 仍然经过同一套软件 TLB。** `0x80206000` 这次写操作照样走了 `store_helper` → `tlb_fill` → `riscv_cpu_tlb_fill` → `get_physical_address`，只是在 `case VM_1_10_MBARE:` 直接返回 `*physical = addr`。真实 CPU 在 Bare 模式下根本不会查 TLB，而 QEMU 的 TLB 本身就是“客户机虚拟地址 → 宿主机指针”的缓存，不论是否开启分页都需要它，否则每次访存都要查一次 QEMU 的内存区域树。M 态（OpenSBI，`mmu_idx=3`）也一样，把条件改成 `mmu_idx == 3` 可以看到成百上千次调用。
* **表项内容不同**：真实 TLB 存“VPN → PPN + 权限位”；QEMU 的 `CPUTLBEntry` 为读、写、取指各存一个页号（`addr_read/addr_write/addr_code`，某种访问不允许时对应字段为 -1），外加 `addend`（直接得到宿主机指针）；访问 IO 时用到的物理地址信息另存在 `iotlb` 中。
* **按特权级分表**：`f[mmu_idx]` 为 U、S、M 各维护一张 TLB，而真实 CPU 用 ASID 和 G 位区分。
* **只有 4 KiB 粒度**：大页被拆成 4 KiB 表项（第 4 节）；真实 CPU 一般有专门存放大页的 TLB 项。
* **容量是动态的**：表的大小会随使用率在 2^6～2^k 项之间伸缩（`CPU_TLB_DYN_MIN_BITS 6`，默认 `CPU_TLB_DYN_DEFAULT_BITS 8`）。本次看到 S 态的表已经缩到下限 64 项，而真实 TLB 的容量是固定的。
* **刷新时机不同**：见第 8 节第 1 条。

---

## 8. 调试中遇到的“抓马”细节（对应要求 5，可选用）

1. **写 satp 并不会刷新 QEMU 的 TLB。** `target/riscv/csr.c:729 write_satp` 只在 **ASID 改变** 时调用 `tlb_flush`。`entry.S` 中 `csrw satp` 之后，下一个 TB 从物理 pc `0x80200038` 开始。此时 satp 已经是 Sv39，按理说 `0x80200038` 在 `boot_page_table_sv39` 中没有映射（第 2 项为 0），取指应该缺页。但从第 7 节的输出可以看到，**`0x80200038` 根本没有触发 `tlb_fill`**：它命中了开启分页之前留下的旧表项（`0x80200000` → `0x80200000`）。随后 `sfence.vma`（`helper_tlb_flush`）才清空 TLB。而 `sfence.vma` 不会结束当前 TB，同一个 TB 里的 `lui sp`、`lui t0`、`addi`、`jr t0` 早已翻译好，照样执行，直到 `jr t0` 跳到高地址。按 RISC-V 特权规范，写 satp 之后、执行 sfence.vma 之前，硬件可以继续使用旧的翻译，但并不保证一定如此，所以这段代码在真实硬件上能否运行取决于具体实现；在 QEMU 中，它是靠“旧 TLB 项 + TB 已翻译”这一巧合跑通的。验证方法：`break get_page_addr_code if addr == 0x80200038`，`next` 三次后执行 `p tlb_hit(entry->addr_code, addr)` 得到 `true`，`p/x entry->addr_code` 为 `0x80200000`，而此时 `p/x env->satp` 已经是 `0x8000000000080205`。
2. **gdbstub 也会调用翻译函数。** 在终端 3 中执行 `x/i $pc`、`b *addr` 时，终端 2 中的 `riscv_cpu_get_phys_page_debug` 断点会被触发，调用栈为 `target_memory_rw_debug`（gdbstub.c）→ `cpu_memory_rw_debug` → `riscv_cpu_get_phys_page_debug` → `get_physical_address`。所以在 `get_physical_address` 上打不带条件的断点时，有一部分停下来的原因其实是 gdb 自己在读内存，看 `bt` 就能分辨。这条路径**不经过 TLB**，也不会把结果写进 TLB。
3. **`riscv_cpu_tlb_fill` 中的 `env->pc` 不准**，源码注释中也写明了这一点。在 TB 中间访存时，pc 只在 TB 边界同步，例如第 7 节中 `sd ra,8(sp)` 那一行打印的 pc 是 TB 的起始地址。要知道准确的客户机 pc，应该看终端 3 或者 `retaddr`。
4. **页表项本身用物理地址读取**（`ldq_phys`），不经过 TLB，也不会递归翻译。
5. **断点一停，终端 3 就“假死”**，因为两个 gdb 是嵌套的。不要以为是连接断了。
6. 不加 `handle SIGPIPE nostop noprint` 时，gdbstub 连接断开等情况会让宿主机 gdb 意外停下。
7. 不加条件地在 `get_physical_address` 上打断点，在 OpenSBI 阶段（M 态）就会停几千次。条件断点是必需的。
8. **所有表项的 `addend` 都一样。** 第 6 节 ③ 中打印出来的十几个表项，`addend` 都是同一个值。这是因为 lab2 的内核页表是线性映射（VA − PA 为常数），而 QEMU 的客户机内存在宿主机上又是一整块连续的内存，所以“宿主机地址 − 客户机虚拟地址”对每一页都相同。

---

## 9. 报告要求与本教程章节的对应

| 指导书要求 | 负责人 | 用到的章节 | 需要的截图或材料 |
| :--- | :--- | :--- | :--- |
| 1. 关键调用路径、分支语句，演示一次虚拟地址到物理地址的翻译 | openfar | 3、5 | 实验一的 `bt` 和 `tlb_set_page_with_attrs` 参数；实验三三个终端的截图 |
| 2. 单步调试页表翻译，解释关键流程 | openfar | 4 | 254 行和 334 行处的变量值 |
| 3. 查 TLB 的 C 代码及调试细节 | lyp | 6 | `store_helper`/`load_helper` 中 `tlb_hit`、`victim_tlb_hit` 的调试截图 |
| 4. QEMU 的 TLB 与真实 TLB 的区别 | lyp | 7 | 第 7 节的打印输出（Bare 与 Sv39 对比） |
| 5. 有趣的细节 | 各自 | 8 | 自己的发现优先 |
| 6. 大模型解决了哪些问题 | 各自 | — | 如实记录 |

## 10. 常见问题

| 现象 | 原因与处理 |
| :--- | :--- |
| `make gdb` 一直连不上 | 终端 2 attach 后还没有 `continue`，QEMU 被冻结了 |
| `ptrace: Operation not permitted` | 原生 Linux 的 Yama 限制；`sudo gdb` 或把 `ptrace_scope` 设为 0 |
| `No symbol "get_physical_address" in current context` | attach 的不是调试版 QEMU；`file $QEMU` 确认带有 debug_info，`echo $QEMU` 确认 make 用的是它 |
| 断点设上了但永远不停 | 访问命中了 TLB 快速路径（第 6 节），或者地址写错了；先用第 7 节的打印方法确认这个地址确实会进 `tlb_fill` |
| `vMustReplyEmpty: timeout` | 残留的 qemu 占用了 1234 端口，`pkill -f qemu-system-riscv64` |
| 变量显示 `<optimized out>` | 不是 `--enable-debug` 编译的（这种构建是 `-O0`） |
