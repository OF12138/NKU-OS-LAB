/* Simple linker script for the ucore kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. 
   ucore的链接脚本(link script), 告诉链接器如何将目标文件的section组合为可执行文件，并指定内核加载地址为0x80200000。
   */

OUTPUT_ARCH(riscv)
ENTRY(kern_entry) /* 规定了程序入口*/
/*
ENTRY(kern_entry)：规定程序从哪条指令开始执行。

链接器把 kern_entry 的地址写进 ELF 头的 e_entry 字段。
它决定的是"从哪开始执行"，不决定"代码放在哪"。代码放在哪由下面的 BASE_ADDRESS 和段的排列决定。
本项目里它是真正起作用的：Makefile 用 -kernel $(kernel) 加载 ELF，QEMU 读出 e_entry，传给 OpenSBI（fw_dynamic）作为 Next Address。Makefile:167-171 的注释说过，换成原框架的 -device loader 就拿不到入口，OpenSBI 会显示 Next Address = 0x0。GDB 也靠这个字段知道入口。
*/

BASE_ADDRESS = 0x80200000; /*定地址：从 0x80200000 开始，依次排布 .text → .rodata → (按页对齐) .data → .sdata → .bss*/


/*
BASE_ADDRESS = 0x80200000 加上 . = BASE_ADDRESS;：规定内核从哪个地址开始排。

BASE_ADDRESS 只是脚本里的一个变量。真正起作用的是 . = BASE_ADDRESS;：. 是"位置计数器"，后面的段从这里依次往后排。
它决定了所有符号的链接地址。lab1 还没开分页，链接地址 = 运行地址 = 物理地址，QEMU 也按 ELF 段头把内核放到这个地址。
为什么是 0x80200000：QEMU virt 机器的 DRAM 从 0x80000000 开始，前面一段给 OpenSBI 用，内核从 2MB 处开始，不会和固件重叠。这也是 OpenSBI 约定的默认内核地址。
如果改错了：内核会被放到别的位置（可能压在 OpenSBI 上），或者 edata、bootstacktop 这些绝对地址和实际位置对不上。
*/

SECTIONS
{
    /* Load the kernel at this address: "." means the current address */
    . = BASE_ADDRESS;

    /* 代码段。entry.o 在链接顺序中排第一，所以 kern_entry 正好在 0x80200000 */

    .text : {
        *(.text.kern_entry .text .stub .text.* .gnu.linkonce.t.*)
    }
        /* 意思是：从.的当前值（当前地址）开始放置一个叫做text的section. 
     花括号内部的*(.text.kern_entry .text .stub .text.* .gnu.linkonce.t.*)是正则表达式
     如果输入文件中有一个section的名称符合花括号内部的格式
     那么这个section就被加到输出文件的text这个section里
     输入文件中section的名称,有些是编译器自动生成的,有些是我们自己定义的*/

    PROVIDE(etext = .); /* Define the 'etext' symbol to this value */
    /*this is the end of .text section. */

    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    } /* 只读数据：字符串常量、const 全局变量等 */

    /* Adjust the address for the data segment to the next page */
    . = ALIGN(0x1000);
    /* 把位置计数器上调到 4KB 边界，数据段从新的一页开始，
                               方便以后开分页时给代码页和数据页设不同的权限 */
    /* 进行地址对齐，将 "."增加到 2的0x1000次方的整数倍，也就是下一个内存页的起始处 */

    /* The data segment */
     /* 已初始化的可写数据。entry.S 的 bootstack（8KB 启动栈）就在这里 */
    .data : {
        *(.data)
        *(.data.*)
    }

    .sdata : {
        *(.sdata)
        *(.sdata.*)
    }  /* small data：编译器把较小的全局变量放这里，可以通过 gp 寄存器快速访问 */

    PROVIDE(edata = .); /* 已初始化数据的结束地址 aka .bss 的开头 */

    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss*)
    }

    PROVIDE(end = .); /* 内核映像的结束地址。后面的 lab 用它确定空闲物理内存从哪里开始 */

    /DISCARD/ : {
        *(.eh_frame .note.GNU-stack)
    }
    /* 直接丢弃的段：.eh_frame 是 C++ 异常和栈回溯用的展开信息，
       .note.GNU-stack 是"栈不可执行"的标记，裸机内核都用不上 */
}
