## 分支任务：调试 QEMU 4.1.1 的软件 TLB

[PROMPT]

**任务**：用双重 GDB 完成 lab2 分支任务中的 TLB 分析，负责人为 2414099-李云鹏。找到软件 TLB 查找的 C 代码，实际观察主表缺失、填充、快速路径命中及 victim TLB 交换；对比 Bare 与 Sv39 下的访存路径，解释软件 TLB 与真实 CPU 地址翻译缓存的区别。

**操作要求**：基于当前 lab2 内核，在独立的 WSL 编译目录中使用带调试信息的 QEMU 4.1.1。保持内核和评分程序不变。先用 `nm`、`objdump` 确定本次二进制中的地址，再设置条件断点；每个结论都要区分源码分析与实际观察。QEMU、宿主机 GDB、交叉 GDB 分别运行在真实 tmux 窗口中，逐条执行调试命令，通过 `capture-pane` 捕获原始交互界面，再复用现有浅色渲染工具生成图片。

**输出要求**：技术分析写入 `report/sections/T5-tlb.md`，图片放在 `report/images/T5-tlb-*.png`，保留可复现的 GDB 命令和原始日志。解释每一步为什么执行、输出是什么，以及它能支持什么结论。

[RELY]

框架 `code/kern/init/entry.S` 中与本实验有关的原始指令如下；符号地址以本次编译结果为准：

```asm
kern_entry:
    la t0, boot_hartid
    sd a0, 0(t0)
    la t0, boot_dtb
    sd a1, 0(t0)
```

启动代码随后设置 `satp`、执行 `sfence.vma`，建立高地址的 1 GiB 映射并跳转到 `kern_init`。`make debug` 启用 QEMU 的 gdbstub；宿主机 GDB 用于调试 QEMU 本身，交叉 GDB 用于连接 gdbstub。

QEMU 4.1.1 的原始查找接口（省略注释）：

```c
static inline uintptr_t tlb_index(CPUArchState *env, uintptr_t mmu_idx,
                                  target_ulong addr)
{
    uintptr_t size_mask = env_tlb(env)->f[mmu_idx].mask >> CPU_TLB_ENTRY_BITS;

    return (addr >> TARGET_PAGE_BITS) & size_mask;
}

static inline bool tlb_hit_page(target_ulong tlb_addr, target_ulong addr)
{
    return addr == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK));
}

static inline bool tlb_hit(target_ulong tlb_addr, target_ulong addr)
{
    return tlb_hit_page(tlb_addr, addr & TARGET_PAGE_MASK);
}
```

- 上述函数分别位于 `include/exec/cpu_ldst.h`、`include/exec/cpu-all.h`。
- `include/exec/cpu-defs.h`：`CPUTLBEntry` 含 `addr_read`、`addr_write`、`addr_code` 和 `addend`，victim 表大小 `CPU_VTLB_SIZE` 为 8。
- `accel/tcg/cputlb.c`：`store_helper`、`load_helper`、`get_page_addr_code`、`victim_tlb_hit`、`tlb_set_page_with_attrs`。
- `tcg/i386/tcg-target.inc.c`：`tcg_out_tlb_load` 生成 x86 宿主机上的快速查表代码。
- `target/riscv/cpu_helper.c`：`get_physical_address` 与 `riscv_cpu_tlb_fill`，负责 Bare/Sv39 翻译和填充。

[GUARANTEE]

必须回答：

1. 用什么地址和访问类型选择主表项？命中判断如何处理页内偏移、无效位和特殊标志？
2. 主表与 victim 均缺失后，哪条调用路径查页表并填充 TLB？普通 RAM 的 `addend` 如何连接客户机虚拟地址与宿主机指针？
3. 主表快速命中是否调用 C helper？如何用实际生成的指令证明？
4. victim 命中后具体交换了什么？是否还需要重新遍历页表？
5. Bare 与 Sv39 下分别有哪些共同路径和不同分支？真实硬件的架构语义与 QEMU 的软件实现如何区分？
6. 调试中遇到了哪些实际问题，如何通过定位源码和调整观测方法解决？

[SPECIFICATION]

**Pre-Condition**：内核编译通过；QEMU 版本为 4.1.1 且带调试符号；记录源码提交、交叉编译器版本和关键符号地址。宿主机 GDB 附加后先继续运行，交叉 GDB 再连接；断点暂停期间要允许 gdbstub 等待。

**Post-Condition**：报告、图片、日志中的数值一致，每个实验可以从保留的命令重做；不把旧二进制的地址或另一次运行的宿主机指针当作本次结果。

**Case 1（主表缺失与填充）**：选 `kern_init` 保存返回地址的一次 `sd`，在 `store_helper` 的命中判断处打印地址、索引、mask、写标签与 `tlb_hit`；跟踪 `tlb_set_page_with_attrs` 的虚实页地址、权限和粒度；回到实际写入处核对 `haddr = addr + addend` 及内存中的值。

**Case 2（快速命中）**：利用 `entry.S` 先后写入同一页的 `boot_hartid` 和 `boot_dtb`。在第一次写的 helper 中取得返回地址，反汇编后定位第二次写的标签比较；单步检查比较结果、条件跳转、加 `addend`、宿主机写入，证明该次访问没有进入慢路径。

**Case 3（victim 命中）**：观察设备树页与内核代码页的主表索引冲突。跳过设备树第一次访问，在后一次 `victim_tlb_hit` 处检查主表和 victim 内容，记录命中的 `vidx`、返回值及交换后的两项；解释 IOTLB 为什么需要同步交换。

**Case 4（Bare/Sv39 对比）**：两次都选择 S 态的数据写，排除取指/特权级变化造成的干扰。记录 `satp`、调用栈、翻译结果及填充参数；Bare 应直接得到同值客户机物理地址，Sv39 应走页表并保留页内偏移。说明两者仍需要客户机地址到宿主机 RAM 的映射。

**Requirements**：容量必须从运行时 mask 计算，不能把 64 项当作固定常量；`addend` 直接访存结论限定于普通 RAM；不把 TCG 代码生成函数当成每次访存都会调用的函数，不把慢路径与 TLB 缺失画等号；不把 QEMU 4.1.1 的刷新策略推广为所有真实 CPU 的实现。
