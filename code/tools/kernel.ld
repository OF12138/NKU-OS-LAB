/* Simple linker script for the ucore kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

OUTPUT_ARCH(riscv)
ENTRY(kern_entry)

BASE_ADDRESS = 0xFFFFFFFFC0200000; 
/*，我们现在想将内核代码放在虚拟地址空间中以 0xffffffffc0200000 开头的一段高地址空间中
规定链接地址的用处

链接地址是指令里写死的那个值。内核开分页后，pc 和各种指针取到的都是链接器填进去的地址，所以这些地址必须是有效的虚拟地址，而且经页表翻译后要正好落在内核实际所在的物理位置。链接地址只"声明内核希望自己在哪个虚拟地址运行"，不负责把它放到那里。

物理地址 0x80200000 的映射结果是否就是这个虚拟地址

是，但这是 entry.S 的页表人为设定的，不是自动发生的

这部分由于改动了链接脚本的起始地址，所以它会认为自己处在以虚拟地址 0xffffffffc0200000 开头的一段连续虚拟地址空间中，以此为依据确定代码里每个部分的地址（每一段都是从BASE_ADDRESS往后依次摆开的，所以代码里各段都会认为自己在0xffffffffc0200000之后的某个地址上，或者说编译器和链接器会把里面的符号/变量地址都对应到0xffffffffc0200000之后的某个地址上）

*/

SECTIONS
{
    /* Load the kernel at this address: "." means the current address */
    . = BASE_ADDRESS;

    .text : {
        *(.text.kern_entry)
        *(.text .stub .text.* .gnu.linkonce.t.*)
    }

    PROVIDE(etext = .); /* Define the 'etext' symbol to this value */

    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    /* Adjust the address for the data segment to the next page */
    . = ALIGN(0x1000);

    /* The data segment */
    .data : {
        *(.data)
        *(.data.*)
    }

    .sdata : {
        *(.sdata)
        *(.sdata.*)
    }

    PROVIDE(edata = .);

    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss*)
    }

    PROVIDE(end = .); 
    /*
    可供内核继续规划的物理内存候选范围是[KernelEnd, 0x88000000)。
    这里的KernelEnd为内核代码结尾的物理地址，而kernel.ld中定义的end符号表示内核代码结尾的虚拟地址，
    二者不能直接混用。
    */

    /DISCARD/ : {
        *(.eh_frame .note.GNU-stack)
    }
}
