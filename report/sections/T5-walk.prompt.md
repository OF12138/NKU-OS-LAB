## 分支任务：用双重 GDB 观察 QEMU 的地址翻译与页表遍历

[PROMPT]

**任务**：在 lab2 的 ucore 上用双重 GDB 调试 QEMU 4.1.1（`--enable-debug` 编译），完成分支任务的调试要求 1、2：找出一次访存从 TCG 生成的代码走到 Sv39 页表遍历的调用路径和关键分支，演示一个虚拟地址如何被翻译成物理地址，并单步解释页表遍历的关键操作。结果写入 `report/sections/T5-walk.md`，截图放在 `report/images/T5-walk-*.png`。负责人为 2411264-张远（TLB 部分由李云鹏完成，不在本提示词范围内）。

**操作要求**：所有命令和输出都要实际运行得到，不能凭源码推测；地址以 lab2 分支最终提交的内核为准（用 `.claude/tools/sync-wsl.sh` 在 WSL 中同步后编译）。先用脚本自动跑通流程，确认断点和输出后再截图。截图使用浅色主题，画面里不能有误输入的字符。

**输出要求**：Markdown。依次给出调试流程、调用路径（调用栈截图加逐层说明表）、关键分支表、页表遍历的单步结果（变量表加计算过程）、翻译结果演示，以及调试中发现的有趣细节和借助大模型解决的问题。gdb 打印的十进制地址要换算成十六进制再解释。

[RELY]

- `code/kern/init/entry.S`（框架原始代码）：用一个 1 GiB 大页建立启动页表，写 `satp` 后执行 `sfence.vma`，再跳到高地址。

```asm
kern_entry:
    la t0, boot_hartid
    sd a0, 0(t0)
    la t0, boot_dtb
    sd a1, 0(t0)
    lui     t0, %hi(boot_page_table_sv39)
    li      t1, 0xffffffffc0000000 - 0x80000000
    sub     t0, t0, t1
    srli    t0, t0, 12
    li      t1, 8 << 60
    or      t0, t0, t1
    csrw    satp, t0
    sfence.vma
    lui sp, %hi(bootstacktop)
    lui t0, %hi(kern_init)
    addi t0, t0, %lo(kern_init)
    jr t0
...
boot_page_table_sv39:
    .zero 8 * 511
    .quad (0x80000 << 10) | 0xcf # VRWXAD
```

- `code/Makefile`：`make debug` 以 `-s -S` 启动 QEMU 并等待 gdb；`make gdb` 用 `riscv64-unknown-elf-gdb` 执行 `target remote localhost:1234`。
- QEMU 4.1.1 中需要关注的函数：`target/riscv/cpu_helper.c` 的 `get_physical_address`、`riscv_cpu_tlb_fill`、`riscv_cpu_get_phys_page_debug`；`accel/tcg/cputlb.c` 的 `get_page_addr_code`、`store_helper`、`tlb_fill`、`tlb_set_page_with_attrs`；`target/riscv/csr.c` 的 `write_satp`。
- 环境：WSL Ubuntu，宿主机 `gdb` 可以直接 `gdb -p <QEMU 的 PID>` attach（WSL2 下不需要 sudo）。

[GUARANTEE]

需要回答的问题：

1. 一次取指和一次数据写分别经过哪些 QEMU 函数才到达页表遍历？路径上哪些分支决定是否查页表、是否直接返回物理地址？
2. `get_physical_address` 每一级做了什么？lab2 的启动页表为什么只遍历一级？大页的物理地址怎样拼出来？
3. 翻译结果以什么粒度、什么权限写入 QEMU 的 TLB？
4. 调试中还观察到哪些与“软件模拟硬件”有关的现象？

需要交付的产出：调用栈截图与逐层说明表、关键分支表、页表遍历变量截图与变量表、TLB 写入截图，以及上述问题的结论。

[SPECIFICATION]

**Pre-Condition**：内核按最终提交编译；QEMU 4.1.1 带调试信息；`kern_init` 的地址用 `riscv64-unknown-elf-nm bin/kernel` 查得（本次为 `0xffffffffc02000d6`）。

**Post-Condition**：

- 启动顺序写清楚：终端 1 `make debug` → 终端 2 attach 并 `handle SIGPIPE nostop noprint`、设断点、`continue` → 终端 3 `make gdb` 并 `continue`；说明顺序颠倒为什么会连不上。
- **Case 1（取指路径）**：`break get_physical_address if addr == <kern_init 的地址>`，`bt` 得到从 `qemu_tcg_cpu_thread_fn` 经 `code_gen_buffer`、`helper_lookup_tb_ptr`、`tb_htable_lookup`、`get_page_addr_code`、`tlb_fill`、`riscv_cpu_tlb_fill` 到 `get_physical_address` 的调用栈，并解释为什么跳转目标要先翻译成物理地址（翻译块按物理地址缓存）。
- **Case 2（数据写路径）**：在 ucore 一侧停在 `kern_init` 的 `sd ra,8(sp)`，QEMU 一侧在 `riscv_cpu_tlb_fill` 上按写入地址设条件断点，`si` 后得到 `helper_le_stq_mmu → store_helper → tlb_fill` 的调用栈，并核对 `val` 与 ucore 一侧的 `$ra`。
- **Case 3（单步页表遍历）**：在读出页表项之后（第 254 行）打印 `i`、`idx`、`pte_addr`、`pte`、`base`，在计算物理地址处（第 334 行）打印 `ppn`、`ptshift`、`vpn` 并单步得到 `*physical`；逐位解释 `0xcf`，说明第一级就是叶子，以及大页时低 18 位虚拟页号如何拼进物理地址。
- **Case 4（翻译结果）**：`finish` 得到返回值 0（`TRANSLATE_SUCCESS`），在 `tlb_set_page_with_attrs` 处打印 `vaddr`、`paddr`、`prot`、`size`，说明 QEMU 只把 4 KiB 写进 TLB；结合 `satp` 解释根页表的物理地址。
- **Case 5（有趣细节）**：用“打印后继续”的断点记录启动阶段所有 S 态的 `tlb_fill`，检查 `csrw satp` 之后、`sfence.vma` 之前的取指为什么没有缺页，用源码（`write_satp`）和条件断点（`tlb_hit`）验证；另外核对 gdbstub 读内存的翻译路径，以及 `env->pc` 是否准确。

**Requirements**：关键分支表要给出源码行号和分支语句原文。截图里的数值要与正文一致；地址变化（例如代码修改导致页表地址改变）时重新实测，不要沿用旧数据。
