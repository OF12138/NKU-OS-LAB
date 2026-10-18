#include <mmu.h>
#include <memlayout.h>


# CPU 状态：处于 S Mode ，寄存器 satp 的 MODE被设置为 Bare ，
# 即无论取指还是访存我们都通过物理地址直接访问物理内存。 
# PC=0x80200000 指向内核的第一条指令。栈顶地址 SP 处在 OpenSBI 代码内。

# 我们假定内核大小不超过 1GiB，
# 通过一个大页将虚拟地址区间[0xffffffffc0000000,0xffffffffffffffff] 
# 映射到物理地址区间 [0x80000000,0xc0000000)，
# 而我们只需要分配一页内存用来存放三级页表，并将其最后一个页表项(也就是对应我们使用的虚拟地址区间的页表项)进行适当设置即可。

    .section .text,"ax",%progbits
    .globl kern_entry
kern_entry:
    # a0: hartid
    # a1: dtb physical address

    # 操作系统怎样知道物理内存所在的那段物理地址呢？在 RISC-V 中，这个一般是由 bootloader ，即 OpenSBI 来完成的。它来完成对于包括物理内存在内的各外设的扫描，将扫描结果以 DTB(Device Tree Blob) 的格式保存在物理内存中的某个地方。随后 OpenSBI 会将其地址保存在 a1 寄存器中，给我们使用。

    # 这个扫描结果描述了所有外设的信息，当中也包括 Qemu 模拟的 RISC-V 计算机中的物理内存


    # save hartid and dtb address
    la t0, boot_hartid
    sd a0, 0(t0)
    la t0, boot_dtb
    sd a1, 0(t0)

    # t0 := 三级页表的虚拟地址
    lui     t0, %hi(boot_page_table_sv39) #这个地址就是本节代码后面自己定义的，编译链接之后，这个占位符被替换。
    # t1 := 0xffffffff40000000 即虚实映射偏移量
    li      t1, 0xffffffffc0000000 - 0x80000000
    # t0 减去虚实映射偏移量 0xffffffff40000000，变为三级页表的物理地址
    sub     t0, t0, t1
    # t0 >>= 12，变为三级页表的物理页号
    srli    t0, t0, 12

    # t1 := 8 << 60，设置 satp 的 MODE 字段为 Sv39
    li      t1, 8 << 60
    # 将刚才计算出的预设三级页表物理页号附加到 satp 中
    or      t0, t0, t1
    # 将算出的 t0(即新的MODE|页表基址物理页号) 覆盖到 satp 中
    csrw    satp, t0
    # 使用 sfence.vma 指令刷新 TLB
    sfence.vma
    # 从此，我们给内核搭建出了一个完美的虚拟内存空间！
    #nop # 可能映射的位置有些bug。。插入一个nop
    
    # 我们在虚拟内存空间中：随意将 sp 设置为虚拟地址！
    lui sp, %hi(bootstacktop)

    # 我们在虚拟内存空间中：随意跳转到虚拟地址！
    # 跳转到 kern_init
    lui t0, %hi(kern_init)
    addi t0, t0, %lo(kern_init)
    jr t0

.section .data
    # .align 2^12
    # 按 2^PGSHIFT = 4096 字节对齐，栈从页边界开始
    .align PGSHIFT
    .global bootstack #物理地址 0x80203000 aka 栈增长的下限。
bootstack:
    # 预留 KSTACKSIZE = 2 页 = 8KiB 作为内核启动栈（定义见 memlayout.h）
    .space KSTACKSIZE #预留是从低地址预留到高地址的。
    # .global 让 C 代码/链接器可见
    .global bootstacktop # 物理地址 0x80205000 ，由sp一开始指向，压栈时sp减小，栈向低地址生长。
bootstacktop:

# ===================================================================
# 启动页表：整个页表在编译期就静态写好在 .data 里，运行时不需要任何
# 代码去"初始化"它。kern_entry 只做一件事：把它的物理页号填进 satp 并
# 开启 Sv39，CPU 的页表遍历硬件就会开始使用它。
#
# Sv39 有三级页表（根表 -> 二级 -> 三级），虚拟地址 39 位被切成
#   VPN[2](9位) | VPN[1](9位) | VPN[0](9位) | 页内偏移(12位)
# 每张表 512 项、每项 8 字节，共 4KiB = 正好一页，所以必须页对齐
# （satp 里只存页表所在的物理页号，低 12 位恒为 0）。
#
# 这里只用到根表（一级）的一项，而且这一项直接做叶子：根表项里 R/W/X 非 0
# 时，它不再指向下一级表，而是直接映射一块 2^30 = 1GiB 的"大页"（giga page）。
# 因此整个启动页表只有一页，不需要二级、三级表。
# ===================================================================
.section .data
    # 由于我们要把这个页表放到一个页里面，因此必须 12 位对齐
    .align PGSHIFT
    .global boot_page_table_sv39
# 分配 4KiB 内存给预设的三级页表
boot_page_table_sv39:
    # 0xffffffff_c0000000 map to 0x80000000 (1G)
    # 前 511 个页表项均设置为 0 ，因此 V=0 ，意味着是空的(unmapped)
    # （0 号到 510 号项对应 VPN[2] = 0..510，即低半区和高半区的前面部分，
    #   访问它们会触发缺页异常。注意：开启分页后，CPU 取指/访存用的地址都是虚拟地址，

    .zero 8 * 511 #.zero N 在当前位置填充 N 个字节的 0，不产生指令。.zero 8 * 511 就是 4088 个 0 字节，相当于 511 个全 0 的页表项，即 V=0 的无效项
    # 设置最后一个页表项，PPN=0x80000，标志位 VRWXAD 均为 1
    # 第 511 项对应 VPN[2] = 511，即虚拟地址 0xFFFFFFFFC0000000 ~ 0xFFFFFFFFFFFFFFFF
    # （VA 的 38:30 位全为 1；63:39 位是第 38 位的符号扩展，也全为 1）。
    # 页表项格式（RV64）：
    #   63..54 保留 | 53..10 PPN | 9..8 RSW | 7 D | 6 A | 5 G | 4 U | 3 X | 2 W | 1 R | 0 V
    #   PPN = 0x80000：物理地址 0x80000000 >> 12，放在 bit 10 起，故写成 0x80000 << 10。
    #         大页要求物理地址 1GiB 对齐（PPN 低 18 位为 0），0x80000 满足。
    #   低 8 位 0xcf = 1100_1111b：D=1 A=1 G=0 U=0 X=1 W=1 R=1 V=1
    #         V：有效；R/W/X：可读可写可执行，同时说明这是叶子项（映射 1GiB 大页）；
    #         U=0：只有 S 态内核能访问；A、D 预先置 1：硬件/模拟器不必再
    #         更新访问位、脏位，否则首次访问会触发缺页（QEMU 就是这样处理的）。
    # 效果：VA 0xFFFFFFFFC0000000 + x  ->  PA 0x80000000 + x （x < 1GiB），
    #       偏移 = 0xFFFFFFFF40000000，内核 PA 0x80200000 就对应 VA 0xFFFFFFFFC0200000。
    .quad (0x80000 << 10) | 0xcf # VRWXAD #.quad X 在当前位置放一个 8 字节（64 位）的值 X

# ===================================================================
# 两个全局变量：kern_entry 开头把 OpenSBI 传来的 a0(hartid)、a1(设备树物理
# 地址) 存到这里，供 kern/driver/dtb.c 的 dtb_init 读取。放在 .data 里
# 而不是 .bss，是因为 kern_init 一开头会 memset 清零 .bss，放 .bss 的话刚存进去的值会被清掉。
# 注意 boot_dtb 存的是物理地址，C 代码要加 PHYSICAL_MEMORY_OFFSET 才能访问。
# ===================================================================
    .global boot_hartid
boot_hartid:
    .quad 0
    .global boot_dtb
boot_dtb:
    .quad 0
