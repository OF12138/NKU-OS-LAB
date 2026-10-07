
# @ entry.S 是内核自己的第一段代码。它只做两件事：给 C 语言准备一块栈，然后跳到 C 函数 kern_init。

# @ C 函数运行需要栈，用来存局部变量、函数调用的返回地址等。但 CPU 刚交到内核手里时，栈指针 sp 没指向任何属于内核的合法内存。C 代码不能直接上来就跑，所以要先用汇编把栈设好。


# @ .S（大写）文件会先经过 C 预处理器，所以可以用 #include 和 #define。小写 .s 文件不行。
# @ 引入这两个头文件，是为了用里面的两个宏：
# @ PGSHIFT = 12（在 mmu.h）：页大小 4096 字节，即 2¹²。
# @ KSTACKSIZE = 2 * 4096 = 8192（在 memlayout.h）：内核栈 8KB。

#include <mmu.h>
#include <memlayout.h>


# The ,"ax",@progbits tells the assembler that the section is allocatable ("a"), executable ("x") and contains data ("@progbits").
# 从这里开始.text 这个section, "ax" 和 %progbits描述这个section的特征

# @ 以 . 开头的叫汇编器指示符（directive）。它不是 CPU 指令，是给汇编器的命令。
# @ .section .text：从这里开始，后面的内容放进名为 .text 的段（section）。
# @ "ax"：段的属性。a = allocatable，运行时要占内存；x = executable，可以执行。
# @ %progbits：这个段有实际内容，是文件里真实存在的字节。它的反面是 nobits，像 .bss 那样只占空间、不占文件体积。

.section .text,"ax",%progbits
    .globl kern_entry # to make "kern_entry" one global symbol so that the linker can find this name, 

kern_entry: # This is one label and in assembly it will be replaced by specific addr. 
    # note that this is the first thing after .text so its addr is 0x80200000
    la sp, bootstacktop # la 是 load address（加载地址） 伪指令，意思是把标签 bootstacktop 的地址放进寄存器 sp。

    # 为什么要我们自己做
    # C 语言需要栈，而编译器默认 sp 已经可用。 编译出的 C 代码一进函数就会 addi sp, sp, -N 并存取局部变量和返回地址，这些都依赖 sp 指向一块可写的合法内存。
    # 没有人替我们保证 sp 的状态。

    # @ tail 是尾调用伪指令，相当于 jal x0, kern_init，即跳过去，不保存返回地址。
    # @ 用 tail 而不是 call，是因为 kern_init 永远不会返回（init.c 里声明了 noreturn，末尾是 while(1) 死循环），保存返回地址没有意义。
    # @ 从这里开始，CPU 开始执行 C 代码。

    tail kern_init

.section .data # swtich to the data section. 
    # .align 2^12 page size is 4KB. i.e. 2^12
    .align PGSHIFT
    .global bootstack # 导出并定义标签 bootstack
bootstack:
    .space KSTACKSIZE # reserve 8192 bytes and inited with 0 by default. 
    
    .global bootstacktop
bootstacktop: # export and define bootstacktop. this is the high addr of stack. 


# entry.S ：OpenSBI启动之后将要跳转到的一段汇编代码。在这里进行内核栈的分配，然后转入C语言编写的内核初始化函数。